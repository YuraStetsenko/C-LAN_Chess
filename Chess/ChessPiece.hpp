#pragma once

#include "Move.hpp"
#include "ChessBoard.hpp"

class ChessPiece {
private:
	PieceType type;
	friend class ChessBoard;

	std::set<std::pair<unsigned, unsigned>> attackedSquares;
	std::set<std::pair<unsigned, unsigned>> potentialMoves;
	std::set<std::pair<unsigned, unsigned>> legalMoves;
	std::set<std::pair<unsigned, unsigned>> illegalMoves;

	void updatePawnMoves(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position)
	{
		attackedSquares.clear();
		potentialMoves.clear();

		unsigned i = position.first, j = position.second;

		if (!CREFboard.size() || CREFboard.size() <= i || CREFboard[i].size() <= j || CREFboard[i][j] != this) // is cell on board and is it this exact piece
			return;

		if (white) {
			if ( i > 0 && !CREFboard[i - 1][j]) {
				potentialMoves.insert({ i - 1,j });
				if (i == 6 && !CREFboard[i - 2][j])
					potentialMoves.insert({ i - 2,j });
			}

			if (i > 0) {
				if(j > 0)
					attackedSquares.insert({ i - 1,j - 1 });
				if(j < 7)
					attackedSquares.insert({ i - 1,j + 1 });
			}
		}

		if (!white) {
			if (i < 7 && !CREFboard[i + 1][j]) {
				potentialMoves.insert({ i + 1,j });
				if (i == 1 && !CREFboard[i + 2][j])
					potentialMoves.insert({ i + 2,j });
			}

			if (i > 0) {
				if (j > 0)
					attackedSquares.insert({ i + 1,j - 1 });
				if (j < 7)
					attackedSquares.insert({ i + 1,j + 1 });
			}
		}

		//TODO:: en passant
			
	}
	void updateKnightMoves(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) {
		attackedSquares.clear();
		potentialMoves.clear();
		
		unsigned i = position.first, j = position.second;

		if (!CREFboard.size() || CREFboard.size() <= i || CREFboard[i].size() <= j || CREFboard[i][j] != this) // is cell on board and is it this exact piece
			return;


		for (int a = 0; a >-3; a -= 2)
			for (int b = 0; b < 5; b += 4)
				if (i + 1 + a <= 7 &&
					j - 2 + b <= 7)
					attackedSquares.insert({ i + 1 + a, j - 2 + b });

		for (int a = 0; a >-5; a -= 4)
			for (int b = 0; b < 3; b += 2)
				if (i + 2 + a <= 7 &&
					j - 1 + b <= 7)
					attackedSquares.insert({ i + 2 + a, j - 1 + b });



	}
	void updateBishopMoves(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) {
		attackedSquares.clear();
		potentialMoves.clear();
		
		unsigned a, i = a = position.first, b, j = b = position.second;

		if (!CREFboard.size() || CREFboard.size() <= i || CREFboard[i].size() <= j || CREFboard[i][j] != this) // is cell on board and is it this exact piece
			return;

		a++, b++;
		while (a <= 7 && b <= 7) {
			attackedSquares.insert({ a,b });

			if (CREFboard[a++][b++])
				break;
		}
		a = i; b = j;
		a--, b--;
		while (a <= 7 && b <= 7) {
			attackedSquares.insert({ a,b });

			if (CREFboard[a--][b--])
				break;
		}
		a = i; b = j;
		a++, b--;
		while (a <= 7 && b <= 7) {
			attackedSquares.insert({ a,b });

			if (CREFboard[a++][b--])
				break;
		}
		a = i; b = j;
		a--, b++;
		while (a <= 7 && b <= 7) {
			attackedSquares.insert({ a,b });

			if (CREFboard[a--][b++])
				break;
		}
			
	}
	void updateRookMoves(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) {
		attackedSquares.clear();
		potentialMoves.clear();
		
		unsigned a, i = a = position.first, j = position.second;

		if (!CREFboard.size() || CREFboard.size() <= i || CREFboard[i].size() <= j || CREFboard[i][j] != this) // is cell on board and is it this exact piece
			return;

		a++;
		while (a <= 7) {
			attackedSquares.insert({ a,j });

			if (CREFboard[a++][j])
				break;
		}
		a = i;
		a--;
		while (a <= 7) {
			attackedSquares.insert({ a,j });

			if (CREFboard[a--][j])
				break;
		}
		a = j;
		a++;
		while (a <= 7) {
			attackedSquares.insert({ i,a });

			if (CREFboard[i][a++])
				break;
		}
		a = j;
		a--;
		while (a <= 7) {
			attackedSquares.insert({ i,a });

			if (CREFboard[i][a--])
				break;
		}

	}
	void updateQueenMoves(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) {
		attackedSquares.clear();
		potentialMoves.clear();
		
		unsigned a, i = a = position.first, b, j = b = position.second;

		if (!CREFboard.size() || CREFboard.size() <= i || CREFboard[i].size() <= j || CREFboard[i][j] != this) // is cell on board and is it this exact piece
			return;

		a++, b++;
		while (a <= 7 && b <= 7) {
			attackedSquares.insert({ a,b });

			if (CREFboard[a++][b++])
				break;
		}
		a = i; b = j;
		a--, b--;
		while (a <= 7 && b <= 7) {
			attackedSquares.insert({ a,b });

			if (CREFboard[a--][b--])
				break;
		}
		a = i; b = j;
		a++, b--;
		while (a <= 7 && b <= 7) {
			attackedSquares.insert({ a,b });

			if (CREFboard[a++][b--])
				break;
		}
		a = i; b = j;
		a--, b++;
		while (a <= 7 && b <= 7) {
			attackedSquares.insert({ a,b });

			if (CREFboard[a--][b++])
				break;
		}


		a = i;
		a++;
		while (a <= 7) {
			attackedSquares.insert({ a,j });

			if (CREFboard[a++][j])
				break;
		}
		a = i;
		a--;
		while (a <= 7) {
			attackedSquares.insert({ a,j });

			if (CREFboard[a--][j])
				break;
		}
		a = j;
		a++;
		while (a <= 7) {
			attackedSquares.insert({ i,a });

			if (CREFboard[i][a++])
				break;
		}
		a = j;
		a--;
		while (a <= 7) {
			attackedSquares.insert({ i,a });

			if (CREFboard[i][a--])
				break;
		}

	}
	void updateKingMoves(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) {
		attackedSquares.clear();
		potentialMoves.clear();
		
		unsigned i = position.first, j = position.second;

		if (!CREFboard.size() || CREFboard.size() <= i || CREFboard[i].size() <= j || CREFboard[i][j] != this) // is cell on board and is it this exact piece
			return;

		for (int a = -1; a < 2; a++)
			for (int b = -1; b < 2; b++)
				if (!(a == 0 && b == 0) && i + a <= 7 && j + b <= 7)
					attackedSquares.insert({ i + a, j + b });

	}


	void updatePiecePotentialMoves(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) 
	{

		switch (type)
		{
		case Pawn:
			updatePawnMoves(CREFboard, position);
			break;
		case Knight:
			updateKnightMoves(CREFboard, position);
			break;
		case Bishop:
			updateBishopMoves(CREFboard, position);
			break;
		case Rook:
			updateRookMoves(CREFboard, position);
			break;
		case Queen:
			updateQueenMoves(CREFboard, position);
			break;
		case King:
			updateKingMoves(CREFboard, position);
			break;
		}

		if (type == Pawn) // A pawn can't move to the square attacked by itself, unless it's to take an enemy's piece
		{
			for (const auto& attacked : attackedSquares)
				if (CREFboard[attacked.first][attacked.second] && white != CREFboard[attacked.first][attacked.second]->white)
					potentialMoves.insert(attacked);
		}
		else //every other piece can move to any of the squares attacked by themselves, unless there's an ally piece
			for (const auto& attacked : attackedSquares)
				if (!CREFboard[attacked.first][attacked.second] || white != CREFboard[attacked.first][attacked.second]->white)
					potentialMoves.insert(attacked);

	}


	//Only use after all of the moves are updated
	void setIllegalMoves(const std::set<std::pair<unsigned, unsigned>>& newMoves){
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

	

	const std::set<std::pair<unsigned, unsigned>>& getAttackedSquares() const {
		return attackedSquares;
	}
	 std::set<std::pair<unsigned, unsigned>>& getPotentialMoves() {
		return potentialMoves;
	}
	 std::set<std::pair<unsigned, unsigned>>& getIllegalMoves() {
		 return illegalMoves;
	 }

	 std::set<std::pair<unsigned, unsigned>>& getLegalMoves() {
		 return legalMoves;
	 }






};