#pragma once

#include <SFML/Network.hpp>

#include "ChessBoard.hpp"
#include "Move.hpp"

#include <atomic>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace myChess {

class ChessApp;

class Room {
public:
    enum class Mode {
        LocalPeerToPeer,
        OfficialServer
    };

    std::atomic<bool> to_updateUI{ false };

private:
    friend class ChessApp;

    ChessBoard board;

    std::string IP_ToConnectTo_string;
    std::string name;
    std::string password = "";
    std::string lastError;

    bool is_thisPlayerWhite = true;
    bool is_this_a_host = false;
    bool are_bothPlayersPresent = false;

    std::unique_ptr<std::thread> roomThread;
    unsigned short port = 54000;
    sf::Socket::Status status = sf::Socket::Done;
    Move lastMove;

    std::atomic<bool> is_thisMove_made{ false };
    std::atomic<bool> to_terminate_handlingThread{ false };
    Mode mode_ = Mode::LocalPeerToPeer;

    // official-server-only state
    sf::TcpSocket officialServer_;
    std::atomic<bool> officialConnected_{ false };

    static void message_to_move(char* msgBuffer, Move& move) {
        try {
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

            move = { { i, j }, { i1, j1 }, (PieceType)ptFrom, (PieceType)ptTo };
        }
        catch (...) {
            std::cerr << "invalid message string\n";
        }
    }

    void loadJoinAddressIfNeeded() {
        if (!IP_ToConnectTo_string.empty())
            return;

        std::ifstream ip("IP.txt");
        if (!ip.is_open()) {
            std::cerr << "no IP.txt file found\n";
            IP_ToConnectTo_string = "127.0.0.1";
            return;
        }

        std::getline(ip, IP_ToConnectTo_string);
        if (IP_ToConnectTo_string.empty())
            IP_ToConnectTo_string = "127.0.0.1";
    }

    // =========================
    // Local peer-to-peer mode
    // =========================
    void connectNewClient(sf::SocketSelector& selector,
                          sf::TcpListener& listener,
                          std::vector<std::unique_ptr<sf::TcpSocket>>& clients) {
        if (!selector.isReady(listener))
            return;

        auto client = std::make_unique<sf::TcpSocket>();
        status = listener.accept(*client);

        if (status != sf::Socket::Done)
            return;

        std::cout << "New Client: " << client->getRemoteAddress() << "\n";
        selector.add(*client);
        clients.push_back(std::move(client));

        sf::Packet packet;
        packet << std::string({ static_cast<char>(!is_thisPlayerWhite), 0 });
        clients[0]->send(packet);
    }

    void waitAndHandleClientsMove(sf::TcpListener& listener,
                                  sf::SocketSelector& selector,
                                  std::vector<std::unique_ptr<sf::TcpSocket>>& clients) {
        while (!to_terminate_handlingThread.load()) {
            if (!selector.wait())
                continue;

            if (clients.empty()) {
                lastError = "client has been disconnected, waiting for connection...";
                connectNewClient(selector, listener, clients);
                if (status == sf::Socket::Done)
                    are_bothPlayersPresent = true;
                else
                    continue;
            }

            for (auto it = clients.begin(); it != clients.end();) {
                auto& client = *it;
                if (!selector.isReady(*client)) {
                    ++it;
                    continue;
                }

                std::string message;
                sf::Packet packet;
                status = client->receive(packet);
                packet >> message;

                std::cout << "received: " << message << "\n";

                if (status == sf::Socket::Disconnected) {
                    std::cout << "Client disconnected: " << client->getRemoteAddress() << "\n";
                    selector.remove(*client);
                    it = clients.erase(it);
                    are_bothPlayersPresent = false;
                    continue;
                }
                else if (status == sf::Socket::Done) {
                    char buffer[64]{};
                    std::strncpy(buffer, message.c_str(), sizeof(buffer) - 1);
                    message_to_move(buffer, lastMove);
                    if (board.makeMove(lastMove))
                        return;
                }

                ++it;
            }
        }
    }

    void waitAndHandleHostsMove(sf::TcpSocket& server, std::string& message) {
        while (!to_terminate_handlingThread.load()) {
            sf::Packet packet;
            status = server.receive(packet);
            packet >> message;

            std::cout << "received: " << message << "\n";

            if (status == sf::Socket::Disconnected) {
                std::cout << "your opponent left lol.\n";
                are_bothPlayersPresent = false;
                return;
            }

            if (status != sf::Socket::Done)
                continue;

            char buffer[64]{};
            std::strncpy(buffer, message.c_str(), sizeof(buffer) - 1);
            message_to_move(buffer, lastMove);

            if (board.makeMove(lastMove)) 
            {
                to_updateUI.store(true);
                return;
            }
        }
    }

    void waitAndHandleThisMove(sf::TcpSocket& socket) {
        is_thisMove_made.wait(false);
        if (to_terminate_handlingThread.load())
            return;

        std::string message = lastMove.to_string();
        sf::Packet packet;
        packet << message;
        socket.send(packet);

        std::cout << "sent: " << message << "\n";
        is_thisMove_made.store(false);
    }

    void handleHost() {
        sf::TcpListener listener;
        if (listener.listen(port) != sf::Socket::Done) {
            lastError = "Couldn't open the port: " + std::to_string(port);
            return;
        }

        std::cout << "Server is waiting for connections\n";

        sf::SocketSelector selector;
        std::vector<std::unique_ptr<sf::TcpSocket>> clients;
        selector.add(listener);

        if (selector.wait() && clients.empty()) {
            lastError = "waiting for connection...";
            connectNewClient(selector, listener, clients);
            if (status == sf::Socket::Done)
                are_bothPlayersPresent = true;
        }

        while (!to_terminate_handlingThread.load()) {
            if (clients.empty() && selector.wait()) {
                connectNewClient(selector, listener, clients);
                if (status == sf::Socket::Done)
                    are_bothPlayersPresent = true;
                else
                    continue;
            }

            if (clients.empty())
                continue;

            if (is_thisPlayerWhite == board.isWhitesMove())
                waitAndHandleThisMove(*clients[0]);
            else
                waitAndHandleClientsMove(listener, selector, clients);
        }
    }

    void handleJoin() {
        sf::TcpSocket server;
        if (server.connect(IP_ToConnectTo_string, port) != sf::Socket::Done) {
            std::cerr << "Couldn't connect to the server\n";
            lastError = "Couldn't connect to the server";
            return;
        }

        are_bothPlayersPresent = true;
        std::string message;
        sf::Packet packet;
        status = server.receive(packet);
        packet >> message;

        if (!message.empty())
            is_thisPlayerWhite = static_cast<bool>(message[0]);

        while (!to_terminate_handlingThread.load()) {
            if (status == sf::Socket::Disconnected) {
                are_bothPlayersPresent = false;
                return;
            }

            if (is_thisPlayerWhite == board.isWhitesMove())
                waitAndHandleThisMove(server);
            else
                waitAndHandleHostsMove(server, message);
        }
    }

    // =========================
    // Official server mode
    // =========================
    bool sendOfficialString(const std::string& msg) {
        sf::Packet packet;
        packet << msg;
        return officialServer_.send(packet) == sf::Socket::Done;
    }

    bool recvOfficialString(std::string& msg) {
        sf::Packet packet;
        const auto st = officialServer_.receive(packet);
        if (st != sf::Socket::Done)
            return false;
        packet >> msg;
        return true;
    }

    void officialReceiveLoop() {
        std::string msg;

        while (!to_terminate_handlingThread.load()) {
            if (!recvOfficialString(msg)) {
                officialConnected_.store(false);
                are_bothPlayersPresent = false;
                lastError = "Disconnected from official server";
                return;
            }

            const auto sep = msg.find('|');
            const std::string type = (sep == std::string::npos) ? msg : msg.substr(0, sep);
            const std::string payload = (sep == std::string::npos) ? "" : msg.substr(sep + 1);

            if (type == "WELCOME" || type == "HELLO_OK" || type == "QUEUED" || type == "INFO")
                continue;

            if (type == "START") {
                is_thisPlayerWhite = (payload == "W");
                are_bothPlayersPresent = true;
                continue;
            }

            if (type == "MOVE") {
                char buffer[64]{};
                std::strncpy(buffer, payload.c_str(), sizeof(buffer) - 1);
                Move move{};
                message_to_move(buffer, move);

                if(board.makeMove(move))
				    to_updateUI.store(true);

                continue;
            }

            if (type == "OPPONENT_LEFT") {
                are_bothPlayersPresent = false;
                continue;
            }

            if (type == "ERROR") {
                lastError = payload;
                continue;
            }
        }
    }

    void handleOfficialServer() {
        loadJoinAddressIfNeeded();

        if (officialServer_.connect(IP_ToConnectTo_string, port) != sf::Socket::Done) {
            lastError = "Couldn't connect to official server";
            return;
        }

        officialConnected_.store(true);
        sendOfficialString("HELLO|" + (name.empty() ? std::string("player") : name));
        sendOfficialString("QUEUE|");

        officialReceiveLoop();
    }

public:
    Room(std::string Name,
         std::string Password,
         bool toHost,
         bool isThisPlayerWhite = true,
         Mode mode = Mode::LocalPeerToPeer,
         unsigned short Port = 54000,
         std::string ip = "")
        : IP_ToConnectTo_string(std::move(ip))
        , name(std::move(Name))
        , password(std::move(Password))
        , is_thisPlayerWhite(isThisPlayerWhite)
        , is_this_a_host(toHost)
        , port(Port)
        , mode_(mode) {
        if (mode_ == Mode::OfficialServer) {
            roomThread.reset(new std::thread([this]() { handleOfficialServer(); }));
            return;
        }

        if (is_this_a_host) {
            roomThread.reset(new std::thread([this]() { handleHost(); }));
        }
        else {
            loadJoinAddressIfNeeded();
            roomThread.reset(new std::thread([this]() { handleJoin(); }));
        }
    }

    ~Room() {
        to_terminate_handlingThread.store(true);
        is_thisMove_made.store(true);
        is_thisMove_made.notify_all();

        if (mode_ == Mode::OfficialServer && officialConnected_.load()) {
            sendOfficialString("LEAVE|");
            officialServer_.disconnect();
        }

        if (roomThread && roomThread->joinable())
            roomThread->join();
    }

    bool sendMove(Move& move) {
        if (!are_bothPlayersPresent)
            return false;

        if (board.isWhitesMove() != is_thisPlayerWhite)
            return false;

        if (!board.makeMove(move))
            return false;

        if (mode_ == Mode::OfficialServer)
            return sendOfficialString("MOVE|" + move.to_string());

        lastMove = move;
        is_thisMove_made.store(true);
        is_thisMove_made.notify_all();
        return true;
    }

    bool requestCancelLastMove() {
        // TODO: extend protocol if you want server-side undo support.
        return false;
    }

    bool isThisPlayerWhite() const {
        return is_thisPlayerWhite;
    }

    bool bothPlayersPresent() const {
        return are_bothPlayersPresent;
    }

    const std::string& lastErrorMessage() const {
        return lastError;
    }
};

} // namespace myChess
