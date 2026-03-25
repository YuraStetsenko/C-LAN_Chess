#pragma once
#include "ChessPiece.hpp"

namespace myChess {

	enum PieceType;

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
}

