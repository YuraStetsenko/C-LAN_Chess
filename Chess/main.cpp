#define _CRT_SECURE_NO_WARNINGS
#include "ChessApp.hpp"



int main() {
	sf::RenderWindow window(sf::VideoMode(), "Chess", sf::Style::Fullscreen);
	sf::Event event;

	ChessApp game(window);

	short playMode = game.mainMenu();

	while (window.isOpen()) {
		while (window.pollEvent(event)) {
			if (event.type == sf::Event::Closed)
			{
				//closing procedure
				window.close();
			}

			if (event.type == sf::Event::MouseButtonPressed)
				switch (game.handleClick(event)) 
				{
				case ChessApp::Signal::ToTryMove:

					//Applies move if it's valid
					if (game.tryMove() && playMode == ChessApp::GameMode::SingleScreen)
						game.toggleCurrentPlayersColor();

					break;
				case ChessApp::Signal::ToPassToUI:

					//Some bs with UI idk

					break;
				}
		}
		
		window.clear(sf::Color::Black);
		
		game.updateUI();

		window.display();
	}
}


//
//sf::TcpSocket socket;
//if (socket.connect("127.0.0.1", 54000) != sf::Socket::Done) {
//    std::cerr << "Error: не вдалося підключитися до сервера\n";
//    return 1;
//}
//std::cout << "Connected to the server!\n";
//
//// Відправляємо повідомлення
//std::string message = "Hello to the server!";
//socket.send(message.c_str(), message.size() + 1);
//
//// Отримуємо відповідь
//char buffer[128];
//std::size_t received;
//if (socket.receive(buffer, sizeof(buffer), received) == sf::Socket::Done) {
//    std::cout << "Received: " << std::string(buffer, received) << "\n";
//}
//
//return 0;