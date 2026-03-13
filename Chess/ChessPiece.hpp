#pragma once

#include "Move.hpp"
#include "ChessBoard.hpp"

class ChessPiece {
private:
	PieceType type;
	friend class ChessBoard;

	std::vector<std::pair<unsigned, unsigned>> attackedSquares;
	std::vector<std::pair<unsigned, unsigned>> possibleMoves;
	std::vector<std::pair<unsigned, unsigned>> illegalMoves;

	void updatePawnMoves(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position)
	{
		attackedSquares.clear();
		possibleMoves.clear();

		unsigned i = position.first, j = position.second;

		if (!CREFboard.size() || CREFboard.size() <= i || CREFboard[i].size() <= j || CREFboard[i][j] != this) // is cell on board and is it this exact piece
			return;

		if (white) {
			if ( i > 0 && !CREFboard[i - 1][j]) {
				possibleMoves.push_back({ i - 1,j });
				if (i == 6 && !CREFboard[i - 2][j])
					possibleMoves.push_back({ i - 2,j });
			}

			if (i > 0) {
				if(j > 0)
					attackedSquares.push_back({ i - 1,j - 1 });
				if(j < 7)
					attackedSquares.push_back({ i - 1,j + 1 });
			}
		}

		if (!white) {
			if (i < 7 && !CREFboard[i + 1][j]) {
				possibleMoves.push_back({ i + 1,j });
				if (i == 1 && !CREFboard[i + 2][j])
					possibleMoves.push_back({ i + 2,j });
			}

			if (i > 0) {
				if (j > 0)
					attackedSquares.push_back({ i + 1,j - 1 });
				if (j < 7)
					attackedSquares.push_back({ i + 1,j + 1 });
			}
		}

		//TODO:: en passant
			
	}
	void updateKnightMoves(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) {
		attackedSquares.clear();
		possibleMoves.clear();
		
		unsigned i = position.first, j = position.second;

		if (!CREFboard.size() || CREFboard.size() <= i || CREFboard[i].size() <= j || CREFboard[i][j] != this) // is cell on board and is it this exact piece
			return;


		for (int a = 0; a >-3; a -= 2)
			for (int b = 0; b < 5; b += 4)
				if (i + 1 + a <= 7 &&
					j - 2 + b <= 7)
					attackedSquares.push_back({ i + 1 + a, j - 2 + b });

		for (int a = 0; a >-5; a -= 4)
			for (int b = 0; b < 3; b += 2)
				if (i + 2 + a <= 7 &&
					j - 1 + b <= 7)
					attackedSquares.push_back({ i + 2 + a, j - 1 + b });



	}
	void updateBishopMoves(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) {
		attackedSquares.clear();
		possibleMoves.clear();
		
		unsigned a, i = a = position.first, b, j = b = position.second;

		if (!CREFboard.size() || CREFboard.size() <= i || CREFboard[i].size() <= j || CREFboard[i][j] != this) // is cell on board and is it this exact piece
			return;

		a++, b++;
		while (a <= 7 && b <= 7) {
			attackedSquares.push_back({ a,b });

			if (CREFboard[a++][b++])
				break;
		}
		a = i; b = j;
		a--, b--;
		while (a <= 7 && b <= 7) {
			attackedSquares.push_back({ a,b });

			if (CREFboard[a--][b--])
				break;
		}
		a = i; b = j;
		a++, b--;
		while (a <= 7 && b <= 7) {
			attackedSquares.push_back({ a,b });

			if (CREFboard[a++][b--])
				break;
		}
		a = i; b = j;
		a--, b++;
		while (a <= 7 && b <= 7) {
			attackedSquares.push_back({ a,b });

			if (CREFboard[a--][b++])
				break;
		}
			
	}
	void updateRookMoves(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) {
		attackedSquares.clear();
		possibleMoves.clear();
		
		unsigned a, i = a = position.first, j = position.second;

		if (!CREFboard.size() || CREFboard.size() <= i || CREFboard[i].size() <= j || CREFboard[i][j] != this) // is cell on board and is it this exact piece
			return;

		a++;
		while (a <= 7) {
			attackedSquares.push_back({ a,j });

			if (CREFboard[a++][j])
				break;
		}
		a = i;
		a--;
		while (a <= 7) {
			attackedSquares.push_back({ a,j });

			if (CREFboard[a--][j])
				break;
		}
		a = j;
		a++;
		while (a <= 7) {
			attackedSquares.push_back({ i,a });

			if (CREFboard[i][a++])
				break;
		}
		a = j;
		a--;
		while (a <= 7) {
			attackedSquares.push_back({ i,a });

			if (CREFboard[i][a--])
				break;
		}

	}
	void updateQueenMoves(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) {
		attackedSquares.clear();
		possibleMoves.clear();
		
		unsigned a, i = a = position.first, b, j = b = position.second;

		if (!CREFboard.size() || CREFboard.size() <= i || CREFboard[i].size() <= j || CREFboard[i][j] != this) // is cell on board and is it this exact piece
			return;

		a++, b++;
		while (a <= 7 && b <= 7) {
			attackedSquares.push_back({ a,b });

			if (CREFboard[a++][b++])
				break;
		}
		a = i; b = j;
		a--, b--;
		while (a <= 7 && b <= 7) {
			attackedSquares.push_back({ a,b });

			if (CREFboard[a--][b--])
				break;
		}
		a = i; b = j;
		a++, b--;
		while (a <= 7 && b <= 7) {
			attackedSquares.push_back({ a,b });

			if (CREFboard[a++][b--])
				break;
		}
		a = i; b = j;
		a--, b++;
		while (a <= 7 && b <= 7) {
			attackedSquares.push_back({ a,b });

			if (CREFboard[a--][b++])
				break;
		}


		a = i;
		a++;
		while (a <= 7) {
			attackedSquares.push_back({ a,j });

			if (CREFboard[a++][j])
				break;
		}
		a = i;
		a--;
		while (a <= 7) {
			attackedSquares.push_back({ a,j });

			if (CREFboard[a--][j])
				break;
		}
		a = j;
		a++;
		while (a <= 7) {
			attackedSquares.push_back({ i,a });

			if (CREFboard[i][a++])
				break;
		}
		a = j;
		a--;
		while (a <= 7) {
			attackedSquares.push_back({ i,a });

			if (CREFboard[i][a--])
				break;
		}

	}
	void updateKingMoves(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) {
		attackedSquares.clear();
		possibleMoves.clear();
		
		unsigned i = position.first, j = position.second;

		if (!CREFboard.size() || CREFboard.size() <= i || CREFboard[i].size() <= j || CREFboard[i][j] != this) // is cell on board and is it this exact piece
			return;

		for (int a = -1; a < 2; a++)
			for (int b = -1; b < 2; b++)
				if (!(a == 0 && b == 0) && i + a <= 7 && j + b <= 7)
					attackedSquares.push_back({ i + a, j + b });

	}


	void updatePieceMoves(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) {
		std::vector<std::pair<unsigned, unsigned>> result;
		switch (type)
		{
		case Pawn:
			updatePawnMoves(CREFboard, position);
			for (const auto& attacked : attackedSquares)
				if (CREFboard[attacked.first][attacked.second] && white != CREFboard[attacked.first][attacked.second]->white)
					possibleMoves.push_back(attacked);

			//en passant???
			//Promotion???
			break;
		case Knight:
			updateKnightMoves(CREFboard, position);
			for (const auto& attacked : attackedSquares)
				if (!CREFboard[attacked.first][attacked.second] || white != CREFboard[attacked.first][attacked.second]->white)
					possibleMoves.push_back(attacked);
			break;
		case Bishop:
			updateBishopMoves(CREFboard, position);
			for (const auto& attacked : attackedSquares)
				if (!CREFboard[attacked.first][attacked.second] || white != CREFboard[attacked.first][attacked.second]->white)
					possibleMoves.push_back(attacked);
			break;
		case Rook:
			updateRookMoves(CREFboard, position);
			for (const auto& attacked : attackedSquares)
				if (!CREFboard[attacked.first][attacked.second] || white != CREFboard[attacked.first][attacked.second]->white)
					possibleMoves.push_back(attacked);
			break;
		case Queen:
			updateQueenMoves(CREFboard, position);
			for (const auto& attacked : attackedSquares)
				if (!CREFboard[attacked.first][attacked.second] || white != CREFboard[attacked.first][attacked.second]->white)
					possibleMoves.push_back(attacked);
			break;
		case King:
			updateKingMoves(CREFboard, position);
			for (const auto& attacked : attackedSquares)
				if (!CREFboard[attacked.first][attacked.second] || white != CREFboard[attacked.first][attacked.second]->white)
					possibleMoves.push_back(attacked);


			//Castling???
			break;
		}



		//further validtaion: is check or stalemate now (and after each move if is check), is


	}


	//Only use after all of the moves are updated
	void setPossibleMoves(const std::vector<std::pair<unsigned, unsigned>>& newMoves){
		illegalMoves = newMoves;
	}

public:
	const bool white;
	bool active = true;

	ChessPiece() = delete;
	ChessPiece(PieceType Type, bool isWhite) : white(isWhite), type(Type) {

	}
	
	PieceType getType() {
		if (this == NULL)
			return EmptyCell;

		return type;
	}

	

	const std::vector<std::pair<unsigned, unsigned>>& getAttackedSquares() const {
		return attackedSquares;
	}
	 std::vector<std::pair<unsigned, unsigned>>& getPieceMoves() {
		return possibleMoves;
	}







};