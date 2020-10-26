#pragma once
#include "tcp_common.h"

namespace game {
	enum game_state :uint8_t {
		ACTIVE,
		NOT_ACTIVE,
		WAITING_FOR_SECOND_PLAYER
	};
	struct game_t {
		int Id;
		std::vector<int> ClientsIds;
		game_state GameState = WAITING_FOR_SECOND_PLAYER;
	};
}