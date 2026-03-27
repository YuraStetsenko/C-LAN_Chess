#include <SFML/Network.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace serverapp {

using ClientId = std::uint64_t;

struct Session;
struct Match {
    std::shared_ptr<Session> white;
    std::shared_ptr<Session> black;
};

struct Session : std::enable_shared_from_this<Session> {
    ClientId id{};
    std::string name;
    sf::TcpSocket socket;
    std::thread thread;
    std::mutex sendMutex;
    std::atomic<bool> running{true};
    std::weak_ptr<Session> opponent;
    bool inMatch = false;
    bool isWhite = false;

    Session() { socket.setBlocking(true); }
};

class ChessRelayServer {
public:
    explicit ChessRelayServer(unsigned short port) : port_(port) {}

    bool start() {
        if (listener_.listen(port_) != sf::Socket::Done) {
            std::cerr << "Failed to listen on port " << port_ << "\n";
            return false;
        }

        std::cout << "Official chess server listening on port " << port_ << "\n";
        running_.store(true);
        acceptThread_ = std::thread([this] { acceptLoop(); });
        return true;
    }

    void stop() {
        if (!running_.exchange(false)) return;

        listener_.close();
        if (acceptThread_.joinable()) acceptThread_.join();

        std::vector<std::shared_ptr<Session>> sessionsCopy;
        {
            std::scoped_lock lock(mutex_);
            for (auto& [_, s] : sessions_) {
                sessionsCopy.push_back(s);
            }
            sessions_.clear();
            waitingQueue_.clear();
        }

        for (auto& s : sessionsCopy) {
            s->running.store(false);
            s->socket.disconnect();
        }

        for (auto& s : sessionsCopy) {
            if (s->thread.joinable()) s->thread.join();
        }
    }

    ~ChessRelayServer() { stop(); }

private:
    void acceptLoop() {
        while (running_.load()) {
            auto session = std::make_shared<Session>();
            const auto status = listener_.accept(session->socket);
            if (status != sf::Socket::Done) {
                if (running_.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                continue;
            }

            session->id = nextId_++;
            {
                std::scoped_lock lock(mutex_);
                sessions_[session->id] = session;
            }

            std::cout << "Client connected: id=" << session->id
                      << " ip=" << session->socket.getRemoteAddress().toString() << "\n";

            session->thread = std::thread([this, session] { clientLoop(session); });
        }
    }

    static bool recvString(sf::TcpSocket& socket, std::string& out) {
        sf::Packet packet;
        const auto status = socket.receive(packet);
        if (status != sf::Socket::Done) return false;
        packet >> out;
        return true;
    }

    static bool sendString(Session& session, const std::string& msg) {
        sf::Packet packet;
        packet << msg;
        std::scoped_lock lock(session.sendMutex);
        return session.socket.send(packet) == sf::Socket::Done;
    }

    static std::optional<std::pair<std::string, std::string>> splitCommand(const std::string& msg) {
        const auto pos = msg.find('|');
        if (pos == std::string::npos) {
            return std::pair<std::string, std::string>{msg, ""};
        }
        return std::pair<std::string, std::string>{msg.substr(0, pos), msg.substr(pos + 1)};
    }

    void clientLoop(const std::shared_ptr<Session>& session) {
        sendString(*session, "WELCOME|Send HELLO|name then QUEUE");

        std::string msg;
        while (running_.load() && session->running.load()) {
            if (!recvString(session->socket, msg)) {
                disconnectSession(session);
                return;
            }

            const auto cmd = splitCommand(msg);
            if (!cmd) continue;

            if (cmd->first == "HELLO") {
                session->name = cmd->second.empty() ? ("player_" + std::to_string(session->id)) : cmd->second;
                sendString(*session, "HELLO_OK|" + session->name);
                continue;
            }

            if (cmd->first == "QUEUE") {
                enqueueForMatch(session);
                continue;
            }

            if (cmd->first == "MOVE") {
                handleMove(session, cmd->second);
                continue;
            }

            if (cmd->first == "PING") {
                sendString(*session, "PONG|");
                continue;
            }

            if (cmd->first == "LEAVE") {
                disconnectSession(session);
                return;
            }

            sendString(*session, "ERROR|Unknown command");
        }

        disconnectSession(session);
    }

    void enqueueForMatch(const std::shared_ptr<Session>& session) {
        std::shared_ptr<Session> opponent;
        {
            std::scoped_lock lock(mutex_);
            if (session->inMatch) {
                sendString(*session, "ERROR|Already in a match");
                return;
            }

            for (auto it = waitingQueue_.begin(); it != waitingQueue_.end(); ++it) {
                if ((*it)->id != session->id && (*it)->running.load() && !(*it)->inMatch) {
                    opponent = *it;
                    waitingQueue_.erase(it);
                    break;
                }
            }

            if (!opponent) {
                waitingQueue_.push_back(session);
                sendString(*session, "QUEUED|");
                return;
            }

            session->inMatch = true;
            opponent->inMatch = true;
            session->isWhite = false;
            opponent->isWhite = true;
            session->opponent = opponent;
            opponent->opponent = session;
        }

        sendString(*opponent, "START|W");
        sendString(*session, "START|B");
        sendString(*opponent, "INFO|Opponent=" + session->name);
        sendString(*session, "INFO|Opponent=" + opponent->name);
    }

    void handleMove(const std::shared_ptr<Session>& session, const std::string& movePayload) {
        auto opponent = session->opponent.lock();
        if (!opponent || !session->inMatch) {
            sendString(*session, "ERROR|Not in a match");
            return;
        }

        if (!sendString(*opponent, "MOVE|" + movePayload)) {
            disconnectSession(opponent);
            sendString(*session, "ERROR|Opponent disconnected while sending move");
        }
    }

    void disconnectSession(const std::shared_ptr<Session>& session) {
        if (!session->running.exchange(false)) return;

        std::shared_ptr<Session> opponent;
        {
            std::scoped_lock lock(mutex_);
            sessions_.erase(session->id);

            waitingQueue_.erase(
                std::remove_if(waitingQueue_.begin(), waitingQueue_.end(),
                               [&](const std::shared_ptr<Session>& s) { return s->id == session->id; }),
                waitingQueue_.end());

            opponent = session->opponent.lock();
            if (opponent) {
                opponent->opponent.reset();
                opponent->inMatch = false;
            }
        }

        if (opponent && opponent->running.load()) {
            sendString(*opponent, "OPPONENT_LEFT|");
        }

        session->socket.disconnect();

        if (session->thread.joinable() && std::this_thread::get_id() != session->thread.get_id()) {
            session->thread.join();
        }

        std::cout << "Client disconnected: id=" << session->id << "\n";
    }

private:
    unsigned short port_;
    sf::TcpListener listener_;
    std::atomic<bool> running_{false};
    std::thread acceptThread_;
    std::atomic<ClientId> nextId_{1};

    std::mutex mutex_;
    std::unordered_map<ClientId, std::shared_ptr<Session>> sessions_;
    std::deque<std::shared_ptr<Session>> waitingQueue_;
};

} // namespace serverapp

int main() {
    constexpr unsigned short kPort = 54000;
    serverapp::ChessRelayServer server{kPort};
    if (!server.start()) return 1;

    std::cout << "Press ENTER to stop the server...\n";
    std::string line;
    std::getline(std::cin, line);
    server.stop();
    return 0;
}
