#include "tcp_client.h"
#include "Game.h"
int main(int argc, char **argv)
{
	game::game_t Game;
	std::thread Thread;
	net_client::client Client;
	Client.on(tcp_common::message_type::GAME_CREATED, [&](std::vector<int> Data) {
		std::cout << "Game has been create!!! Yoohay" << std::endl;
		std::cout << "Game Id is: " << Data[0] << std::endl;
		Game.Id = Data[0];
		game::player_type Side = (game::player_type)Data[1];
		if (Side == game::player_type::CIRCLE) {
			std::cout << "You are Circle" << std::endl;
		}
		else if (Side == game::player_type::CROSS) {
			std::cout << "You are Cross" << std::endl;
		}
		else {
			std::cout << "[Debug]Wrong data" << std::endl;
		}
	});
	Client.on(tcp_common::message_type::OTHER_PLAYER_LEFT, [&](std::vector<int> Data) {
		std::cout << "Other player has left the game\n";
		//Reset the game so the player can create another game.
		Game.Id = -1;
	});
	if (!Client.initialize()) {
		return -1;
	}
	if (!Client.connect_to_server("127.0.0.1", "27015")) {
		return -1;
	}
	Thread = std::thread([&](void) {
		Client.listen_on_port();
	});
	int userAnswer = 0;
	const char* Menu = "Choose one of the following: \n"
		"1. Send a create game message\n"
		"2. Send mark message\n"
		"3. Quit\n";
	while (Client.Running.load()) {
		std::cout << Menu;
		std::cin >> userAnswer;
		switch (userAnswer) {
		case 1:{
			std::vector<int> data;
			data.push_back(0);
			data.push_back(0);
			Client.send_message
			(tcp_common::create_message(tcp_common::message_type::CREATE_GAME, data));
			break;
		}
		case 2: {
			if (Game.Id == -1) {
				std::cout << "You are not connected to any game\n";
				break;
			}
			std::cout << "Enter the value of x\n";
			int x = 1;
			std::cin >> x;
			std::cout << "Enter the value of y\n";
			int y = 2;
			std::cin >> y;
			std::vector<int> data;
			data.push_back(x);
			data.push_back(y);
			Client.send_message
			(tcp_common::create_message(tcp_common::message_type::MARK, data));
			break;
		}
		case 3: {
			Client.stop();
			break;
		}
		}
	}
	Thread.join();
	return 0;
}