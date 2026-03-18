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
	std::stack<Move> moves;
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
	bool toCheckForEnPassant = false;
	bool areThereLegalMovesLeft = true;

	

	//ONLY use to get a PREVIOUS position. !!!Not supposed to be used instead of makeMove()!!! Update 
	void revertMove(const Move& move, bool wasLastWhite, bool no_checks = true) {
		const unsigned& i_from = move.fromPos.first, & j_from = move.fromPos.second, & i_to = move.toPos.first, & j_to = move.toPos.second;
		if(!no_checks)
			if (
				playBoard[i_from][j_from] || 
				!playBoard[i_to][j_to] || 
				move.ptFrom != EmptyCell ||
				(playBoard[i_to][j_to]->getType() != move.ptFrom && move.ptFrom != Promotion )
			   )
					return;
		
		if (move.ptFrom == Promotion)
		{
			playBoard[i_from][j_from] = new ChessPiece(Pawn, !wasLastWhite);
			
			delete playBoard[i_to][j_to];
			pieces.erase(playBoard[i_to][j_to]);

			playBoard[i_to][j_to] = new ChessPiece(move.ptTo, wasLastWhite);
			pieces.insert(playBoard[i_to][j_to]);

			return;
		}
		else
		{
			playBoard[i_from][j_from] = playBoard[i_to][j_to]; //return the piece back to the starting cell
			playBoard[i_to][j_to] = NULL; //free the square to replace with the piece that was there before or an empty cell if none were.

			if (move.ptFrom == King)
				(playBoard[i_from][j_from]->white ? white_king : black_king) = { i_from , j_from};
		}
	

		if (move.ptTo == Castling)
		{
			//Revert the rook back
			revertMove
			(
				(j_to == 1) ? Move({ {i_to, 0}, {i_to, 2}, Rook, EmptyCell }) : Move({ {i_to, 7}, {i_to, 4}, Rook, EmptyCell }),
				wasLastWhite
			);
		}
		else if (move.ptTo == EnPassant)
		{
			if (playBoard[i_from][j_to]) //This shouldn't ever be true... But still, WHAT IF??
				delete playBoard[i_from][j_to];

			//revive the killed pawn
			playBoard[i_from][j_to] = new ChessPiece(Pawn, !wasLastWhite);
			pieces.insert(playBoard[i_from][j_to]);
		}
		else if (move.ptTo != EmptyCell)
		{
			playBoard[i_to][j_to] = new ChessPiece(move.ptTo, !wasLastWhite);
			pieces.insert(playBoard[i_to][j_to]);
		}
		
	}


	//Marks the move with an appropriate type if it's Castling/En Passant/Promotion
	void handleExceptionTypes(Move& move) 
	{
		const unsigned& i_from = move.fromPos.first, & j_from = move.fromPos.second;
		const unsigned& i_to = move.toPos.first, & j_to = move.toPos.second;

		
		if (move.ptFrom == Pawn)
		{
			//A pawn suddenly changed its column stepping into an empty cell? En Passant detected!
			if (((int)j_from) - ((int)j_to) && move.ptTo == EmptyCell)
				move.ptTo = EnPassant;

			//piece type is changed on "from" position, because we know it's a pawn, but the "to" will be a newly created one
			else if (i_to == 7 || i_to == 0)
				move.ptFrom = Promotion;
		}
		
		//if it's a castling, mark it as so.
		else if (move.ptFrom == King && std::abs(((int)j_from) - ((int)j_to)) == 2)
			move.ptTo = Castling;
	}

	//Applies a move WITHOUT ANY validations
	void applyAssumedMove(const Move& move) 
	{
		const unsigned& i_from = move.fromPos.first, & j_from = move.fromPos.second;
		const unsigned& i_to = move.toPos.first, & j_to = move.toPos.second;
		auto& king = (isWhitesTurn ? white_king : black_king);

		//(kill the piece that standed there if there was one)
		if (playBoard[i_to][j_to]) 
		{
			delete playBoard[i_to][j_to];
			pieces.erase(playBoard[i_to][j_to]); 
		}

		//move the piece from "from" to "to"... 
		playBoard[i_to][j_to] = playBoard[i_from][j_from];
		playBoard[i_from][j_from] = NULL;

		if (move.ptFrom == King)
		{
			king = { i_to, j_to }; //in case we moved a king, save his new position

			if (move.ptTo == Castling)
				revertMove((j_to == 1 ? Move({ {i_to, 2} ,{i_to, 0}, Rook, EmptyCell }) : Move({ {i_to, 4}, {i_to, 7}, Rook, EmptyCell })), true /*it doesn't matter which color's is move for castling made with revertMove*/);
		}

		else if (move.ptTo == EnPassant)
		{
			delete playBoard[i_from][j_to];
			pieces.erase(playBoard[i_from][j_to]); //(kill the attacked pawn)
			playBoard[i_from][j_to] = NULL;
		}
	}

	//ONLY to be used in updateLegalMoves() after calling updatePotentialMoves()
	bool init_validateMove(Move& move) 
	{
		auto& king = (isWhitesTurn ? white_king : black_king);
		const auto& attackedByEnemyCells = (isWhitesTurn ? attackedByBlackCells : attackedByWhiteCells);
		const unsigned& i_from = move.fromPos.first, & j_from = move.fromPos.second;

		//is the selected piece of the moving color?
		if (isWhitesTurn != playBoard[i_from][j_from]->white)
			return false;

		// En Passant/Castling/Promotion type marking
		handleExceptionTypes(move);

		//Castling validation is handled while adding its available moves
		if (move.ptTo == Castling)
			return true;

		//apply the move to see whether the king of the moving color is under attack now, thus making the move invalid
		applyAssumedMove(move);	
		updateAttackedSquares();
		bool result = true;
		if (attackedByEnemyCells.find(king) != attackedByEnemyCells.end())
			result = false;
		
		//revert the board to the initial state
		revertMove(move, isWhitesTurn); 
		
		return result;
	}
	
	//ONLY use after updateBoardState()
	bool validateMove(Move& move)
	{
		//Validate the positions so the server logic can't be tricked even if the move is transmitted as something inappropriate
		const unsigned& i_from = move.fromPos.first, & j_from = move.fromPos.second;
		const unsigned& i_to = move.toPos.first, & j_to = move.toPos.second;

		//are the positions within allowed bounds? + (*)
		if (i_from >= playBoard.size() ||
			j_from >= playBoard[i_from].size() ||
			i_to >= playBoard.size() ||
			j_to >= playBoard[i_to].size() ||
			!playBoard[i_from][j_from])	 //			 (*) is there even a piece on "from"?
			return false;

		//is the selected piece of the moving color?
		if (isWhitesTurn != playBoard[i_from][j_from]->white)
			return false;

		//is the move among avialable ones for the piece?
		const auto& legalMoves = playBoard[i_from][j_from]->getLegalMoves();
		if (std::find(legalMoves.begin(), legalMoves.end(), move.toPos) == legalMoves.end())
			return false;

		handleExceptionTypes(move);

		return true;
	}

	//In case if the latest move was a pawn making a double-step, adds En Passant to potentialMoves of the enemy pawns left and right from it if such exist
	//( ONLY Use After updatePotentialMoves() But Before updateLegalMoves() )
	void updateEnPassant() {
		if (moves.empty())
			return;

		const Move& move = moves.top();
		const unsigned& i_from = move.fromPos.first, & j_from = move.fromPos.second;
		const unsigned& i_to = move.toPos.first, & j_to = move.toPos.second;

		//A pawn took a double-step? En Passant alert!
		if ( !(move.ptFrom == Pawn && std::abs(((int)i_from) - ((int)i_to)) == 2) )
			return;

		std::pair<unsigned, unsigned> enPassant_toPos = { i_to - ((((int)i_to) - ((int)i_from)) / 2), j_from };
		for(short offset = -1; offset <= 1; offset += 2)
			if (
				j_to + offset <= 7 &&
				playBoard[i_to][j_to + offset] &&
				playBoard[i_to][j_to + offset]->getType() == Pawn &&
				playBoard[i_to][j_to + offset]->white != playBoard[i_to][j_to]->white
			   )
			{
				playBoard[i_to][j_to + offset]->getPotentialMoves().insert(enPassant_toPos);
			}


	}

	void updateCastlingFlags() {
		if (moves.empty() || !(canBlackCastleQueenSide || canBlackCastleKingSide || canWhiteCastleKingSide || canWhiteCastleQueenSide))
			return;

		const Move& move = moves.top();

		if (move.ptFrom == King) 
		{
			if(move.toPos == white_king)
				canWhiteCastleKingSide = canWhiteCastleQueenSide = false;
			else if (move.toPos == black_king)
				canBlackCastleKingSide = canBlackCastleQueenSide = false;

			return;
		}

		if (move.ptFrom == Rook) 
		{
			if (canBlackCastleQueenSide && move.fromPos == std::pair<unsigned, unsigned>({ 0,7 })) {
				canBlackCastleQueenSide = false;
				return;
			}
			if (canBlackCastleKingSide && move.fromPos == std::pair<unsigned, unsigned>({ 0,0 })) {
				canBlackCastleKingSide = false;
				return;
			}
			if (canWhiteCastleQueenSide && move.fromPos == std::pair<unsigned, unsigned>({ 7,7 })) {
				canWhiteCastleQueenSide = false;
				return;
			}
			if (canWhiteCastleKingSide && move.fromPos == std::pair<unsigned, unsigned>({ 7,0 })) {
				canWhiteCastleKingSide = false;
				return;
			}
		}
	}

	//ONLY use after updateChecks()
	void updateCastlingMoves() {
		if (!(canBlackCastleQueenSide || canBlackCastleKingSide || canWhiteCastleKingSide || canWhiteCastleQueenSide))
			return;


		//TODO:: possibly shorten this function
		if (!blackUnderCheck) 
		{
			if (
				canBlackCastleKingSide && 
				!playBoard[0][1] && 
				!playBoard[0][2] && 
				attackedByWhiteCells.find({ 0,1 }) == attackedByWhiteCells.end() && 
				attackedByWhiteCells.find({ 0,2 }) == attackedByWhiteCells.end()
				)
					playBoard[black_king.first][black_king.second]->getPotentialMoves().insert({ 0,1 });

			if (
				canBlackCastleQueenSide && 
				!playBoard[0][4] && 
				!playBoard[0][5] && 
				!playBoard[0][6] && 
				attackedByWhiteCells.find({ 0,4 }) == attackedByWhiteCells.end() &&
				attackedByWhiteCells.find({ 0,5 }) == attackedByWhiteCells.end() &&
				attackedByWhiteCells.find({ 0,6 }) == attackedByWhiteCells.end())
					playBoard[black_king.first][black_king.second]->getPotentialMoves().insert({ 0,5 });
		}

		if (!whiteUnderCheck)
		{
			if (
				canWhiteCastleKingSide && 
				!playBoard[7][1] && 
				!playBoard[7][2] && 
				attackedByBlackCells.find({ 7,1 }) == attackedByBlackCells.end() && 
				attackedByBlackCells.find({ 7,2 }) == attackedByBlackCells.end()
				)
					playBoard[white_king.first][white_king.second]->getPotentialMoves().insert({ 7,1 });

			if (
				canWhiteCastleQueenSide && 
				!playBoard[7][4] && 
				!playBoard[7][5] && 
				!playBoard[7][6] && 
				attackedByBlackCells.find({ 7,4 }) == attackedByBlackCells.end() && 
				attackedByBlackCells.find({ 7,5 }) == attackedByBlackCells.end() && 
				attackedByBlackCells.find({ 7,6 }) == attackedByBlackCells.end()
				)
					playBoard[white_king.first][white_king.second]->getPotentialMoves().insert({ 7,5 });
		}

	}

	void updateAttackedSquares()
	{
		attackedByBlackCells.clear();
		attackedByWhiteCells.clear();

		for (int i = 0, size = playBoard.size(); i < size; i++)
			for (int j = 0; j < size; j++)
				if (playBoard[i][j])
				{
					playBoard[i][j]->updatePieceAttackedSquares(playBoard, { i,j });

					auto& attackedByEnemyCells = playBoard[i][j]->white ? attackedByWhiteCells : attackedByBlackCells;
					for (const auto& attackedCell : playBoard[i][j]->getAttackedSquares())
						attackedByEnemyCells.insert(attackedCell);
				}
	}

	//ONLY use after updateAttackedSquares()
	void updatePotentialMoves() 
	{
		for (int i = 0, size = playBoard.size(); i < size; i++)
			for (int j = 0; j < size; j++)
				if (playBoard[i][j])
					playBoard[i][j]->updatePiecePotentialMoves(playBoard, { i,j });
	}

	//ALWAYS call updateAttackedSquares() After this function once again
	void updateLegalMoves()
	{
		areThereLegalMovesLeft = false;
		 
		for (int i = 0, size = playBoard.size(); i < size; i++)
			for (int j = 0; j < size; j++)
				if (playBoard[i][j]) //we iterate trough "playBoard" instead of "pieces" because it's easier to get indexes i and j this way
				{
					//find legal and illegal moves
					const auto& potentialMoves = playBoard[i][j]->getPotentialMoves();
					auto& illegalMoves = playBoard[i][j]->getIllegalMoves();
					auto& legalMoves = playBoard[i][j]->getLegalMoves();
					legalMoves.clear();
					illegalMoves.clear();
					auto currentPieceType = playBoard[i][j]->getType();

					for (auto it = potentialMoves.begin(); it != potentialMoves.end(); ++it)
					{
						Move move = { {i,j}, *it, currentPieceType, getPieceType(*this, *it) };
						if (init_validateMove(move)) 
						{
							areThereLegalMovesLeft = true;
							legalMoves.insert(*it);
						}
						else
							illegalMoves.insert(*it);
					}
				}
	}

	void updateChecks()
	{
		auto& isKingUnderCheck = (isWhitesTurn ? whiteUnderCheck : blackUnderCheck);
		auto& king = (isWhitesTurn ? white_king : black_king);
		const auto& attackedByEnemyCells = (isWhitesTurn ? attackedByBlackCells : attackedByWhiteCells);
		
		isKingUnderCheck = false;

		if (attackedByEnemyCells.find(king) != attackedByEnemyCells.end())
		{
			isKingUnderCheck = true;
			return;
		}
	
	}

	//Use only after applying a valid move and pushing it to "moves"
	void updateBoardState()
	{
		updateCastlingFlags();
		updateAttackedSquares();
		updatePotentialMoves();
		updateEnPassant();
		updateLegalMoves();
		updateAttackedSquares();
		updateChecks();
		updateCastlingMoves();
		//TODO::check for (stale)mates
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

		updateBoardState();


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

	//custom board
	//ChessBoard(const std::vector<std::vector<PieceType>>& BeginningBoard, const std::vector<Move> Moves):pieces(Pieces.size(),NULL) {
	//	for (unsigned amount = Pieces.size(), i = 0; i < amount; i++) {
	//		//pieces[i] = new;
	//	}
	//}

	bool isWhitesMove() const { return isWhitesTurn; }

	ChessPiece*& getCellPtr(std::pair<unsigned, unsigned> position) {
		return playBoard[position.first][position.second];
	}

	//handles move validation, formats inputed move if needed, and creates new available moves
	bool makeMove(Move& move) 
	{
		const unsigned& i_from = move.fromPos.first, & j_from = move.fromPos.second;
		const unsigned& i_to = move.toPos.first, & j_to = move.toPos.second;

		if (!validateMove(move))
			return false;
		
		applyAssumedMove(move);
		isWhitesTurn = !isWhitesTurn;
		moves.push(move);
		updateBoardState();
		
		return true;
	}

	bool cancelLastMove() {
		if (moves.empty())
			return false;

		revertMove(moves.top(), isWhitesTurn);
		isWhitesTurn = !isWhitesTurn;
		moves.pop();
		updateBoardState();

		return true;
	}
	
	void updateUI(bool is_player_white, const sf::Vector2f& boardPosition, const float& squareSize, sf::RenderTarget& target) {
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
					default:
						sprite.setTexture(king.second);
						sprite.setColor(sf::Color::Red);
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

	void drawMovesOfSelected(bool is_player_white, const std::pair<unsigned, unsigned>& fromCell, const sf::Vector2f& boardPosition, const float& squareSize, sf::RenderTarget& target) {
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

			for (const auto& moveSquare : playBoard[i][j]->getLegalMoves()) 
			{
				const unsigned& move_i = moveSquare.first, & move_j = moveSquare.second;
				sf::Vector2f position = boardPosition + sf::Vector2f({ (is_player_white ? (7 - move_j) : move_j) * squareSize,   (!is_player_white ? (7 - move_i) : move_i) * squareSize });

				circle.setPosition(position);
				circle.setFillColor(sf::Color(180, 0, 0, 100));
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