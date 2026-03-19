#pragma once
#include "Room.hpp"

bool loadFontFromResource(sf::Font& font, int resourceId) {
	HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(resourceId), RT_RCDATA);
	if (!hRes) return false;

	HGLOBAL hData = LoadResource(NULL, hRes);
	if (!hData) return false;

	void* pData = LockResource(hData);
	DWORD dataSize = SizeofResource(NULL, hRes);
	if (!pData || dataSize == 0) return false;

	return font.loadFromMemory(pData, dataSize);
}

class ChessApp {
public:
	enum GameMode {
		Bot = 0,
		SingleScreen = 1,
		LocalNetwork = 2,
		OfficialServer = 3
	};

	enum Signal {
		NoSignal = 0,
		ToTryMove = 1,
		ToPassToUI = 2
	};

private:
	sf::RenderWindow& window;
	std::unique_ptr<Room> pRoom;
	ChessBoard* pBoard;
	
	float squareSize;
	sf::Vector2f boardPosition;
	std::vector<std::vector<sf::RectangleShape>> squares;

	bool is_host = true;
	bool is_player_white = true;
	bool hasSelectedPiece = false;
	std::pair<unsigned, unsigned> toCell;
	std::pair<unsigned, unsigned> fromCell;
	short playMode = 1;

public:

	ChessApp() = delete;
	ChessApp(sf::RenderWindow& Window) : window(Window) 
	{
		squareSize = std::min(window.getSize().x, window.getSize().y) / 12.f;
		sf::RectangleShape initRect({ squareSize, squareSize });
		squares = std::vector<std::vector<sf::RectangleShape>>(8, std::vector<sf::RectangleShape>(8, initRect));
		boardPosition = { ((window.getSize().x / 2.f) - 8 * squareSize) / 2.f, (window.getSize().y - 8 * squareSize) / 2.f };

		for (int i = 0; i < 8; i++) {
			for (int j = 0 + (i % 2); j < 8; j += 2) {
				squares[i][j].setFillColor(sf::Color::White);
				squares[i][j].setPosition(boardPosition + sf::Vector2f({ i * squareSize, j * squareSize }));
			}
			for (int j = 1 - (i % 2); j < 8; j += 2) {
				squares[i][j].setFillColor(sf::Color::Blue);
				squares[i][j].setPosition(boardPosition + sf::Vector2f({ i * squareSize, j * squareSize }));
			}
		}
	}
	~ChessApp() {
		if (playMode == 2)
			return;

		delete pBoard;
	}

	//handles piece selection and sends a signal to send a move if necessary
	Signal handleClick(sf::Event& event, bool gameIsOn = true) {

		sf::Vector2f mousePos({ (float)event.mouseButton.x,  (float)event.mouseButton.y });

		if (!hasSelectedPiece && sf::FloatRect(boardPosition, { squareSize * 8, squareSize * 8 }).contains(mousePos)) //if clicked on the chess board and NO pieces were selected BEFORE this click
		{
			fromCell = { (unsigned)(mousePos.y - boardPosition.y) / squareSize, (unsigned)(mousePos.x - boardPosition.x) / squareSize }; //fromCell to the clicked cell

			if (is_player_white)
				fromCell = { fromCell.first, 7 - fromCell.second };	//adjust for the board direction if white
			else
				fromCell = { 7 - fromCell.first, fromCell.second }; //if black

			if (!pBoard->getCellPtr(fromCell) || pBoard->isWhitesMove() != pBoard->getCellPtr(fromCell)->white || is_player_white != pBoard->getCellPtr(fromCell)->white)
				return NoSignal; //if the clicked cell isn't valid to select

			hasSelectedPiece = true; //if the clicked cell is valid, set the corresponding flag to true

			return NoSignal;
		}

		else if (hasSelectedPiece) //if some piece was selected BEFORE this click
		{

			if (!sf::FloatRect(boardPosition, { squareSize * 8, squareSize * 8 }).contains(mousePos)) {
				hasSelectedPiece = false;
				return ToPassToUI;					//if clicked not on the board, unselect
			}

			toCell = { (unsigned)((mousePos.y - boardPosition.y) / squareSize), (unsigned)((mousePos.x - boardPosition.x) / squareSize) }; //clicked cell
			if (is_player_white)
				toCell = { toCell.first, 7 - toCell.second };
			else
				toCell = { 7 - toCell.first,toCell.second };


			if (pBoard->getCellPtr(toCell) && pBoard->getCellPtr(toCell)->white == pBoard->getCellPtr(fromCell)->white) {
				fromCell = toCell;
				return ToPassToUI;		//if there is a piece AND it's the same color as selected (the move's color), SELECT it INSTEAD, and pass to UI
			}

			return ToTryMove; //A move needs to be Sent

		}

		return ToPassToUI; // No piece selected, clicked not on the board. Pass to UI.
	}
	
	bool tryMove()
	{
		try
		{
			Move move = { fromCell, toCell, getPieceType(*pBoard, fromCell), getPieceType(*pBoard, toCell) };
			if (((playMode == LocalNetwork || playMode == OfficialServer) ? pRoom->sendMove(move) : pBoard->makeMove(move)))
			{
				hasSelectedPiece = false;

				return true;
			}
		}
		catch (std::exception) 
		{
			std::cerr << "\n\nDuring SendLastMove() an Error Occured!\n\n";
			//_exit(404);
		}

		return false;
	}

	//true = undo last move
	//false = redo last undone move
	bool requestCancelMove(const bool& undo) 
	{
		try
		{
			Move move = { fromCell, toCell, getPieceType(*pBoard, fromCell), getPieceType(*pBoard, toCell) };
			if (((playMode == LocalNetwork || playMode == OfficialServer) ? pRoom->requestCancelLastMove() : pBoard->cancelLastMove(undo)))
			{
				hasSelectedPiece = false;

				return true;
			}
		}
		catch (std::exception)
		{
			std::cerr << "\n\nDuring requestCancelMove() an Error Occured!\n\n";
			//_exit(404);
		}

		return false;
	}

	//display the chessboard state on UI
	void updateUI()
	{
		for (auto& line : squares)
			for (auto& square : line)
				window.draw(square);
		pBoard->updateUI(is_player_white, boardPosition, squareSize, window);//TODO:: optimize?
		if (hasSelectedPiece)
			pBoard->drawMovesOfSelected(is_player_white, fromCell, boardPosition, squareSize, window);
	}

	// TODO:: painfully, the main menu... 
	short mainMenu() {
		sf::Event event;
		sf::Text color, isHost, playModeText;
		color.setString(is_player_white ? "White" : "Black");
		isHost.setString(is_host ? "Host" : "Join");
		color.setFillColor(sf::Color::White);
		isHost.setFillColor(sf::Color::White);
		playModeText.setFillColor(sf::Color::White);
		sf::Font font;
		loadFontFromResource(font, 200);
		playModeText.setFont(font);
		color.setFont(font);
		isHost.setFont(font);
		playModeText.setCharacterSize(60);
		isHost.setCharacterSize(40);
		color.setCharacterSize(40);
		playModeText.setPosition((window.getSize().x - playModeText.getGlobalBounds().getSize().x) / 2.f, window.getSize().y/8.f);
		isHost.setPosition((window.getSize().x - isHost.getGlobalBounds().getSize().x) / 2.f, ((window.getSize().y / 2.f) - isHost.getGlobalBounds().getSize().y) / 2.f + (window.getSize().y / 8.f));
		color.setPosition((window.getSize().x - color.getGlobalBounds().getSize().x) / 2.f, (((window.getSize().y / 2.f) - color.getGlobalBounds().getSize().y) / 2.f) + (window.getSize().y / 4.f));

		while (window.isOpen()) {
			while (window.pollEvent(event)) {
				if (event.type == sf::Event::Closed)
				{
					//TODO:: closing procedure
					window.close();
				}
				if (event.type == sf::Event::KeyPressed) {
					if (event.key.code == sf::Keyboard::Tab) {
						++playMode %= 3;
					}
					if (event.key.code == sf::Keyboard::H) {
						is_host = true;
						isHost.setString("Host");
					}
					else if (event.key.code == sf::Keyboard::J) {
						is_host = false;
						isHost.setString("Join");
					}
					else if (event.key.code == sf::Keyboard::B) {
						is_player_white = false;
						color.setString("Black");
					}
					else if (event.key.code == sf::Keyboard::W) {
						is_player_white = true;
						color.setString("White");
					}
					else if (event.key.code == sf::Keyboard::Enter) 
					{

						switch (playMode)
						{
						case Bot:
							pBoard = new ChessBoard(); // Does ts free the previous pBoard if anything? IDK and for now IDC
							return Bot;
						case SingleScreen:
							pBoard = new ChessBoard();
							if (!is_player_white)
								toggleCurrentPlayersColor(); 
							return SingleScreen;
						case LocalNetwork:
							pRoom.reset(new Room("", "", is_host, is_player_white));
							pBoard = &pRoom->board;
							return LocalNetwork;
						case OfficialServer:
							//IDK for now
							return OfficialServer;
						}

					}

				}
			}

			window.clear(sf::Color::Black);

			switch (playMode) 
			{
			case Bot:
				playModeText.setString("Play with a Bot");
				break;
			case SingleScreen:
				playModeText.setString("Play on Single Screen");
				break;
			case LocalNetwork:
				playModeText.setString("Play on Local Network");
				window.draw(color);
				window.draw(isHost);
				break;
			case OfficialServer:
				//IDK for now
				break;
			}

			playModeText.setPosition((window.getSize().x - playModeText.getGlobalBounds().getSize().x) / 2.f, window.getSize().y / 8.f);
			window.draw(playModeText);

			window.display();
		}

		
	}

	//idk why would you need ts
	void toggleCurrentPlayersColor() {
		is_player_white = !is_player_white;
		//return i
	}
};