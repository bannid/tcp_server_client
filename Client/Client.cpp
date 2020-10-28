#include "tcp_client.h"
#include "Game.h"
void clear_screen(char fill = ' ') {
	COORD tl = { 0,0 };
	CONSOLE_SCREEN_BUFFER_INFO s;
	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo(console, &s);
	DWORD written, cells = s.dwSize.X * s.dwSize.Y;
	FillConsoleOutputCharacter(console, fill, cells, tl, &written);
	FillConsoleOutputAttribute(console, s.wAttributes, cells, tl, &written);
	SetConsoleCursorPosition(console, tl);
}
int main(int argc, char **argv)
{
	game::game_t Game;
	game::player_type Side;
	std::thread Thread;
	net_client::client Client;
	Client.on(tcp_common::message_type::GAME_CREATED, [&](std::vector<int> Data) {
		Game.Id = Data[0];
		Side = (game::player_type)Data[1];
		game::print_game(&Game);
	});
	Client.on(tcp_common::message_type::OTHER_PLAYER_LEFT, [&](std::vector<int> Data) {
		std::cout << "Other player has left the game\n";
		game::reset_game(&Game);
	});
	Client.on(tcp_common::message_type::GAME_REQUEST_EXISTS, [&](std::vector<int> Data) {
		std::cout << "Cannot request two games at same time\n";
	});
	Client.on(tcp_common::message_type::GAME_ALREADY_EXISTS, [&](std::vector<int> Data) {
		std::cout << "Cannot request while ongoing game\n";
	});
	Client.on(tcp_common::message_type::MARK, [&](std::vector<int> Data) {
		int x = Data[0];
		int y = Data[1];
		char c;
		if (Side == game::player_type::CIRCLE) {
			c = 'X';
		}
		else {
			c = 'O';
		}
		Game.NumberOfMoves++;
		Game.Board[y * 3 + x] = c;
		clear_screen();
		game::print_game(&Game);
	});

	if (!Client.initialize()) {
		Client.stop();
	}
	if (!Client.connect_to_server("1.156.190.59", "27015")) {
		Client.stop();
	}
	Thread = std::thread([&](void) {
		if (Client.Running.load()) {
			Client.listen_on_port();
		}
	});
	
	const char* Menu = "1. Create a game\n"
		"2. Mark on game\n"
		"3. Clear screen\n"
		"4. Print the game.\n"
		"5. Stop the client\n";
	while (Client.Running.load()) {
		std::vector<int> Data;
		int userAnswer = 0;
		std::cout << Menu;
		std::cin >> userAnswer;
		switch (userAnswer) {

		case 1: {
			Data.push_back(0);
			Data.push_back(0);
			Client.send_message(tcp_common::create_message(tcp_common::CREATE_GAME,Data));
			break;
		}
		case 2: {
			if (Game.Id == -1) {
				std::cout << "You do not have an active game\n";
				break;
			}
			if (Side == game::player_type::CIRCLE) {
				if (Game.NumberOfMoves % 2 != 0) {
					std::cout << "Not your move!\n";
					clear_screen();
					break;
				}
			}
			else if (Side == game::player_type::CROSS) {
				if (Game.NumberOfMoves % 2 == 0) {
					std::cout << "Not your move!\n";
					clear_screen();
					break;
				}
			}
			clear_screen();
			int x, y;
			std::cout << "Enter the value of the x\n";
			std::cin >> x;
			std::cout << "Enter the value of the y\n";
			std::cin >> y;
			Data.push_back(x);
			Data.push_back(y);
			char c;
			if (Side == game::player_type::CIRCLE) {
				c = 'O';
			}
			else {
				c = 'X';
			}
			Game.NumberOfMoves++;
			Game.Board[y * 3 + x] = c;
			Client.send_message(tcp_common::create_message(tcp_common::message_type::MARK, Data));
		}
		case 3: {
			clear_screen();
			break;
		}
		case 4: {
			if (Game.Id == -1) {
				std::cout << "No game active!!\n";
			}
			else {
				game::print_game(&Game);
			}
			break;
		}
		case 5: {
			Client.stop();
			break;
		}
		}
	}
	Thread.join();
	return 0;
}