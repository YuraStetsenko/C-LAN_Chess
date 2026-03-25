#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "stockfish/bitboard.h"
#include "stockfish/position.h"
#include "stockfish/tune.h"
#include "stockfish/uci.h"

namespace myChess 
{

class StockfishRunner {

private:
    std::unique_ptr<Stockfish::UCIEngine>          engine_;
    std::unique_ptr<Stockfish::UCICommandChannel>  in_;
    std::unique_ptr<Stockfish::UCIAnalysisChannel> analysis_;
    std::thread                                    worker_;
    bool                                           shutdownCalled_ = false;


    bool send_command(std::string cmd) {
        if (!in_)
            return false;

        for (;;) {
            auto s = in_->state.load(std::memory_order_acquire);

            if (s == Stockfish::UCICommandChannel::State::Closed)
                return false;

            if (s == Stockfish::UCICommandChannel::State::Idle)
                break;

            std::this_thread::yield();
        }

        in_->command = std::move(cmd);
        in_->state.store(Stockfish::UCICommandChannel::State::Ready, std::memory_order_release);
        return true;
    }

    void wait_input_idle_or_closed() const {
        if (!in_)
            return;

        for (;;) {
            auto s = in_->state.load(std::memory_order_acquire);
            if (s == Stockfish::UCICommandChannel::State::Idle
                || s == Stockfish::UCICommandChannel::State::Closed)
                return;

            std::this_thread::yield();
        }
    }

    bool send_and_wait(const std::string& cmd) {
        if (!send_command(cmd))
            return false;

        wait_input_idle_or_closed();
        return true;
    }

public:
    struct Result {
        bool        searchFinished = false;
        bool        hasScore = false;
        bool        scoreIsMate = false;
        int         scoreValue = 0;   // cp or mate
        int         depth = 0;
        int         selDepth = 0;
        int         multiPV = 0;
        int         nodes = 0;
        int         nps = 0;
        int         hashfull = 0;
        int         tbHits = 0;
        int         timeMs = 0;
        int         currMoveNumber = 0;
        std::string currMove;
        std::string bestMove;
        std::string ponder;
        std::string pv;
    };

    StockfishRunner(int argc, char* argv[]) {
        static std::once_flag initFlag;
        std::call_once(initFlag, []() {
            Stockfish::Bitboards::init();
            Stockfish::Position::init();
            });

        in_ = std::make_unique<Stockfish::UCICommandChannel>();
        analysis_ = std::make_unique<Stockfish::UCIAnalysisChannel>();
        engine_ = std::make_unique<Stockfish::UCIEngine>(argc, argv, in_.get(), analysis_.get());

        Stockfish::Tune::init(engine_->engine_options());

        worker_ = std::thread([this]() {
            engine_->loop();
            });

        send_command("uci");
        wait_input_idle_or_closed();

        send_command("isready");
        wait_input_idle_or_closed();
    }

    ~StockfishRunner() {
        shutdown();
    }

    StockfishRunner(const StockfishRunner&) = delete;
    StockfishRunner& operator=(const StockfishRunner&) = delete;

    StockfishRunner(StockfishRunner&&) = delete;
    StockfishRunner& operator=(StockfishRunner&&) = delete;

    bool set_option(const std::string& name, const std::string& value) {
        return send_and_wait("setoption name " + name + " value " + value);
    }

    bool new_game() {
        return send_and_wait("ucinewgame");
    }

    bool set_position_fen(const std::string& fen) {
        return send_and_wait("position fen " + fen);
    }

    bool set_position_fen(const std::string& fen, const std::string& movesSuffix) {
        return send_and_wait("position fen " + fen + " moves " + movesSuffix);
    }

    bool set_position_startpos() {
        return send_and_wait("position startpos");
    }

    bool set_position_startpos(const std::string& movesSuffix) {
        return send_and_wait("position startpos moves " + movesSuffix);
    }

    bool go_movetime(int ms) {
        if (!send_command("go movetime " + std::to_string(ms)))
            return false;

        wait_input_idle_or_closed();
        return true;
    }

    bool go_depth(int depth) {
        if (!send_command("go depth " + std::to_string(depth)))
            return false;

        wait_input_idle_or_closed();
        return true;
    }

    bool stop() {
        return send_and_wait("stop");
    }

    bool wait_for_search_finish() {
        if (!analysis_)
            return false;

        while (!analysis_->searchFinished.load(std::memory_order_acquire)) {
            if (analysis_->closed.load(std::memory_order_acquire))
                return false;

            std::this_thread::yield();
        }

        return true;
    }

    Result get_result() const {
        Result r;

        if (!analysis_)
            return r;

        r.searchFinished = analysis_->searchFinished.load(std::memory_order_acquire);
        r.hasScore = analysis_->hasScore.load(std::memory_order_acquire);
        r.scoreIsMate = analysis_->scoreIsMate.load(std::memory_order_acquire);
        r.scoreValue = analysis_->scoreValue.load(std::memory_order_acquire);
        r.depth = analysis_->depth.load(std::memory_order_acquire);
        r.selDepth = analysis_->selDepth.load(std::memory_order_acquire);
        r.multiPV = analysis_->multiPV.load(std::memory_order_acquire);
        r.nodes = analysis_->nodes.load(std::memory_order_acquire);
        r.nps = analysis_->nps.load(std::memory_order_acquire);
        r.hashfull = analysis_->hashfull.load(std::memory_order_acquire);
        r.tbHits = analysis_->tbHits.load(std::memory_order_acquire);
        r.timeMs = analysis_->timeMs.load(std::memory_order_acquire);
        r.currMoveNumber = analysis_->currMoveNumber.load(std::memory_order_acquire);

        {
            std::lock_guard<std::mutex> lock(analysis_->textMutex);
            r.currMove = analysis_->currMove;
            r.bestMove = analysis_->bestMove;
            r.ponder = analysis_->ponder;
            r.pv = analysis_->pv;
        }

        return r;
    }

    std::string best_move(int ms) {
        if (!go_movetime(ms))
            return {};

        if (!wait_for_search_finish())
            return {};

        return get_result().bestMove;
    }

    std::string best_move_depth(int depth) {
        if (!go_depth(depth))
            return {};

        if (!wait_for_search_finish())
            return {};

        return get_result().bestMove;
    }

    void shutdown() {
        if (shutdownCalled_)
            return;

        shutdownCalled_ = true;

        if (in_ && in_->state.load(std::memory_order_acquire) != Stockfish::UCICommandChannel::State::Closed) {
            send_command("quit");
            wait_input_idle_or_closed();
        }

        if (worker_.joinable())
            worker_.join();
    }
};

}