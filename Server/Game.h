#pragma once
#include "tcp_common.h"

namespace game {
	enum game_state :uint8_t {
		ACTIVE,
		NOT_ACTIVE,
		WAITING_FOR_SECOND_PLAYER
	};
	enum player_type :uint32_t {
		CIRCLE,
		CROSS
	};
	struct game_t {
		int Id = -1;
		int NumberOfMoves = 0;
		std::vector<int> ClientsIds;
		game_state GameState = game_state::NOT_ACTIVE;
		char Board[9] = {
			' ',' ',' ',
			' ',' ',' ',
			' ',' ',' '
		};
	};
	void reset_game(game_t* Game) {
		Game->Id = -1;
		for (int i = 0; i < 9; i++) {
			Game->Board[i] = ' ';
		}
		Game->NumberOfMoves = 0;
		Game->GameState = game_state::NOT_ACTIVE;
	}
	void print_game(game_t* Game) {
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				char c = Game->Board[3 * i + j];
				if (c == ' ') {
					std::cout << '_' << ' ';
				}
				else {
					std::cout << c << ' ';
				}
				
			}
			std::cout << std::endl;
		}
	}
}