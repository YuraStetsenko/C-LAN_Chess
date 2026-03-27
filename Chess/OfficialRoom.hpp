#pragma once

#include <SFML/Network.hpp>

#include <atomic>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "ChessBoard.hpp"
#include "Move.hpp"

namespace myChess {

class OfficialRoom {
private:
    sf::TcpSocket server_;
    std::unique_ptr<std::thread> thread_;
    std::atomic<bool> terminate_{false};
    std::atomic<bool> connected_{false};
    std::string serverIp_ = "127.0.0.1";
    unsigned short port_ = 54000;
    std::string playerName_ = "player";
    bool isThisPlayerWhite_ = false;
    bool areBothPlayersPresent_ = false;
    std::string lastError_;

    static void message_to_move(char* msgBuffer, Move& move) {
        unsigned i = 0, j = 0, i1 = 0, j1 = 0;
        char* tokPtr = nullptr;
        char* endPtr = nullptr;
        int ptFrom = 0, ptTo = 0;

        tokPtr = std::strtok(msgBuffer, ",");
        i = std::strtol(msgBuffer, &endPtr, 10);
        tokPtr = std::strtok(nullptr, ",");
        j = std::strtol(tokPtr, &endPtr, 10);
        tokPtr = std::strtok(nullptr, ",");
        i1 = std::strtol(tokPtr, &endPtr, 10);
        tokPtr = std::strtok(nullptr, ",");
        j1 = std::strtol(tokPtr, &endPtr, 10);
        tokPtr = std::strtok(nullptr, ",");
        ptFrom = std::strtol(tokPtr, &endPtr, 10);
        tokPtr = std::strtok(nullptr, ",");
        ptTo = std::strtol(tokPtr, &endPtr, 10);

        move = { {i, j}, {i1, j1}, (PieceType)ptFrom, (PieceType)ptTo };
    }

    bool sendString(const std::string& msg) {
        sf::Packet packet;
        packet << msg;
        return server_.send(packet) == sf::Socket::Done;
    }

    bool recvString(std::string& msg) {
        sf::Packet packet;
        const auto status = server_.receive(packet);
        if (status != sf::Socket::Done) return false;
        packet >> msg;
        return true;
    }

    void receiveLoop() {
        std::string msg;
        while (!terminate_.load()) {
            if (!recvString(msg)) {
                connected_.store(false);
                areBothPlayersPresent_ = false;
                return;
            }

            const auto sep = msg.find('|');
            const std::string type = (sep == std::string::npos) ? msg : msg.substr(0, sep);
            const std::string payload = (sep == std::string::npos) ? "" : msg.substr(sep + 1);

            if (type == "WELCOME" || type == "HELLO_OK" || type == "QUEUED" || type == "INFO") {
                continue;
            }

            if (type == "START") {
                isThisPlayerWhite_ = (payload == "W");
                areBothPlayersPresent_ = true;
                continue;
            }

            if (type == "MOVE") {
                char buffer[64]{};
                std::strncpy(buffer, payload.c_str(), sizeof(buffer) - 1);
                Move move{};
                message_to_move(buffer, move);
                board.makeMove(move);
                continue;
            }

            if (type == "OPPONENT_LEFT") {
                areBothPlayersPresent_ = false;
                continue;
            }

            if (type == "ERROR") {
                lastError_ = payload;
                continue;
            }
        }
    }

public:
    ChessBoard board;

    OfficialRoom(std::string playerName = "player", std::string serverIp = "", unsigned short port = 54000)
        : port_(port), playerName_(playerName.empty() ? "player" : playerName) {
        if (serverIp.empty()) {
            std::ifstream ip("IP.txt");
            if (ip.is_open()) {
                std::getline(ip, serverIp_);
            }
            if (serverIp_.empty()) serverIp_ = "127.0.0.1";
        } else {
            serverIp_ = serverIp;
        }

        if (server_.connect(serverIp_, port_) != sf::Socket::Done) {
            lastError_ = "Couldn't connect to official server";
            return;
        }

        connected_.store(true);
        sendString("HELLO|" + playerName_);
        sendString("QUEUE|");
        thread_.reset(new std::thread([this] { receiveLoop(); }));
    }

    ~OfficialRoom() {
        terminate_.store(true);
        if (connected_.load()) {
            sendString("LEAVE|");
            server_.disconnect();
        }
        if (thread_ && thread_->joinable()) thread_->join();
    }

    bool sendMove(Move& move) {
        if (!areBothPlayersPresent_) return false;
        if (board.isWhitesMove() != isThisPlayerWhite_) return false;
        if (!board.makeMove(move)) return false;
        return sendString("MOVE|" + move.to_string());
    }

    bool requestCancelLastMove() {
        return false;
    }

    bool isThisPlayerWhite() const { return isThisPlayerWhite_; }
    bool bothPlayersPresent() const { return areBothPlayersPresent_; }
    const std::string& lastError() const { return lastError_; }
};

} // namespace myChess
