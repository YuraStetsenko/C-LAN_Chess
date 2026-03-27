#pragma once

#include "Room.hpp"

#include <SFML/Graphics.hpp>
#include <Windows.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream>

namespace myChess {

inline bool loadFontFromResource(sf::Font& font, int resourceId) {
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (!hRes) return false;
    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) return false;
    void* pData = LockResource(hData);
    DWORD dataSize = SizeofResource(NULL, hRes);
    if (!pData || dataSize == 0) return false;
    return font.loadFromMemory(pData, dataSize);
}

inline std::map<PieceType, char> mapPieceToFEN{
    {Pawn, 'P'}, {Knight, 'N'}, {Bishop, 'B'}, {Rook, 'R'}, {Queen, 'Q'}, {King, 'K'}
};
inline std::map<char, PieceType> mapLetterToPieceType{
    {'P', Pawn}, {'N', Knight}, {'B', Bishop}, {'R', Rook}, {'Q', Queen}, {'K', King}
};
inline std::map<int, std::string> mapFileIndexToLetter{
    {7, "a"}, {6, "b"}, {5, "c"}, {4, "d"}, {3, "e"}, {2, "f"}, {1, "g"}, {0, "h"}
};
inline std::map<char, int> mapLetterToFileIndex{
    {'a', 7}, {'b', 6}, {'c', 5}, {'d', 4}, {'e', 3}, {'f', 2}, {'g', 1}, {'h', 0}
};

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
        ToRedrawUI = 2,
        ToProcessGUI = 3
    };

private:
    sf::RenderWindow& window;
    std::unique_ptr<Room> pRoom;
    ChessBoard* pBoard = nullptr;
    float squareSize = 0.f;
    sf::Vector2f boardPosition;
    std::vector<std::vector<sf::RectangleShape>> squares;
    bool is_host = true;
    bool is_player_white = true;
    bool hasSelectedPiece = false;
    std::pair<unsigned, unsigned> toCell;
    std::pair<unsigned, unsigned> fromCell;
    short playMode = SingleScreen;
	std::atomic<bool> to_updateUI_bot{ false };

public:
    ChessApp() = delete;

    ChessApp(sf::RenderWindow& Window)
        : window(Window) {
        squareSize = std::min(window.getSize().x, window.getSize().y) / 12.f;
        sf::RectangleShape initRect({ squareSize, squareSize });
        squares = std::vector<std::vector<sf::RectangleShape>>(8, std::vector<sf::RectangleShape>(8, initRect));
        boardPosition = { ((window.getSize().x / 2.f) - 8 * squareSize) / 2.f,
                          (window.getSize().y - 8 * squareSize) / 2.f };

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
        if (playMode == LocalNetwork || playMode == OfficialServer)
            return;
        delete pBoard;
    }

    //TODO:: make this fucntion return void?
    Signal handleClick(sf::Event& event, bool gameIsOn = true) {
        sf::Vector2f mousePos({ (float)event.mouseButton.x, (float)event.mouseButton.y });

        if (!hasSelectedPiece && sf::FloatRect(boardPosition, { squareSize * 8, squareSize * 8 }).contains(mousePos)) {
            fromCell = { (unsigned)(mousePos.y - boardPosition.y) / squareSize,
                         (unsigned)(mousePos.x - boardPosition.x) / squareSize };

            if (is_player_white)
                fromCell = { fromCell.first, 7 - fromCell.second };
            else
                fromCell = { 7 - fromCell.first, fromCell.second };

            if (!pBoard->getCellPtr(fromCell)
                || pBoard->isWhitesMove() != pBoard->getCellPtr(fromCell)->white
                || is_player_white != pBoard->getCellPtr(fromCell)->white)
                return NoSignal;

            hasSelectedPiece = true;

            window.clear(sf::Color::Black);
            updateUI(); //TODO:: Only call updateUI when it's triggered by user or server
            window.display();

            return ToRedrawUI;
        }
        else if (hasSelectedPiece) {
            if (!sf::FloatRect(boardPosition, { squareSize * 8, squareSize * 8 }).contains(mousePos)) {
                hasSelectedPiece = false;

                window.clear(sf::Color::Black);
                updateUI(); //TODO:: Only call updateUI when it's triggered by user or server
                window.display();

                return ToRedrawUI;
            }

            toCell = { (unsigned)((mousePos.y - boardPosition.y) / squareSize),
                       (unsigned)((mousePos.x - boardPosition.x) / squareSize) };

            if (is_player_white)
                toCell = { toCell.first, 7 - toCell.second };
            else
                toCell = { 7 - toCell.first, toCell.second };

            if (pBoard->getCellPtr(toCell)
                && pBoard->getCellPtr(toCell)->white == pBoard->getCellPtr(fromCell)->white) {
                fromCell = toCell;

                window.clear(sf::Color::Black);
                updateUI(); //TODO:: Only call updateUI when it's triggered by user or server
                window.display();

                return ToRedrawUI;
            }

            return ToTryMove;
        }

        return ToProcessGUI;
    }

    bool tryMove() {
        try {
            Move move = { fromCell, toCell, getPieceType(*pBoard, fromCell), getPieceType(*pBoard, toCell) };
            if (((playMode == LocalNetwork || playMode == OfficialServer) ? pRoom->sendMove(move) : pBoard->makeMove(move))) {
                hasSelectedPiece = false;

                window.clear(sf::Color::Black);
                updateUI(); //TODO:: Only call updateUI when it's triggered by user or server
                window.display();

                return true;
            }
        }
        catch (std::exception&) {
            std::cerr << "\n\nDuring SendLastMove() an Error Occured!\n\n";
        }
        return false;
    }

    bool requestCancelMove(const bool& undo) {
        try {
            if (((playMode == LocalNetwork || playMode == OfficialServer)
                    ? pRoom->requestCancelLastMove()
                    : pBoard->cancelLastMove(undo))) {
                hasSelectedPiece = false;

                window.clear(sf::Color::Black);
                updateUI(); //TODO:: Only call updateUI when it's triggered by user or server
                window.display();

                return true;
            }
        }
        catch (std::exception&) {
            std::cerr << "\n\nDuring requestCancelMove() an Error Occured!\n\n";
        }
        return false;
    }

    void updateUI() {
        for (auto& line : squares)
            for (auto& square : line)
                window.draw(square);

        pBoard->updateUI(is_player_white, boardPosition, squareSize, window);
        if (hasSelectedPiece)
            pBoard->drawMovesOfSelected(is_player_white, fromCell, boardPosition, squareSize, window);
    }

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

        while (window.isOpen()) {
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed)
                    window.close();

                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Tab) {
                        ++playMode %= 4;
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
                    else if (event.key.code == sf::Keyboard::Enter) {
                        switch (playMode) {
                        case Bot:
                            pBoard = new ChessBoard();
                            return Bot;

                        case SingleScreen:
                            pBoard = new ChessBoard();
                            if (!is_player_white)
                                toggleCurrentPlayersColor();
                            return SingleScreen;

                        case LocalNetwork:
                            pRoom.reset(new Room("", "", is_host, is_player_white, Room::Mode::LocalPeerToPeer));
                            pBoard = &pRoom->board;
                            return LocalNetwork;

                        case OfficialServer:
                            pRoom.reset(new Room("player", "", false, true, Room::Mode::OfficialServer));
                            pBoard = &pRoom->board;
                            return OfficialServer;
                        }
                    }
                }
            }

            window.clear(sf::Color::Black);
            switch (playMode) {
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
                playModeText.setString("Play on Official Server");
                break;
            }

            playModeText.setPosition((window.getSize().x - playModeText.getGlobalBounds().getSize().x) / 2.f,
                                     window.getSize().y / 8.f);
            isHost.setPosition((window.getSize().x - isHost.getGlobalBounds().getSize().x) / 2.f,
                               ((window.getSize().y / 2.f) - isHost.getGlobalBounds().getSize().y) / 2.f + (window.getSize().y / 8.f));
            color.setPosition((window.getSize().x - color.getGlobalBounds().getSize().x) / 2.f,
                              (((window.getSize().y / 2.f) - color.getGlobalBounds().getSize().y) / 2.f) + (window.getSize().y / 4.f));

            window.draw(playModeText);
            window.display();
        }

        return SingleScreen;
    }
    
    bool isWhitesMove() const {
        return pBoard->isWhitesMove();
	}

    bool getCurrentPlayersColor() const {
        return is_player_white;
	}

    void toggleCurrentPlayersColor() {
        is_player_white = !is_player_white;

        window.clear(sf::Color::Black);
        updateUI(); //TODO:: Only call updateUI when it's triggered by user or server
        window.display();
    }

    std::string getCurrentFEN() {
        std::string fen;
        fen.reserve(100);

        for (int i = 0, emptySquaresCount; i < 8; ++i) {
            emptySquaresCount = 0;
            for (int j = 7; j >= 0; j--) {
                auto& pPiece = pBoard->getCellPtr({ i, j });
                if (pPiece) {
                    if (emptySquaresCount) {
                        fen += std::to_string(emptySquaresCount);
                        emptySquaresCount = 0;
                    }
                    fen += pPiece->white ? mapPieceToFEN[pPiece->getType()] : std::tolower(mapPieceToFEN[pPiece->getType()]);
                }
                else {
                    ++emptySquaresCount;
                }
            }
            if (emptySquaresCount)
                fen += std::to_string(emptySquaresCount);
            fen += "/";
        }
        fen.pop_back();

        const auto& flags = pBoard->crefFlags;
        fen += flags.isWhitesTurn ? " w " : " b ";
        if (!flags.canWhiteCastleKingSide && !flags.canWhiteCastleQueenSide
            && !flags.canBlackCastleKingSide && !flags.canBlackCastleQueenSide) {
            fen += "- ";
        }
        else {
            if (flags.canWhiteCastleKingSide) fen += "K";
            if (flags.canWhiteCastleQueenSide) fen += "Q";
            if (flags.canBlackCastleKingSide) fen += "k";
            if (flags.canBlackCastleQueenSide) fen += "q";
            fen += " ";
        }

        if (flags.enPassantTo.first)
            fen += mapFileIndexToLetter[flags.enPassantTo.first] + std::to_string(8 - flags.enPassantTo.second);
        else
            fen += "- ";

        fen += std::to_string(flags.halfMoveClock);
        fen += " ";
        fen += std::to_string(flags.fullMoveCount);
        return fen;
    }

    bool makeBotsMove(std::string sMove) {
        if (sMove.empty()) return false;

        std::pair<unsigned, unsigned> fromPos = { static_cast<unsigned>(8 - sMove[1] + '0'), static_cast<unsigned>(mapLetterToFileIndex[sMove[0]]) };
        std::pair<unsigned, unsigned> toPos = { static_cast<unsigned>(8 - sMove[3] + '0'), static_cast<unsigned>(mapLetterToFileIndex[sMove[2]]) };
        Move move = { fromPos, toPos, getPieceType(*pBoard, fromPos), getPieceType(*pBoard, toPos) };

        if (sMove[4] != '\n')
            pBoard->setNextPromotionType(mapLetterToPieceType[std::toupper(sMove[4])]);

        bool result = pBoard->makeMove(move);

        window.clear(sf::Color::Black);
        updateUI(); //TODO:: Only call updateUI when it's triggered by user or server
        window.display();

        return result;
    }

    std::atomic<bool>& getReffUpdateUI(){ return pRoom->to_updateUI; }
};



} // namespace myChess
