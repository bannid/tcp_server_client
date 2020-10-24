#pragma once
#include<vector>
#include <stdint.h>

namespace tcp_common {
	enum message_type : uint8_t{
		CREATE_GAME,
		MARK,
		QUIT_GAME
	};
	template<typename DataType>
	std::vector<char> create_message(message_type MessageType,std::vector<DataType> Data ) {
		std::vector<char> ToReturn;
		switch (MessageType) {
		case message_type::MARK: {
			ToReturn.push_back(message_type::MARK);
			for (int i = 0; i < Data.size(); i++) {
				char* DataTemp = (char*)&Data[i];
				for (int j = 0; j < sizeof(DataType); j++) {
					ToReturn.push_back(*DataTemp);
					DataTemp++;
				}
			}
			return ToReturn;
		}
		default: {
			ToReturn.push_back(MessageType);
			return ToReturn;
		}
		}
	}
}