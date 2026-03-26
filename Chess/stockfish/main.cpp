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

#include <iostream>
#include <memory>
#include <thread>

#include "bitboard.h"
#include "misc.h"
#include "position.h"
#include "tune.h"
#include "uci.h"

using namespace Stockfish;

struct StockfishHandle {
    std::unique_ptr<UCIEngine>          engine;
    std::unique_ptr<UCICommandChannel>  in;
    std::unique_ptr<UCIAnalysisChannel> analysis;
    std::thread                         worker;
};

static void wait_input_idle_or_closed(UCICommandChannel& ch) {
    for (;;)
    {
        auto s = ch.state.load(std::memory_order_acquire);
        if (s == UCICommandChannel::State::Idle || s == UCICommandChannel::State::Closed)
            return;
        std::this_thread::yield();
    }
}

static bool send_command(UCICommandChannel& ch, std::string cmd) {
    for (;;)
    {
        auto s = ch.state.load(std::memory_order_acquire);

        if (s == UCICommandChannel::State::Closed)
            return false;

        if (s == UCICommandChannel::State::Idle)
            break;

        std::this_thread::yield();
    }

    ch.command = std::move(cmd);
    ch.state.store(UCICommandChannel::State::Ready, std::memory_order_release);
    return true;
}

static StockfishHandle launch_stockfish(int argc, char* argv[]) {
    std::cout << engine_info() << std::endl;

    Bitboards::init();
    Position::init();

    auto in = std::make_unique<UCICommandChannel>();
    auto analysis = std::make_unique<UCIAnalysisChannel>();
    auto uci = std::make_unique<UCIEngine>(argc, argv, in.get(), analysis.get());

    Tune::init(uci->engine_options());

    std::thread worker([engine = uci.get()]() {
        engine->loop();
        });

    return { std::move(uci), std::move(in), std::move(analysis), std::move(worker) };
}

int main(int argc, char* argv[]) {
    auto sf = launch_stockfish(argc, argv);

    send_command(*sf.in, "uci");
    wait_input_idle_or_closed(*sf.in);

    send_command(*sf.in, "isready");
    wait_input_idle_or_closed(*sf.in);

    send_command(*sf.in, "position startpos moves e2e4 e7e5 g1f3");
    wait_input_idle_or_closed(*sf.in);

    send_command(*sf.in, "go movetime 200");
    wait_input_idle_or_closed(*sf.in);

    while (!sf.analysis->searchFinished.load(std::memory_order_acquire))
        std::this_thread::yield();

    int depth = sf.analysis->depth.load(std::memory_order_acquire);
    bool isMate = sf.analysis->scoreIsMate.load(std::memory_order_acquire);
    int scoreValue = sf.analysis->scoreValue.load(std::memory_order_acquire);

    std::string bestMove;
    std::string ponder;
    std::string pv;

    {
        std::lock_guard<std::mutex> lock(sf.analysis->textMutex);
        bestMove = sf.analysis->bestMove;
        ponder = sf.analysis->ponder;
        pv = sf.analysis->pv;
    }

    std::cout << "Best move: " << bestMove << '\n';
    std::cout << "Ponder: " << ponder << '\n';
    std::cout << "Depth: " << depth << '\n';
    std::cout << (isMate ? "Mate: " : "CP: ") << scoreValue << '\n';
    std::cout << "PV: " << pv << '\n';

    send_command(*sf.in, "quit");
    wait_input_idle_or_closed(*sf.in);

    sf.worker.join();
    return 0;
}