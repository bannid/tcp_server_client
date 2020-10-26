#pragma once
#include<vector>
#include <stdint.h>

namespace tcp_common {
	enum message_type : uint8_t {
		CREATE_GAME,
		GAME_CREATED,
		GAME_FINISHED,
		WINNER,
		DRAW,
		OTHER_PLAYER_LEFT,
		MARK,
		QUIT_GAME
	};
	const int SIZE_OF_MESSAGE = sizeof(message_type) + sizeof(int) * 2;
	template<typename DataType>
	std::vector<unsigned char> create_message(message_type MessageType, std::vector<DataType> Data) {
		std::vector<unsigned char> ToReturn;
		ToReturn.push_back(MessageType);
		unsigned char* DataTemp = (unsigned char*)&Data[0];
		for (int j = 0; j < SIZE_OF_MESSAGE - sizeof(message_type); j++) {
			ToReturn.push_back(*DataTemp);
			DataTemp++;
		}
		std::cout << "[Debug] Size of message is " << ToReturn.size() << std::endl;
		return ToReturn;

	}
}