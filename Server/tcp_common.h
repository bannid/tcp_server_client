#pragma once
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <stdint.h>
#include "Game.h"

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
namespace tcp_common {
	enum message_type : uint8_t {
		CREATE_GAME, //No msg body
		GAME_CREATED, //Need to send the game id.
		GAME_ALREADY_EXISTS,//Need to send the game id.
		GAME_REQUEST_EXISTS,
		GAME_FINISHED,//No msg body.
		WINNER,//No message needed
		DRAW,//No msg body
		LOSER,//No msg body
		OTHER_PLAYER_LEFT,//No msg body
		MARK,//Need to send the x and y coords
		QUIT_GAME,
		INVALID_GAME_ID
	};
	const int SIZE_OF_MESSAGE = sizeof(message_type) + sizeof(int) * 2;
	struct client {
		SOCKET Socket;
		int Id;
		bool Active = true;
		bool ClosedGracefully = false;
		std::vector<unsigned char> ByteStream;
		int GameId = -1;
		game::player_type Side;
	};
	struct msg {
		tcp_common::message_type Header;
		std::vector<unsigned char> MsgBody;
		int ClientId;
	};
	std::vector<unsigned char> create_message(message_type MessageType, std::vector<int> Data) {
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

	int parse_int_from_bytes(unsigned char* bytes) {
		int ToReturn = 0;
		for (int i = 0; i < 4; i++) {
			int temp = (int)bytes[i];
			temp = temp << (i * 8);
			ToReturn = ToReturn | temp;
		}
		return ToReturn;
	}

	std::vector<int> decode_data(message_type MessageType, std::vector<unsigned char> data) {
		std::vector<int> ToReturn;
		switch (MessageType) {
		case message_type::MARK: {
			std::cout << "Mark message received" << std::endl;
			int x = 3;
			int y = 2;
			x = parse_int_from_bytes(&data[0]);
			y = parse_int_from_bytes(&data[4]);
			std::cout << "value of x is " << x << std::endl;
			std::cout << "value of y is " << y << std::endl;
			ToReturn.push_back(x);
			ToReturn.push_back(y);
			return ToReturn;
		}
		case message_type::GAME_CREATED: {
			std::cout << "Game created message received" << std::endl;
			int GameId = 0;
			int Side;
			GameId = parse_int_from_bytes(&data[0]);
			Side = parse_int_from_bytes(&data[4]);
			std::cout << "Game ID is: " << GameId << std::endl;
			ToReturn.push_back(GameId);
			ToReturn.push_back(Side);
			return ToReturn;

		}
		}
	}
}