#ifndef UCI_H_INCLUDED
#define UCI_H_INCLUDED

#include <atomic>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

#include "engine.h"
#include "misc.h"
#include "search.h"

namespace Stockfish {

    class Position;
    class Move;
    class Score;
    enum Square : uint8_t;
    using Value = int;

    struct UCICommandChannel {
        enum class State : std::uint8_t {
            Idle,
            Ready,
            Processing,
            Closed
        };

        std::string        command;
        std::atomic<State> state{ State::Idle };
    };

    struct UCIAnalysisChannel {
        std::atomic<int>  depth{ 0 };
        std::atomic<int>  selDepth{ 0 };
        std::atomic<int>  multiPV{ 0 };
        std::atomic<int>  nodes{ 0 };
        std::atomic<int>  nps{ 0 };
        std::atomic<int>  hashfull{ 0 };
        std::atomic<int>  tbHits{ 0 };
        std::atomic<int>  timeMs{ 0 };
        std::atomic<int>  currMoveNumber{ 0 };

        std::atomic<bool> hasScore{ false };
        std::atomic<bool> scoreIsMate{ false };
        std::atomic<int>  scoreValue{ 0 };  // cp or mate value
        std::atomic<bool> searchFinished{ false };
        std::atomic<bool> closed{ false };

        std::mutex  textMutex;
        std::string pv;
        std::string currMove;
        std::string bestMove;
        std::string ponder;
    };

    class UCIEngine {
    public:
        UCIEngine(int argc,
            char** argv,
            UCICommandChannel* externalCommandChannel = nullptr,
            UCIAnalysisChannel* externalAnalysisChannel = nullptr);

        void loop();

        static int         to_cp(Value v, const Position& pos);
        static std::string format_score(const Score& s);
        static std::string square(Square s);
        static std::string move(Move m, bool chess960);
        static std::string wdl(Value v, const Position& pos);
        static std::string to_lower(std::string str);
        static Move        to_move(const Position& pos, std::string str);

        static Search::LimitsType parse_limits(std::istream& is);

        auto& engine_options() { return engine.get_options(); }

    private:
        Engine              engine;
        CommandLine         cli;
        UCICommandChannel* commandChannel = nullptr;
        UCIAnalysisChannel* analysisChannel = nullptr;

        void print_info_string(std::string_view str);

        void reset_analysis_state_for_new_search();
        void set_bestmove_result(std::string_view bestmove, std::string_view ponder);

        void          go(std::istringstream& is);
        void          bench(std::istream& args);
        void          benchmark(std::istream& args);
        void          position(std::istringstream& is);
        void          setoption(std::istringstream& is);
        std::uint64_t perft(const Search::LimitsType&);

        void on_update_no_moves(const Engine::InfoShort& info);
        void on_update_full(const Engine::InfoFull& info, bool showWDL);
        void on_iter(const Engine::InfoIter& info);
        void on_bestmove(std::string_view bestmove, std::string_view ponder);

        void init_search_update_listeners();
    };

}  // namespace Stockfish

#endif  // UCI_H_INCLUDED