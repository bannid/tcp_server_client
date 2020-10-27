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
		game_state GameState = WAITING_FOR_SECOND_PLAYER;
	};
}