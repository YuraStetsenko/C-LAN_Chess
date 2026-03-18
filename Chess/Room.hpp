#pragma once
#include <SFML/Network.hpp>
#include "ChessBoard.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>



class Room {
private:
	ChessBoard board;
	std::string IP_ToConnectTo_string;
	std::string name;
	std::string password = "";
	std::string lastError;
	bool is_thisPlayerWhite = true;
	bool is_this_a_host;
	bool are_bothPlayersPresent = false;
	std::unique_ptr <std::thread> roomThread;
	short port = 54000;
	sf::Socket::Status status;
	Move lastMove;

	std::atomic<bool> is_thisMove_made{false};
	std::atomic<bool> to_terminate_handlingThread{false};


	friend class ChessApp;

	void message_to_move(char* msgBuffer, Move& move) {
		try {
			unsigned i = 0, j = 0, i1 = 0, j1 = 0;
			char* tokPtr = NULL;
			char* endPtr = NULL;

			int ptFrom, ptTo;

			tokPtr = std::strtok(msgBuffer, ",");
			i = std::strtol(msgBuffer, &endPtr, 10);

			tokPtr = std::strtok(NULL, ",");
			j = std::strtol(tokPtr, &endPtr, 10);

			tokPtr = std::strtok(NULL, ",");
			i1 = std::strtol(tokPtr, &endPtr, 10);

			tokPtr = std::strtok(NULL, ",");
			j1 = std::strtol(tokPtr, &endPtr, 10);

			tokPtr = std::strtok(NULL, ",");
			ptFrom = std::strtol(tokPtr, &endPtr, 10);

			tokPtr = std::strtok(NULL, ",");
			ptTo = std::strtol(tokPtr, &endPtr, 10);

			move = { {i,j }, {i1,j1}, (PieceType)ptFrom,(PieceType)ptTo };
		}
		catch (...) {
			std::cerr << "invalid message string\n";
		}
	}

	void connectNewClient(sf::SocketSelector& selector, sf::TcpListener& listener, std::vector<std::unique_ptr<sf::TcpSocket>>& clients) {
		if (selector.isReady(listener)) {
			auto client = std::make_unique<sf::TcpSocket>();

			status = listener.accept(*client);
			if (status == sf::Socket::Done) {
				std::cout << "New Client: " << client->getRemoteAddress() << "\n";
				selector.add(*client);
				clients.push_back(std::move(client));

				sf::Packet packet;
				packet << std::string({ !is_thisPlayerWhite, 0 });
				clients[0]->send(packet);
			}
		}
	}

	void waitAndHandleClientsMove(sf::TcpListener& listener, sf::SocketSelector& selector, std::vector<std::unique_ptr<sf::TcpSocket>>& clients) {
		while (!to_terminate_handlingThread.load()) 
		{


			if (selector.wait())
			{


				if (clients.size() < 1) {
					lastError = "client has been disconnected, waiting for connection...";
					connectNewClient(selector, listener, clients);

					if (status == sf::Socket::Done)
						are_bothPlayersPresent = true;
					else
						continue;
				}


				// Checking the messages
				for (auto it = clients.begin(); it != clients.end();) 
				{
					auto& client = *it;

					if (selector.isReady(*client))
					{
						std::string message;
						sf::Packet packet;
						status = client->receive(packet);
						packet >> message;

						std::cout << "received: " + message;

						//if Disconnected delete it
						if (status == sf::Socket::Disconnected) {
							std::cout << "Client disconnected: " << client->getRemoteAddress() << "\n"; //log
							selector.remove(*client);
							it = clients.erase(it);  

							are_bothPlayersPresent = false;

							continue;
						}

						//Handle client's message
						else if (status == sf::Socket::Done) 
						{
							char buffer[64];
							std::strcpy(buffer, message.c_str());
							message_to_move(buffer, lastMove);

							if (board.makeMove(lastMove))
								return;
						}
						

						++it;
					}


				}


			}


		}
	}

	void waitAndHandleHostsMove(sf::TcpSocket& server, std::string& message) {
		while (!to_terminate_handlingThread.load())
		{
			sf::Packet packet;
			status = server.receive(packet);
			packet >> message;

			std::cout << "received: " + message;

			if (status == sf::Socket::Disconnected) {
				std::cout << "your opponent left lol.";
				are_bothPlayersPresent = false;
				return;
			}
			if (status != sf::Socket::Done)
				continue;
			
			char buffer[64];
			std::strcpy(buffer, message.c_str());
			message_to_move(buffer, lastMove);
			
			if (board.makeMove(lastMove))
				return;
		}
	}

	void waitAndHandleThisMove(sf::TcpSocket& socket)
	{
		is_thisMove_made.wait(false);
		
		std::string message = lastMove.to_string();
	
		sf::Packet packet;
		packet << message;
		socket.send(packet);
		std::cout << "sent: "+ message;

		is_thisMove_made.store(false);
	}

	void handleHost() {
		sf::TcpListener listener;
		if (listener.listen(54000) != sf::Socket::Done) {
			lastError = "Couldn't open the port: 54000\n"; //TODO:: port validation
			return;
		}
		std::cout << "Server is waiting for connections";

		sf::SocketSelector selector;
		std::vector<std::unique_ptr<sf::TcpSocket>> clients;
		
		selector.add(listener);

		if (selector.wait())
		{
			if (clients.size() < 1) {
				lastError = "waiting for connection...";
				connectNewClient(selector, listener, clients);

				if (status == sf::Socket::Done)
					are_bothPlayersPresent = true;
			}
		}

		while (!to_terminate_handlingThread.load()) {
			if (clients.size() < 1 && selector.wait())
			{
				connectNewClient(selector, listener, clients);
				
				if (status == sf::Socket::Done)
					are_bothPlayersPresent = true;
				else
					continue;
			}

			if (is_thisPlayerWhite == board.isWhitesMove())
				waitAndHandleThisMove(*clients[0]);
			else
				waitAndHandleClientsMove(listener, selector, clients);
		}

		
		
	}

	void handleJoin() {
		sf::TcpSocket server;
		if (server.connect(IP_ToConnectTo_string, 54000) != sf::Socket::Done) {
			std::cerr << "Couldn't connect to the server\n";
			return;
		}

		are_bothPlayersPresent = true;

		std::string message;
		sf::Packet packet;
		status = server.receive(packet);
		packet >> message;
		is_thisPlayerWhite = message[0];

		while (!to_terminate_handlingThread.load())
		{
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
		
	

public:
	Room(std::string Name, std::string Password, bool toHost, bool isThisPlayerWhite = true) : name(Name), password(Password), is_this_a_host(toHost), is_thisPlayerWhite(isThisPlayerWhite){
		if (is_this_a_host) {
			roomThread.reset(new std::thread([&]() { (*this).handleHost(); }));
		}
		else {
			std::ifstream ip("IP.txt");

			if (!ip.is_open()) {
				std::cerr << "no IP.txt file found \n";
				IP_ToConnectTo_string = "localhost";
			}
			else {
				std::getline(ip, IP_ToConnectTo_string);
				if (IP_ToConnectTo_string.empty())
					IP_ToConnectTo_string = "localhost";

				ip.close();
			}

			roomThread.reset(new std::thread([&]() { (*this).handleJoin(); }));
		}
	}
	~Room() {
		to_terminate_handlingThread.store(true);
		roomThread->join();
	}

	bool sendMove( Move& move) {
		if (!are_bothPlayersPresent)  //TODO:: add are_bothPlayersPresent flag to the logic of reconnection
			return false;

		if (board.isWhitesMove() != is_thisPlayerWhite)
			return false;

		if (!board.makeMove(move))
			return false;

		lastMove = move;
		is_thisMove_made.store(true);
		is_thisMove_made.notify_all();

		return true;
	}

	bool requestCancelLastMove() {
		//some logic later
		return false;
	}
		
	
};

//
//if (listener.listen(54000) != sf::Socket::Done) {
//    std::cerr << "E: не вдалося відкрити порт 54000\n";
//    return 1;
//}
//std::cout << "Сервер чекає на підключення...\n";
//
//sf::TcpSocket client;
//if (listener.accept(client) != sf::Socket::Done) {
//    std::cerr << "Помилка: не вдалося прийняти підключення\n";
//    return 1;
//}
//std::cout << "Клієнт підключений!\n";
//
//// Приймаємо дані
//char buffer[128];
//std::size_t received;
//if (client.receive(buffer, sizeof(buffer), received) == sf::Socket::Done) {
//    std::cout << "Отримано: " << std::string(buffer, received) << "\n";
//}
//
//// Відправляємо відповідь
//std::string response = "Привіт від сервера!";
//client.send(response.c_str(), response.size() + 1);
//
//return 0;