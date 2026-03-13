#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <set>
#include <stack>
#include "ChessPiece.hpp"
#include <windows.h>

void loadTextureFromResource(int resourceID, sf::Texture& texture) {
	HRSRC res = FindResource(NULL, MAKEINTRESOURCE(resourceID), RT_RCDATA);
	HGLOBAL resHandle = LoadResource(NULL, res);
	DWORD size = SizeofResource(NULL, res);
	void* data = LockResource(resHandle);

	if (!texture.loadFromMemory(data, size)) {
		throw std::runtime_error("Не вдалося завантажити текстуру з ресурсів!");
	}
}


class ChessBoard {
private:

	std::vector<std::vector<PieceType>> beginningBoard;
	std::vector<std::vector<ChessPiece*>> playBoard;
	std::set<ChessPiece*> pieces;//it's to keep track of the memory and easier moves calculation
	std::stack<Move> moves; //TODO:: make the game remember the moves and display them
	std::vector<Move> illegalBlackMoves; //TODO:: make the game remember the moves and display them
	std::vector<Move> illegalWhiteMoves; //TODO:: make the game remember the moves and display them
	std::set<std::pair<unsigned, unsigned>> attackedByBlackCells;
	std::set<std::pair<unsigned, unsigned>> attackedByWhiteCells;

	std::pair<sf::Texture, sf::Texture> pawn;
	std::pair<sf::Texture, sf::Texture> king;
	std::pair<sf::Texture, sf::Texture> queen;
	std::pair<sf::Texture, sf::Texture> bishop;
	std::pair<sf::Texture, sf::Texture> knight;
	std::pair<sf::Texture, sf::Texture> rook;

	std::pair<unsigned,unsigned> white_king;
	std::pair<unsigned, unsigned> black_king;

	bool isWhitesTurn = true;
	bool whiteUnderCheck = false;
	bool blackUnderCheck = false;
	bool canWhiteCastleQueenSide = true;
	bool canWhiteCastleKingSide = true;
	bool canBlackCastleQueenSide = true;
	bool canBlackCastleKingSide = true;

	void revertMove(const Move& move, bool wasLastWhite, bool no_checks = false) {
		const unsigned& i_from = move.fromPos.first, & j_from = move.fromPos.second, & i_to = move.toPos.first, & j_to = move.toPos.second;
		if(no_checks)
			if (
				playBoard[i_from][j_from] || !playBoard[i_to][j_to] || move.ptFrom == EmptyCell ||
				playBoard[i_to][j_to]->getType() != move.ptFrom
				)
				return;
			
		playBoard[i_from][j_from] = playBoard[i_to][j_to];
		playBoard[i_to][j_to] = NULL;
		
		if (move.ptTo == Castling)
		{
			revertMove((move.toPos.second == 1 ? Move({ {move.toPos.first, 0}, {move.toPos.first, 2}, Rook, EmptyCell }) : Move({ {move.toPos.first, 7}, {move.toPos.first, 4}, Rook, EmptyCell })), wasLastWhite);
		}
		else if (move.ptTo == EnPassant)
		{

		}
		else if (move.ptTo != EmptyCell)
		{
			playBoard[i_to][j_to] = new ChessPiece(move.ptTo, !wasLastWhite);
			pieces.insert(playBoard[i_to][j_to]);
		}

		//TODO:: additional logic for castling AND en passant
	}

	bool isMoveValid(const Move& move) {
		if (move.fromPos == move.toPos)
			return false;

		const unsigned& i_from = move.fromPos.first, &j_from = move.fromPos.second;
		const unsigned& i_to = move.toPos.first, &j_to = move.toPos.second;

		if (i_from >= playBoard.size() ||
			j_from >= playBoard[i_from].size() ||
			i_to >= playBoard.size() ||
			j_to >= playBoard[i_to].size() ||		//check for an incorrect positions
			!playBoard[i_from][j_from])
			return false;
		
		if (isWhitesTurn && !playBoard[i_from][j_from]->white ||
			!isWhitesTurn && playBoard[i_from][j_from]->white)
			return false;

		bool result = false;
		for (const auto& posibbleMove : playBoard[i_from][j_from]->getPieceMoves())
			if (posibbleMove == move.toPos) {
				result = true;
				break;
			}
		if (!result)
			return false;


		if (playBoard[i_to][j_to]) {
			delete playBoard[i_to][j_to];
			pieces.erase(playBoard[i_to][j_to]);
		}
		playBoard[i_to][j_to] = playBoard[i_from][j_from];
		playBoard[i_from][j_from] = NULL;
		if (move.ptFrom == King)
			(playBoard[i_to][j_to]->white ? white_king : black_king) = { i_to, j_to };

		updateAllMoves();
		if (isWhitesTurn) {
			for (const auto& cell : attackedByBlackCells) {
				if (white_king == cell) 
				{
					revertMove(move, true);
					updateAllMoves();

					if (move.ptFrom == King)
						white_king = { i_from, j_from };
					
					return false;
				}
			}
		}
		else {
			for (const auto& cell : attackedByWhiteCells) {
				if (black_king == cell) 
				{
					revertMove(move, false);
					updateAllMoves();

					if (move.ptFrom == King)
						black_king = { i_from, j_from };

					return false;
				}
			}
		}
		revertMove(move, isWhitesTurn);
		updateAllMoves();

		if (move.ptFrom == King)
			(playBoard[i_from][j_from]->white ? white_king : black_king) = { i_from, j_from };

		

			//TODO:: additional logic for castling AND en passant

		return  true;
	}

	/*void updateEnPassant(const Move& madeMove) {
		if (madeMove.ptTo == Pawn && madeMove.fromPos.first == )
			
	}*/
	void updateCastlingFlags(const Move& madeMove) {
		if (!(canBlackCastleQueenSide || canBlackCastleKingSide || canWhiteCastleKingSide || canWhiteCastleQueenSide))
			return;


		if (madeMove.ptFrom == King) {
			if(madeMove.toPos == white_king)
				canWhiteCastleKingSide = canWhiteCastleQueenSide = false;
			else if (madeMove.toPos == black_king)
				canBlackCastleKingSide = canBlackCastleQueenSide = false;

			return;
		}

		if (madeMove.ptFrom == Rook) {
			if (madeMove.fromPos == std::pair<unsigned, unsigned>({ 0,7 })) {
				canBlackCastleQueenSide = false;
				return;
			}
			if (madeMove.fromPos == std::pair<unsigned, unsigned>({ 0,0 })) {
				canBlackCastleKingSide = false;
				return;
			}
			if (madeMove.fromPos == std::pair<unsigned, unsigned>({ 7,7 })) {
				canWhiteCastleQueenSide = false;
				return;
			}
			if (madeMove.fromPos == std::pair<unsigned, unsigned>({ 7,0 })) {
				canWhiteCastleKingSide = false;
				return;
			}
		}
	}

	void updateCastlingMoves() {
		if (!(canBlackCastleQueenSide || canBlackCastleKingSide || canWhiteCastleKingSide || canWhiteCastleQueenSide))
			return;

		if (!blackUnderCheck) 
		{
			if (canBlackCastleKingSide && !playBoard[0][1] && !playBoard[0][2] && attackedByWhiteCells.find({ 0,1 }) == attackedByWhiteCells.end() && attackedByWhiteCells.find({ 0,2 }) == attackedByWhiteCells.end())
				playBoard[black_king.first][black_king.second]->getPieceMoves().push_back({ 0,1 });
			if (canBlackCastleQueenSide && !playBoard[0][4] && !playBoard[0][5] && !playBoard[0][6] && attackedByWhiteCells.find({ 0,4 }) == attackedByWhiteCells.end() && attackedByWhiteCells.find({ 0,5 }) == attackedByWhiteCells.end() && attackedByWhiteCells.find({ 0,6 }) == attackedByWhiteCells.end())
				playBoard[black_king.first][black_king.second]->getPieceMoves().push_back({ 0,5 });
		}

		if (!whiteUnderCheck)
		{
			if (canWhiteCastleKingSide && !playBoard[7][1] && !playBoard[7][2] && attackedByBlackCells.find({ 7,1 }) == attackedByBlackCells.end() && attackedByBlackCells.find({ 7,2 }) == attackedByBlackCells.end())
				playBoard[white_king.first][white_king.second]->getPieceMoves().push_back({ 7,1 });
			if (canWhiteCastleQueenSide && !playBoard[7][4] && !playBoard[7][5] && !playBoard[7][6] && attackedByBlackCells.find({ 7,4 }) == attackedByBlackCells.end() && attackedByBlackCells.find({ 7,5 }) == attackedByBlackCells.end() && attackedByBlackCells.find({ 7,6 }) == attackedByBlackCells.end())
				playBoard[white_king.first][white_king.second]->getPieceMoves().push_back({ 7,5 });
		}


	}

	void updateAllMoves(){
		attackedByBlackCells.clear();
		attackedByWhiteCells.clear();

		for (int i = 0, size = playBoard.size(); i < size; i++)
			for (int j = 0; j < size; j++)
				if (playBoard[i][j])
				{
					playBoard[i][j]->updatePieceMoves(playBoard, { i,j });

					for (auto attackedCell : playBoard[i][j]->getAttackedSquares())
						(playBoard[i][j]->white ? attackedByWhiteCells : attackedByBlackCells).insert(attackedCell);
				}
	}

	void updatePossibleMoves() {
		illegalWhiteMoves.clear();
		illegalBlackMoves.clear();

	
		for (int i = 0, size = playBoard.size(); i < size; i++)
			for (int j = 0; j < size; j++)
				if (playBoard[i][j])
				{
					auto illegalMoves = playBoard[i][j]->getPieceMoves();

					for (auto it = illegalMoves.begin(); it != illegalMoves.end(); ) {
						if (isMoveValid({ {i,j}, *it, getPieceType(*this,{i,j}), getPieceType(*this,*it) }))
							it = illegalMoves.erase(it);
						else
							it++;
					}

					playBoard[i][j]->setPossibleMoves(illegalMoves);

					auto& allowedMoves = playBoard[i][j]->white ? illegalWhiteMoves : illegalBlackMoves;
					for (auto& moveTo : illegalMoves)
						allowedMoves.push_back({ {i, j}, moveTo });
				}
	}
	
	void updateChecks(){
		if (isWhitesTurn) {
			whiteUnderCheck = false;
			for (const auto& cell : attackedByBlackCells) {
				if (white_king == cell) {
					whiteUnderCheck = true;
					break;
				}
			}
		}
		else {
			blackUnderCheck = false;
			for (const auto& cell : attackedByWhiteCells) {
				if (black_king == cell) {
					blackUnderCheck = true;
					break;
				}
			}
		}
	}


public:
	friend class ChessPiece;
	//basic board
	ChessBoard() :beginningBoard(8, std::vector<PieceType>(8, EmptyCell)), playBoard(8, std::vector<ChessPiece*>(8, NULL)) {
		//initialize standard chess game
		int pieceCounter = 0;

		for (int i = 0; i < 8; i++) {
			/*beginningBoard[6][i] = */playBoard[6][i] = new ChessPiece(Pawn, true);
			pieces.insert(playBoard[6][i]);
		}
		for (int i = 0; i < 8; i++) {
			/*beginningBoard[1][i] = */playBoard[1][i] = new ChessPiece(Pawn, false);
			pieces.insert(playBoard[1][i]);
		}
		for (int i = 0; i < 2; i++) {
			/*beginningBoard[7][1 + i * 5] = */playBoard[7][1 + i * 5] = new ChessPiece(Knight, true);
			pieces.insert(playBoard[7][1 + i * 5]);
		}
		for (int i = 0; i < 2; i++) {
			/*beginningBoard[0][1 + i * 5] = */playBoard[0][1 + i * 5] = new ChessPiece(Knight, false);
			pieces.insert(playBoard[0][1 + i * 5]);
		}
		for (int i = 0; i < 2; i++) {
			/*beginningBoard[7][2 + i * 3] = */playBoard[7][2 + i * 3] = new ChessPiece(Bishop, true);
			pieces.insert(playBoard[7][2 + i * 3]);
		}
		for (int i = 0; i < 2; i++) {
			/*beginningBoard[0][2 + i * 3] = */playBoard[0][2 + i * 3] = new ChessPiece(Bishop, false);
			pieces.insert(playBoard[0][2 + i * 3]);
		}
		for (int i = 0; i < 2; i++) {
			/*beginningBoard[7][i * 7] = */playBoard[7][i * 7] = new ChessPiece(Rook, true);
			pieces.insert(playBoard[7][i * 7]);
		}
		for (int i = 0; i < 2; i++){
			/*beginningBoard[0][i * 7] = */playBoard[0][i * 7] = new ChessPiece(Rook, false);
			pieces.insert(playBoard[0][i * 7]);
		}
		/*beginningBoard[0][4] = */playBoard[0][4] = new ChessPiece(Queen, false);
		pieces.insert(playBoard[0][4]);
		/*beginningBoard[7][4] = */playBoard[7][4] = new ChessPiece(Queen, true);
		pieces.insert(playBoard[7][4]);
		/*beginningBoard[0][3] = */playBoard[0][3] = new ChessPiece(King, false);
		pieces.insert(playBoard[0][3]);
		/*beginningBoard[7][3] = */playBoard[7][3] = new ChessPiece(King, true);
		pieces.insert(playBoard[7][3]);
		black_king = { 0,3 };
		white_king = { 7,3 };

		updateAllowedMoves();


		//UI

		loadTextureFromResource(110, king.first);
		loadTextureFromResource(111, queen.first);
		loadTextureFromResource(112, rook.first);
		loadTextureFromResource(113, bishop.first);
		loadTextureFromResource(114, knight.first);
		loadTextureFromResource(115, pawn.first);

		loadTextureFromResource(116, king.second);
		loadTextureFromResource(117, queen.second);
		loadTextureFromResource(118, rook.second);
		loadTextureFromResource(119, bishop.second);
		loadTextureFromResource(120, knight.second);
		loadTextureFromResource(121, pawn.second);
	}

	~ChessBoard() {
		for (auto& piece : pieces)
			if (piece)
				delete piece;
	}

	//fancy board
	//ChessBoard(const std::vector<std::vector<PieceType>>& BeginningBoard, const std::vector<Move> Moves):pieces(Pieces.size(),NULL) {
	//	for (unsigned amount = Pieces.size(), i = 0; i < amount; i++) {
	//		//pieces[i] = new;
	//	}
	//}

	bool isWhitesMove() { return isWhitesTurn; }

	ChessPiece*& getCellPtr(std::pair<unsigned, unsigned> position) {
		return playBoard[position.first][position.second];
	}

	void updateAllowedMoves() {
		updateAllMoves();
		updatePossibleMoves();
		updateChecks();
		updateCastlingMoves();
		//TODO::check for mates
	}

	std::vector<std::pair<unsigned, unsigned>> getPossibleMoves(std::pair<unsigned, unsigned> pos) {
		const unsigned& i = pos.first, &j = pos.second;
		if (playBoard[i][j])
			return playBoard[i][j]->getPieceMoves();

		return std::vector<std::pair<unsigned, unsigned>>();
	}

	bool makeMove(Move& move) {
		const unsigned& i_from = move.fromPos.first, & j_from = move.fromPos.second;
		const unsigned& i_to = move.toPos.first, & j_to = move.toPos.second;

		if (move.ptFrom == Pawn)
		{
			if (std::abs(((int)i_from) - ((int)i_to)) == 2)
			{
				
				move.ptTo = EnPassant;
			}
			else if (i_to == 7 || i_to == 0) {
				move.ptTo = Promotion;
			}
		}

		if (move.ptFrom == King && std::abs( ((int)j_from) - ((int)j_to) ) == 2 )
		{
			auto& movesVec = playBoard[i_from][j_from]->getPieceMoves();
			if (std::find(movesVec.begin(), movesVec.end(), move.toPos) == movesVec.end())
				return false;
			move.ptTo = Castling;
			revertMove((j_to == 1 ? Move({ {i_to, 2} ,{i_to, 0}, Rook, EmptyCell }) : Move({ {i_to, 4}, {i_to, 7}, Rook, EmptyCell })), true /*it doesn't matter which color's is move for castling*/ );
		}

		if (!isMoveValid(move))
			return false;
		
		
		if (playBoard[i_to][j_to]) {
			delete playBoard[i_to][j_to];
			pieces.erase(playBoard[i_to][j_to]);
		}
		playBoard[i_to][j_to] = playBoard[i_from][j_from];
		playBoard[i_from][j_from] = NULL;
		if (playBoard[i_to][j_to]->getType() == King) {
			(playBoard[i_to][j_to]->white ? white_king : black_king) = { i_to, j_to };
		}

		isWhitesTurn = !isWhitesTurn;

		
		updateCastlingFlags(move);
		updateAllowedMoves();

		//additional logic en passant
		if (move.ptFrom == Pawn && std::abs(((int)i_from) - ((int)i_to)) == 2) {
			if (j_to - 1 <= 7 && playBoard[i_to][j_to - 1] && playBoard[i_to][j_to - 1]->getType() == Pawn && playBoard[i_to][j_to - 1]->white == isWhitesTurn)
				playBoard[i_to][j_to - 1]->getPieceMoves().push_back({ i_to + ((((int)i_from) - ((int)i_to)) / 2), j_from });
			if (j_to + 1 <= 7 && playBoard[i_to][j_to + 1] && playBoard[i_to][j_to + 1]->getType() == Pawn && playBoard[i_to][j_to + 1]->white == isWhitesTurn)
				playBoard[i_to][j_to + 1]->getPieceMoves().push_back({ i_to + ((((int)i_from) - ((int)i_to)) / 2), j_from });
		}
		

		return true;
	}
	
	void updateUI(bool is_player_white/*NIGGA*/, const sf::Vector2f& boardPosition, const float& squareSize, sf::RenderTarget& target) {
		float pictureSize = 60.f;

		for (int i = 0; i < playBoard.size(); i++)
			for (int j = 0; j < playBoard[i].size(); j++)
				if (playBoard[i][j]) {
					auto& piece = playBoard[i][j];
					sf::Sprite sprite;
					

					switch (piece->getType()) 
					{
					case Pawn:
						if (piece->white)
							sprite.setTexture(pawn.second);
						else
							sprite.setTexture(pawn.first);
						break;
					case King:
						if (piece->white)
							sprite.setTexture(king.second);
						else
							sprite.setTexture(king.first);
						break;
					case Queen:
						if (piece->white)
							sprite.setTexture(queen.second);
						else
							sprite.setTexture(queen.first);
						break;
					case Bishop:
						if (piece->white)
							sprite.setTexture(bishop.second);
						else
							sprite.setTexture(bishop.first);
						break;
					case Knight:
						if (piece->white)
							sprite.setTexture(knight.second);
						else
							sprite.setTexture(knight.first);
						break;
					case Rook:
						if (piece->white)
							sprite.setTexture(rook.second);
						else
							sprite.setTexture(rook.first);
						break;
					}

					auto size = sprite.getGlobalBounds().getSize();
					sprite.setScale(squareSize / size.x, squareSize / size.y);
					sprite.setPosition(boardPosition + sf::Vector2f({ (is_player_white ? (7 - j) : j) * squareSize,   (!is_player_white ? (7 - i) : i) * squareSize }));
					if (is_player_white) {
						//sprite.rotate(180);
					}

					target.draw(sprite);
				}
	}

	void drawMovesOfSelected(bool is_player_white/*NIGGA*/, const std::pair<unsigned, unsigned>& fromCell, const sf::Vector2f& boardPosition, const float& squareSize, sf::RenderTarget& target) {
		const unsigned &i = fromCell.first, &j  = fromCell.second;

		if (playBoard[i][j]) 
		{
			auto fromPos = std::pair<unsigned, unsigned>({ i,j });
			sf::Vector2f selectedSquarePosition = boardPosition + sf::Vector2f({ (is_player_white ? (7 - j) : j) * squareSize,   (!is_player_white ? (7 - i) : i) * squareSize });

			sf::CircleShape circle(squareSize / 4.5f);
			sf::RectangleShape outline({ squareSize, squareSize });
			outline.setFillColor({ 0,0,0,0 });
			outline.setOutlineColor({ 0,200,0 });
			outline.setOutlineThickness({ -4 });
			outline.setPosition(selectedSquarePosition);
			target.draw(outline);
			
			float gap = (squareSize - 2 * circle.getRadius()) / 2.f;
			circle.setOrigin(-gap, -gap);

			for (const auto& moveSquare : playBoard[i][j]->getPieceMoves()) {
				
				const unsigned& move_i = moveSquare.first, & move_j = moveSquare.second;
				sf::Vector2f position = boardPosition + sf::Vector2f({ (is_player_white ? (7 - move_j) : move_j) * squareSize,   (!is_player_white ? (7 - move_i) : move_i) * squareSize });

				circle.setPosition(position);
				circle.setFillColor(sf::Color(180, 0, 0, 100));
				for (auto& illegalMove : (isWhitesTurn ? illegalWhiteMoves : illegalBlackMoves)) 
					if (illegalMove.fromPos == fromPos && illegalMove.toPos == moveSquare)
					{
						circle.setFillColor({ 0, 0, 0, 0 });
						break;
					}
				
				target.draw(circle);

			}
		}
	}
	
	friend PieceType getPieceType(const ChessBoard& board, std::pair<unsigned, unsigned> position);
};

PieceType getPieceType(const ChessBoard& board, std::pair<unsigned, unsigned> position) {
	if (board.playBoard[position.first][position.second])
		return board.playBoard[position.first][position.second]->getType();

	return EmptyCell;
}