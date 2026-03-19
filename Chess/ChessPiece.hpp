#pragma once

#include "Move.hpp"
#include "ChessBoard.hpp"

enum PieceType {
	EnPassant = -3,
	Promotion = -2,
	Castling = -1,
	EmptyCell = 0,
	Pawn = 1,
	Knight = 2,
	Bishop = 3,
	Rook = 4,
	Queen = 5,
	King = 6
};

class ChessPiece {
private:
	PieceType type;
	friend class ChessBoard;

	std::set<std::pair<unsigned, unsigned>> attackedSquares;
	std::set<std::pair<unsigned, unsigned>> potentialMoves;
	std::set<std::pair<unsigned, unsigned>> legalMoves;
	std::set<std::pair<unsigned, unsigned>> illegalMoves;

	void updatePawnAttackedSquares(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position)
	{
		const unsigned& i = position.first, &j = position.second;


		// is cell on board and is it this exact piece
		if (!CREFboard.size() || CREFboard.size() <= i || CREFboard[i].size() <= j || CREFboard[i][j] != this) 
			return;

		unsigned offset = white ? -1 : 1;
		//add attacks
		if(j != 0)
			attackedSquares.insert({ i + offset, j - 1 });
		if(j != 7)
			attackedSquares.insert({ i + offset, j + 1 });
	}
	void updateKnightAttackedSquares(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) 
	{	
		unsigned i = position.first, j = position.second;

		if (!CREFboard.size() || CREFboard.size() <= i || CREFboard[i].size() <= j || CREFboard[i][j] != this) // is cell on board and is it this exact piece
			return;


		for (int a = 0; a >=-2; a -= 2)
			for (int b = 0; b <= 4; b += 4)
				if ( (unsigned)(i + 1 + a) <= 7 &&
					 (unsigned)(j - 2 + b) <= 7)
					attackedSquares.insert({ i + 1 + a, j - 2 + b });

		for (int a = 0; a >=-4; a -= 4)
			for (int b = 0; b <= 2; b += 2)
				if ( (unsigned)(i + 2 + a) <= 7 &&
					 (unsigned)(j - 1 + b) <= 7)
					attackedSquares.insert({ i + 2 + a, j - 1 + b });

	}
	void updateBishopAttackedSquares(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) 
	{
		unsigned a, b;
		const auto& i_pos = position.first;
		const auto& j_pos = position.second;

		if (!CREFboard.size() || CREFboard.size() <= i_pos || CREFboard[i_pos].size() <= j_pos || CREFboard[i_pos][j_pos] != this) // is cell on board and is it this exact piece
			return;

		for (short i = 0, offsetA, offsetB; i <= 3; i++) 
		{
			offsetA = (i & 2) ? -1 : 1;
			offsetB = (i & 1) ? -1 : 1;
			a = i_pos + offsetA;
			b = j_pos + offsetB;

			while (a <= 7 && b <= 7) 
			{
				attackedSquares.insert({ a,b });

				if (CREFboard[a][b]) 
					break;
				
				a += offsetA;
				b += offsetB;
			}
		}	
	}
	void updateRookAttackedSquares(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) 
	{
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
	void updateQueenAttackedSquares(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) 
	{
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
	void updateKingAttackedSquares(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) 
	{
		unsigned i = position.first, j = position.second;

		if (!CREFboard.size() || CREFboard.size() <= i || CREFboard[i].size() <= j || CREFboard[i][j] != this) // is cell on board and is it this exact piece
			return;

		for (int a = -1; a < 2; a++)
			for (int b = -1; b < 2; b++)
				if (!(a == 0 && b == 0) && i + a <= 7 && j + b <= 7)
					attackedSquares.insert({ i + a, j + b });

	}

	void updatePieceAttackedSquares(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position)
	{
		attackedSquares.clear();

		switch (type)
		{
		case Pawn:
			updatePawnAttackedSquares(CREFboard, position);
			return;
		case Knight:
			updateKnightAttackedSquares(CREFboard, position);
			break;
		case Bishop:
			updateBishopAttackedSquares(CREFboard, position);
			break;
		case Rook:
			updateRookAttackedSquares(CREFboard, position);
			break;
		case Queen:
			updateQueenAttackedSquares(CREFboard, position);
			break;
		case King:
			updateKingAttackedSquares(CREFboard, position);
			break;
		}

		
	}
	
	//ONLY to be used after calling updatePieceAttackedSquares()
	void updatePiecePotentialMoves(const std::vector<std::vector<ChessPiece*>>& CREFboard, std::pair<unsigned, unsigned> position) 
	{
		potentialMoves.clear();

		unsigned i = position.first, j = position.second;

		if (!CREFboard.size() || CREFboard.size() <= i || CREFboard[i].size() <= j || CREFboard[i][j] != this) // is cell on board and is there this exact piece
			return;

		if (type == Pawn) 
		{
			short offset = white ? -1 : 1, //move direction
				edge = white ? (CREFboard.size() - 1) : 0; //corrseponding edge of the table


			if (i != (white ? 0 : edge) && !CREFboard[i + offset][j])
			{
				//add forward step
				potentialMoves.insert({ i + offset,j });

				//add double step if necessary
				if (i == (edge + offset) && !CREFboard[i + 2 * offset][j])
					potentialMoves.insert({ i + 2 * offset, j });
			}

			//add diagonal attacks if necessary
			for (const auto& attacked : attackedSquares)
				if (CREFboard[attacked.first][attacked.second] && white != CREFboard[attacked.first][attacked.second]->white)
					potentialMoves.insert(attacked);

			return;
		}

		//every other piece is able to move only to the squares attacked by themselves, in case there isn't an ally piece, so add all such ones.
		for (const auto& attacked : attackedSquares)
			if (!CREFboard[attacked.first][attacked.second] || white != CREFboard[attacked.first][attacked.second]->white)
				potentialMoves.insert(attacked);
	}
	



public:
	const bool white;
	bool active = true;

	ChessPiece() = delete;
	ChessPiece(PieceType Type, bool isWhite) : white(isWhite), type(Type) {

	}

	void setType(const PieceType& _type) 
	{
		if (_type < 1 || _type > 6)
			return;

		type = _type;
	}
	
	PieceType getType() {
		if (this == NULL)
			return EmptyCell;

		return type;
	}

	void setIllegalMoves(const std::set<std::pair<unsigned, unsigned>>& set) {
		illegalMoves = set;
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