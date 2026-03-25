/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "uci.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <optional>
#include <sstream>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "benchmark.h"
#include "engine.h"
#include "memory.h"
#include "movegen.h"
#include "position.h"
#include "score.h"
#include "search.h"
#include "types.h"
#include "ucioption.h"

namespace Stockfish {

    constexpr auto BenchmarkCommand = "speedtest";

    constexpr auto StartFEN =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    template<typename... Ts>
    struct overload : Ts... {
        using Ts::operator()...;
    };

    template<typename... Ts>
    overload(Ts...) -> overload<Ts...>;

    UCIEngine::UCIEngine(int argc,
        char** argv,
        UCICommandChannel* externalCommandChannel,
        UCIAnalysisChannel* externalAnalysisChannel) :
        engine(argv[0]),
        cli(argc, argv),
        commandChannel(externalCommandChannel),
        analysisChannel(externalAnalysisChannel) {

        engine.get_options().add_info_listener([this](const std::optional<std::string>& str) {
            if (str.has_value())
                print_info_string(*str);
            });

        init_search_update_listeners();
    }

    void UCIEngine::init_search_update_listeners() {
        engine.set_on_iter([this](const auto& i) { on_iter(i); });
        engine.set_on_update_no_moves([this](const auto& i) { on_update_no_moves(i); });
        engine.set_on_update_full(
            [this](const auto& i) { on_update_full(i, engine.get_options()["UCI_ShowWDL"]); });
        engine.set_on_bestmove([this](const auto& bm, const auto& p) { on_bestmove(bm, p); });
        engine.set_on_verify_networks([this](const auto& s) { print_info_string(s); });
    }

    void UCIEngine::print_info_string(std::string_view) {}

    void UCIEngine::reset_analysis_state_for_new_search() {
        if (!analysisChannel)
            return;

        analysisChannel->depth.store(0, std::memory_order_release);
        analysisChannel->selDepth.store(0, std::memory_order_release);
        analysisChannel->multiPV.store(0, std::memory_order_release);
        analysisChannel->nodes.store(0, std::memory_order_release);
        analysisChannel->nps.store(0, std::memory_order_release);
        analysisChannel->hashfull.store(0, std::memory_order_release);
        analysisChannel->tbHits.store(0, std::memory_order_release);
        analysisChannel->timeMs.store(0, std::memory_order_release);
        analysisChannel->currMoveNumber.store(0, std::memory_order_release);
        analysisChannel->hasScore.store(false, std::memory_order_release);
        analysisChannel->scoreIsMate.store(false, std::memory_order_release);
        analysisChannel->scoreValue.store(0, std::memory_order_release);
        analysisChannel->searchFinished.store(false, std::memory_order_release);

        std::lock_guard<std::mutex> lock(analysisChannel->textMutex);
        analysisChannel->pv.clear();
        analysisChannel->currMove.clear();
        analysisChannel->bestMove.clear();
        analysisChannel->ponder.clear();
    }

    void UCIEngine::set_bestmove_result(std::string_view bestmove, std::string_view ponder) {
        if (!analysisChannel)
            return;

        {
            std::lock_guard<std::mutex> lock(analysisChannel->textMutex);
            analysisChannel->bestMove = std::string(bestmove);
            analysisChannel->ponder = std::string(ponder);
        }

        analysisChannel->searchFinished.store(true, std::memory_order_release);
    }

    void UCIEngine::loop() {
        std::string token, cmd;

        for (int i = 1; i < cli.argc; ++i)
            cmd += std::string(cli.argv[i]) + " ";

        do
        {
            if (commandChannel)
            {
                for (;;)
                {
                    auto s = commandChannel->state.load(std::memory_order_acquire);

                    if (s == UCICommandChannel::State::Ready)
                    {
                        commandChannel->state.store(UCICommandChannel::State::Processing,
                            std::memory_order_release);

                        cmd = commandChannel->command;
                        break;
                    }

                    if (s == UCICommandChannel::State::Closed)
                    {
                        cmd = "quit";
                        break;
                    }

                    std::this_thread::yield();
                }
            }
            else if (cli.argc == 1
                && !getline(std::cin, cmd))  // Wait for an input or an end-of-file (EOF) indication
                cmd = "quit";

            std::istringstream is(cmd);

            token.clear();  // Avoid a stale if getline() returns nothing or a blank line
            is >> std::skipws >> token;

            if (token == "quit" || token == "stop")
                engine.stop();

            else if (token == "ponderhit")
                engine.set_ponderhit(false);

            else if (token == "uci")
            {
                // ignored in this integration mode
            }

            else if (token == "setoption")
                setoption(is);
            else if (token == "go")
            {
                reset_analysis_state_for_new_search();
                go(is);
            }
            else if (token == "position")
                position(is);
            else if (token == "ucinewgame")
                engine.search_clear();
            else if (token == "isready")
            {
                // ignored in this integration mode
            }

            else if (token == "flip")
                engine.flip();
            else if (token == "bench")
                bench(is);
            else if (token == BenchmarkCommand)
                benchmark(is);
            else if (token == "d")
            {
            }
            else if (token == "eval")
                engine.trace_eval();
            else if (token == "compiler")
            {
            }
            else if (token == "export_net")
            {
                std::pair<std::optional<std::string>, std::string> files[2];

                if (is >> std::skipws >> files[0].second)
                    files[0].first = files[0].second;

                if (is >> std::skipws >> files[1].second)
                    files[1].first = files[1].second;

                engine.save_network(files);
            }

            if (commandChannel)
            {
                commandChannel->state.store(
                    token == "quit" ? UCICommandChannel::State::Closed
                    : UCICommandChannel::State::Idle,
                    std::memory_order_release);
            }

        } while (token != "quit" && (commandChannel != nullptr || cli.argc == 1));

        if (analysisChannel)
            analysisChannel->closed.store(true, std::memory_order_release);
    }

    Search::LimitsType UCIEngine::parse_limits(std::istream& is) {
        Search::LimitsType limits;
        std::string        token;

        limits.startTime = now();  // The search starts as early as possible

        while (is >> token)
            if (token == "searchmoves")
                while (is >> token)
                    limits.searchmoves.push_back(to_lower(token));

            else if (token == "wtime")
                is >> limits.time[WHITE];
            else if (token == "btime")
                is >> limits.time[BLACK];
            else if (token == "winc")
                is >> limits.inc[WHITE];
            else if (token == "binc")
                is >> limits.inc[BLACK];
            else if (token == "movestogo")
                is >> limits.movestogo;
            else if (token == "depth")
                is >> limits.depth;
            else if (token == "nodes")
                is >> limits.nodes;
            else if (token == "movetime")
                is >> limits.movetime;
            else if (token == "mate")
                is >> limits.mate;
            else if (token == "perft")
                is >> limits.perft;
            else if (token == "infinite")
                limits.infinite = 1;
            else if (token == "ponder")
                limits.ponderMode = true;

        return limits;
    }

    void UCIEngine::go(std::istringstream& is) {
        Search::LimitsType limits = parse_limits(is);

        if (limits.perft)
            perft(limits);
        else
            engine.go(limits);
    }

    void UCIEngine::bench(std::istream& args) {
        std::string token;
        uint64_t    num, nodes = 0, cnt = 1;
        uint64_t    nodesSearched = 0;
        const auto& options = engine.get_options();

        engine.set_on_update_full([&](const auto& i) {
            nodesSearched = i.nodes;
            on_update_full(i, options["UCI_ShowWDL"]);
            });

        std::vector<std::string> list = Benchmark::setup_bench(engine.fen(), args);

        num = count_if(list.begin(), list.end(),
            [](const std::string& s) { return s.find("go ") == 0 || s.find("eval") == 0; });

        TimePoint elapsed = now();

        for (const auto& cmd : list)
        {
            std::istringstream is(cmd);
            is >> std::skipws >> token;

            if (token == "go" || token == "eval")
            {
                std::cerr << "\nPosition: " << cnt++ << '/' << num << " (" << engine.fen() << ")"
                    << std::endl;
                if (token == "go")
                {
                    Search::LimitsType limits = parse_limits(is);

                    if (limits.perft)
                        nodesSearched = perft(limits);
                    else
                    {
                        engine.go(limits);
                        engine.wait_for_search_finished();
                    }

                    nodes += nodesSearched;
                    nodesSearched = 0;
                }
                else
                    engine.trace_eval();
            }
            else if (token == "setoption")
                setoption(is);
            else if (token == "position")
                position(is);
            else if (token == "ucinewgame")
            {
                engine.search_clear();
                elapsed = now();
            }
        }

        elapsed = now() - elapsed + 1;

        dbg_print();

        std::cerr << "\n==========================="    //
            << "\nTotal time (ms) : " << elapsed  //
            << "\nNodes searched  : " << nodes    //
            << "\nNodes/second    : " << 1000 * nodes / elapsed << std::endl;

        engine.set_on_update_full([this, &options](const auto& i) {
            on_update_full(i, options["UCI_ShowWDL"]);
            });
    }

    void UCIEngine::benchmark(std::istream& args) {
        static constexpr int NUM_WARMUP_POSITIONS = 3;

        std::string token;
        uint64_t    nodes = 0, cnt = 1;
        uint64_t    nodesSearched = 0;

        engine.set_on_update_full([&](const Engine::InfoFull& i) { nodesSearched = i.nodes; });

        engine.set_on_iter([](const auto&) {});
        engine.set_on_update_no_moves([](const auto&) {});
        engine.set_on_bestmove([](const auto&, const auto&) {});
        engine.set_on_verify_networks([](const auto&) {});

        Benchmark::BenchmarkSetup setup = Benchmark::setup_benchmark(args);

        const auto numGoCommands = count_if(setup.commands.begin(), setup.commands.end(),
            [](const std::string& s) { return s.find("go ") == 0; });

        TimePoint totalTime = 0;

        auto ss = std::istringstream("name Threads value " + std::to_string(setup.threads));
        setoption(ss);
        ss = std::istringstream("name Hash value " + std::to_string(setup.ttSize));
        setoption(ss);
        ss = std::istringstream("name UCI_Chess960 value false");
        setoption(ss);

        for (const auto& cmd : setup.commands)
        {
            std::istringstream is(cmd);
            is >> std::skipws >> token;

            if (token == "go")
            {
                std::cerr << "\rWarmup position " << cnt++ << '/' << NUM_WARMUP_POSITIONS;

                Search::LimitsType limits = parse_limits(is);

                engine.go(limits);
                engine.wait_for_search_finished();
            }
            else if (token == "position")
                position(is);
            else if (token == "ucinewgame")
            {
                engine.search_clear();
            }

            if (cnt > NUM_WARMUP_POSITIONS)
                break;
        }

        std::cerr << "\n";

        cnt = 1;
        nodes = 0;

        int           numHashfullReadings = 0;
        constexpr int hashfullAges[] = { 0, 999 };
        constexpr int hashfullAgeCount = std::size(hashfullAges);
        int           totalHashfull[hashfullAgeCount] = { 0 };
        int           maxHashfull[hashfullAgeCount] = { 0 };

        auto updateHashfullReadings = [&]() {
            numHashfullReadings += 1;

            for (int i = 0; i < hashfullAgeCount; ++i)
            {
                const int hashfull = engine.get_hashfull(hashfullAges[i]);
                maxHashfull[i] = std::max(maxHashfull[i], hashfull);
                totalHashfull[i] += hashfull;
            }
            };

        engine.search_clear();

        for (const auto& cmd : setup.commands)
        {
            std::istringstream is(cmd);
            is >> std::skipws >> token;

            if (token == "go")
            {
                std::cerr << "\rPosition " << cnt++ << '/' << numGoCommands;

                Search::LimitsType limits = parse_limits(is);

                nodesSearched = 0;
                TimePoint elapsed = now();

                engine.go(limits);
                engine.wait_for_search_finished();

                totalTime += now() - elapsed;

                updateHashfullReadings();

                nodes += nodesSearched;
            }
            else if (token == "position")
                position(is);
            else if (token == "ucinewgame")
            {
                engine.search_clear();
            }
        }

        totalTime = std::max<TimePoint>(totalTime, 1);

        dbg_print();

        std::cerr << "\n";

        static_assert(
            std::size(hashfullAges) == 2 && hashfullAges[0] == 0 && hashfullAges[1] == 999,
            "Hardcoded for display. Would complicate the code needlessly in the current state.");

        std::string threadBinding = engine.thread_binding_information_as_string();
        if (threadBinding.empty())
            threadBinding = "none";

        std::cerr << "==========================="
            << "\nVersion                    : "
            << engine_version_info()
            << compiler_info()
            << "Large pages                : " << (has_large_pages() ? "yes" : "no")
            << "\nUser invocation            : " << BenchmarkCommand << " "
            << setup.originalInvocation << "\nFilled invocation          : " << BenchmarkCommand
            << " " << setup.filledInvocation
            << "\nAvailable processors       : " << engine.get_numa_config_as_string()
            << "\nThread count               : " << setup.threads
            << "\nThread binding             : " << threadBinding
            << "\nTT size [MiB]              : " << setup.ttSize
            << "\nHash max, avg [per mille]  : "
            << "\n    single search          : " << maxHashfull[0] << ", "
            << totalHashfull[0] / numHashfullReadings
            << "\n    single game            : " << maxHashfull[1] << ", "
            << totalHashfull[1] / numHashfullReadings
            << "\nTotal nodes searched       : " << nodes
            << "\nTotal search time [s]      : " << totalTime / 1000.0
            << "\nNodes/second               : " << 1000 * nodes / totalTime << std::endl;

        init_search_update_listeners();
    }

    void UCIEngine::setoption(std::istringstream& is) {
        engine.wait_for_search_finished();
        engine.get_options().setoption(is);
    }

    std::uint64_t UCIEngine::perft(const Search::LimitsType& limits) {
        auto nodes = engine.perft(engine.fen(), limits.perft, engine.get_options()["UCI_Chess960"]);
        return nodes;
    }

    void UCIEngine::position(std::istringstream& is) {
        std::string token, fen;

        is >> token;

        if (token == "startpos")
        {
            fen = StartFEN;
            is >> token;
        }
        else if (token == "fen")
            while (is >> token && token != "moves")
                fen += token + " ";
        else
            return;

        std::vector<std::string> moves;

        while (is >> token)
            moves.push_back(token);

        engine.set_position(fen, moves);
    }

    namespace {

        struct WinRateParams {
            double a;
            double b;
        };

        WinRateParams win_rate_params(const Position& pos) {
            int material = pos.count<PAWN>() + 3 * pos.count<KNIGHT>() + 3 * pos.count<BISHOP>()
                + 5 * pos.count<ROOK>() + 9 * pos.count<QUEEN>();

            double m = std::clamp(material, 17, 78) / 58.0;

            constexpr double as[] = { -72.32565836, 185.93832038, -144.58862193, 416.44950446 };
            constexpr double bs[] = { 83.86794042, -136.06112997, 69.98820887, 47.62901433 };

            double a = (((as[0] * m + as[1]) * m + as[2]) * m) + as[3];
            double b = (((bs[0] * m + bs[1]) * m + bs[2]) * m) + bs[3];

            return { a, b };
        }

        int win_rate_model(Value v, const Position& pos) {
            auto [a, b] = win_rate_params(pos);
            return int(0.5 + 1000 / (1 + std::exp((a - double(v)) / b)));
        }
    }

    std::string UCIEngine::format_score(const Score& s) {
        constexpr int TB_CP = 20000;
        const auto    format =
            overload{ [](Score::Mate mate) -> std::string {
                         auto m = (mate.plies > 0 ? (mate.plies + 1) : mate.plies) / 2;
                         return std::string("mate ") + std::to_string(m);
                     },
                     [](Score::Tablebase tb) -> std::string {
                         return std::string("cp ")
                              + std::to_string((tb.win ? TB_CP - tb.plies : -TB_CP - tb.plies));
                     },
                     [](Score::InternalUnits units) -> std::string {
                         return std::string("cp ") + std::to_string(units.value);
                     } };

        return s.visit(format);
    }

    int UCIEngine::to_cp(Value v, const Position& pos) {
        auto [a, b] = win_rate_params(pos);
        return int(std::round(100 * int(v) / a));
    }

    std::string UCIEngine::wdl(Value v, const Position& pos) {
        std::stringstream ss;

        int wdl_w = win_rate_model(v, pos);
        int wdl_l = win_rate_model(-v, pos);
        int wdl_d = 1000 - wdl_w - wdl_l;
        ss << wdl_w << " " << wdl_d << " " << wdl_l;

        return ss.str();
    }

    std::string UCIEngine::square(Square s) {
        return std::string{ char('a' + file_of(s)), char('1' + rank_of(s)) };
    }

    std::string UCIEngine::move(Move m, bool chess960) {
        if (m == Move::none())
            return "(none)";

        if (m == Move::null())
            return "0000";

        Square from = m.from_sq();
        Square to = m.to_sq();

        if (m.type_of() == CASTLING && !chess960)
            to = make_square(to > from ? FILE_G : FILE_C, rank_of(from));

        std::string move = square(from) + square(to);

        if (m.type_of() == PROMOTION)
            move += " pnbrqk"[m.promotion_type()];

        return move;
    }

    std::string UCIEngine::to_lower(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), [](auto c) { return std::tolower(c); });
        return str;
    }

    Move UCIEngine::to_move(const Position& pos, std::string str) {
        str = to_lower(str);

        for (const auto& m : MoveList<LEGAL>(pos))
            if (str == move(m, pos.is_chess960()))
                return m;

        return Move::none();
    }

    void UCIEngine::on_update_no_moves(const Engine::InfoShort& info) {
        if (!analysisChannel)
            return;

        analysisChannel->depth.store(info.depth, std::memory_order_release);

        const auto scoreText = format_score(info.score);
        analysisChannel->hasScore.store(true, std::memory_order_release);

        if (scoreText.rfind("mate ", 0) == 0)
        {
            analysisChannel->scoreIsMate.store(true, std::memory_order_release);
            analysisChannel->scoreValue.store(std::stoi(scoreText.substr(5)), std::memory_order_release);
        }
        else if (scoreText.rfind("cp ", 0) == 0)
        {
            analysisChannel->scoreIsMate.store(false, std::memory_order_release);
            analysisChannel->scoreValue.store(std::stoi(scoreText.substr(3)), std::memory_order_release);
        }
    }

    void UCIEngine::on_update_full(const Engine::InfoFull& info, bool) {
        if (!analysisChannel)
            return;

        analysisChannel->depth.store(info.depth, std::memory_order_release);
        analysisChannel->selDepth.store(info.selDepth, std::memory_order_release);
        analysisChannel->multiPV.store(info.multiPV, std::memory_order_release);
        analysisChannel->nodes.store(static_cast<int>(info.nodes), std::memory_order_release);
        analysisChannel->nps.store(static_cast<int>(info.nps), std::memory_order_release);
        analysisChannel->hashfull.store(info.hashfull, std::memory_order_release);
        analysisChannel->tbHits.store(static_cast<int>(info.tbHits), std::memory_order_release);
        analysisChannel->timeMs.store(static_cast<int>(info.timeMs), std::memory_order_release);

        const auto scoreText = format_score(info.score);
        analysisChannel->hasScore.store(true, std::memory_order_release);

        if (scoreText.rfind("mate ", 0) == 0)
        {
            analysisChannel->scoreIsMate.store(true, std::memory_order_release);
            analysisChannel->scoreValue.store(std::stoi(scoreText.substr(5)), std::memory_order_release);
        }
        else if (scoreText.rfind("cp ", 0) == 0)
        {
            analysisChannel->scoreIsMate.store(false, std::memory_order_release);
            analysisChannel->scoreValue.store(std::stoi(scoreText.substr(3)), std::memory_order_release);
        }

        {
            std::lock_guard<std::mutex> lock(analysisChannel->textMutex);
            analysisChannel->pv = info.pv;
        }
    }

    void UCIEngine::on_iter(const Engine::InfoIter& info) {
        if (!analysisChannel)
            return;

        analysisChannel->depth.store(info.depth, std::memory_order_release);
        analysisChannel->currMoveNumber.store(info.currmovenumber, std::memory_order_release);

        {
            std::lock_guard<std::mutex> lock(analysisChannel->textMutex);
            analysisChannel->currMove = info.currmove;
        }
    }

    void UCIEngine::on_bestmove(std::string_view bestmove, std::string_view ponder) {
        set_bestmove_result(bestmove, ponder);
    }

}  // namespace Stockfish