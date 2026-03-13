#pragma once
#include "ChessPiece.hpp"

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

class Move {
public:
	std::pair<unsigned, unsigned> fromPos;
	std::pair<unsigned, unsigned> toPos;
	PieceType ptFrom;
	PieceType ptTo;

	std::string to_string() {
		return  std::to_string(fromPos.first) + "," + std::to_string(fromPos.second) + "," + std::to_string(toPos.first) + "," + std::to_string(toPos.second) + "," + std::to_string(ptFrom) + "," + std::to_string(ptTo);
	}
};

