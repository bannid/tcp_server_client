#include "olcPixelGameEngine.h"
#include "tcp_client.h"
#include "Game.h"
#include <cmath>

enum connection_status {
	CONNECTING,
	FAILED,
	CONNECTED,
	DISCONNECTED
};
enum main_menu_status {
	PLAY_GAME,
	EXIT_GAME
};
enum interrupted_menu_status {
	BACK_TO_MAIN_MENU_INTERRUPTED,
	EXIT_GAME_INTERRUPTED
};
enum game_finished_menu_status {
	BACK_TO_MAIN_MENU_DRAWN_GAME,
	EXIT_GAME_DRAWN_GAME
};
enum game_status {
	PLAYING,
	NOT_PLAYING,
	WAITING_FOR_RESPONSE,
	OTHER_PLAYER_LEFT,
	GAME_DRAWN,
	GAME_LOST,
	GAME_WON
};
class tic_tac_toe : public olc::PixelGameEngine {
public:
	tic_tac_toe() {
		sAppName = "Online Tic tac toe";
	}
public:
	void DrawMainMenu() {
		if (MenuStatus == main_menu_status::PLAY_GAME) {
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2, this->Option1, olc::RED);
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2 + 20, this->Option2);
		}
		else if (MenuStatus == main_menu_status::EXIT_GAME) {
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2, this->Option1);
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2 + 20, this->Option2, olc::RED);
		}
	}
	void DrawWaitingMenu() {
		this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2, this->WaitingMessage, olc::RED);
	}
	void DrawGameInterruptedMenu() {
		this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2, this->GameInterruptedMessage, olc::YELLOW);
		if (this->InterruptedMenuStatus == interrupted_menu_status::BACK_TO_MAIN_MENU_INTERRUPTED) {
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2 + 20, this->InterruptedOption1, olc::RED);
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2 + 40, this->InterruptedOption2);
		}
		else if (this->InterruptedMenuStatus == interrupted_menu_status::EXIT_GAME_INTERRUPTED) {
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2 + 20, this->InterruptedOption1);
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2 + 40, this->InterruptedOption2, olc::RED);
		}
	}
	void DrawFinishedGame() {
		switch (this->GameStatus) {
		case game_status::GAME_DRAWN: {
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2, this->GameDrawnMessage, olc::YELLOW);
			break;
		}
		case game_status::GAME_LOST: {
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2, this->GameLostMessage, olc::YELLOW);
			break;
		}
		case game_status::GAME_WON: {
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2, this->GameWonMessage, olc::YELLOW);
			break;
		}
		}
		if (this->DrawnGameMenuStatus == game_finished_menu_status::BACK_TO_MAIN_MENU_DRAWN_GAME) {
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2 + 20, this->GameDrawnOption1, olc::RED);
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2 + 40, this->GameDrawnOption2);
		}
		else if (this->DrawnGameMenuStatus == game_finished_menu_status::EXIT_GAME_DRAWN_GAME) {
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2 + 20, this->GameDrawnOption1);
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2 + 40, this->GameDrawnOption2, olc::RED);
		}
	}
	void DrawGameSprites(char C, int TopLeftX, int TopLeftY, int BottomRightX, int BottomRightY) {
		switch (C) {
		case 'X': {
			this->DrawLine(TopLeftX, TopLeftY, BottomRightX, BottomRightY);
			this->DrawLine(TopLeftX, BottomRightY, BottomRightX, TopLeftY);
			break;
		}
		case 'O': {
			int Radius = (BottomRightX - TopLeftX) / 2;
			this->DrawCircle(TopLeftX + Radius, TopLeftY + Radius, Radius);

			break;
		}
		case ' ': {
			this->DrawLine(TopLeftX, BottomRightY, BottomRightX, BottomRightY);
			break;
		}
		}
	}
	void DrawGame() {
		for (int x = 0; x < 3; x++) {
			for (int y = 0; y < 3; y++) {
				int mappedX = x * 200;
				int mappedY = y * 200;
				char C = this->Game.Board[x + y * 3];
				DrawGameSprites(C, mappedX, mappedY, mappedX + 150, mappedY + 150);
			}
		}

	}
	bool OnUserCreate()override {
		this->ConnectedToServer = this->ConnectoToServer();
		if (!this->ConnectedToServer) {
			Status = "Failed to connect to Server";
			this->ConnectionStatus = connection_status::FAILED;
		}
		else {
			Status = "Server connected!";
			this->ConnectionStatus = connection_status::CONNECTED;
		}
		this->Client.on(tcp_common::message_type::GAME_CREATED, [&](std::vector<int> Data) {
			this->GameStatus = game_status::PLAYING;
			game::reset_game(&this->Game);
			this->Game.Id = Data[0];
			this->Side = (game::player_type)Data[1];
		});
		this->Client.on(tcp_common::message_type::OTHER_PLAYER_LEFT, [&](std::vector<int> Data) {
			this->GameStatus = game_status::OTHER_PLAYER_LEFT;
			this->GameInterruptedMessage = "Other player has left the game!";

		});
		this->Client.on(tcp_common::message_type::MARK, [&](std::vector<int> Data) {
			int X = Data[0];
			int Y = Data[1];
			char C;
			if (this->Side == game::player_type::CIRCLE) {
				C = 'X';
			}
			else if (this->Side == game::player_type::CROSS) {
				C = 'O';
			}
			this->Game.Board[X + Y * 3] = C;
			this->Game.NumberOfMoves++;
		});

		this->Client.on(tcp_common::message_type::DRAW, [&](std::vector<int>Data) {
			this->GameStatus = game_status::GAME_DRAWN;
		});
		this->Client.on(tcp_common::message_type::LOSER, [&](std::vector<int> Data) {
			this->GameStatus = game_status::GAME_LOST;
		});
		this->Client.on(tcp_common::message_type::WINNER, [&](std::vector<int>Data) {
			this->GameStatus = game_status::GAME_WON;
		});

		return true;
	}
	bool OnUserUpdate(float ElapsedTime) override {
		Clear(olc::BLACK);
		this->DrawString(ScreenWidth() - Status.size() * PixelPerAlphabet, 0, Status, olc::YELLOW, 1U);
		if (this->ConnectionStatus == connection_status::CONNECTED) {
			switch (this->GameStatus) {
			case game_status::NOT_PLAYING: {
				this->DrawMainMenu();
				if (GetKey(olc::Key::DOWN).bPressed) {
					this->MenuStatus = this->MenuStatus == main_menu_status::PLAY_GAME ? main_menu_status::EXIT_GAME : main_menu_status::PLAY_GAME;
				}
				if (GetKey(olc::Key::UP).bPressed) {
					this->MenuStatus = this->MenuStatus == main_menu_status::PLAY_GAME ? main_menu_status::EXIT_GAME : main_menu_status::PLAY_GAME;
				}
				if (GetKey(olc::ENTER).bPressed) {
					if (this->MenuStatus == main_menu_status::EXIT_GAME) {
						return false;
					}
					else if (this->MenuStatus == main_menu_status::PLAY_GAME) {
						std::vector<int> Data;
						Data.push_back(0);
						Data.push_back(0);
						if (this->Client.send_message(tcp_common::create_message(tcp_common::CREATE_GAME, Data))) {
							this->GameStatus = game_status::WAITING_FOR_RESPONSE;
						}
					}
				}
				break;
			}
			case game_status::WAITING_FOR_RESPONSE: {
				this->DrawWaitingMenu();
				break;
			}
			case game_status::PLAYING: {
				this->DrawGame();
				if (GetMouse(0).bPressed) {
					int X = GetMouseX();
					int Y = GetMouseY();
					if (X > 0 && X <= 600 &&
						Y > 0 && Y <= 600) {
						int MappedX = X / 200;
						int MappedY = Y / 200;
						std::vector<int> Data;
						Data.push_back(MappedX);
						Data.push_back(MappedY);
						char C;
						if (this->Side == game::player_type::CIRCLE) {
							C = 'O';
						}
						else if (this->Side == game::player_type::CROSS) {
							C = 'X';
						}
						bool MyMove = true;
						if (this->Game.NumberOfMoves % 2 == 0 && this->Side == game::player_type::CROSS) {
							MyMove = false;
						}
						if (this->Game.NumberOfMoves % 2 != 0 && this->Side == game::player_type::CIRCLE) {
							MyMove = false;
						}
						if (MyMove &&
							this->Game.Board[MappedX + MappedY * 3] == ' ') {
							this->Game.Board[MappedX + MappedY * 3] = C;
							this->Game.NumberOfMoves++;
							this->Client.send_message(tcp_common::create_message(tcp_common::MARK, Data));
						}
					}
				}
				break;
			}
			case game_status::OTHER_PLAYER_LEFT: {
				this->DrawGameInterruptedMenu();
				if (GetKey(olc::Key::DOWN).bPressed) {
					this->InterruptedMenuStatus = this->InterruptedMenuStatus == interrupted_menu_status::BACK_TO_MAIN_MENU_INTERRUPTED ? interrupted_menu_status::EXIT_GAME_INTERRUPTED : interrupted_menu_status::BACK_TO_MAIN_MENU_INTERRUPTED;
				}
				if (GetKey(olc::Key::UP).bPressed) {
					this->InterruptedMenuStatus = this->InterruptedMenuStatus == interrupted_menu_status::BACK_TO_MAIN_MENU_INTERRUPTED ? interrupted_menu_status::EXIT_GAME_INTERRUPTED : interrupted_menu_status::BACK_TO_MAIN_MENU_INTERRUPTED;
				}
				if (GetKey(olc::ENTER).bPressed) {
					if (this->InterruptedMenuStatus == interrupted_menu_status::EXIT_GAME_INTERRUPTED) {
						return false;
					}
					else if (this->InterruptedMenuStatus == interrupted_menu_status::BACK_TO_MAIN_MENU_INTERRUPTED) {
						this->GameStatus = game_status::NOT_PLAYING;
					}
				}
				break;
			}
			case game_status::GAME_LOST: {

			}
			case game_status::GAME_WON: {

			}
			case game_status::GAME_DRAWN: {
				this->DrawGame();
				this->DrawFinishedGame();
				if (GetKey(olc::Key::DOWN).bPressed) {
					this->DrawnGameMenuStatus = this->DrawnGameMenuStatus == game_finished_menu_status::BACK_TO_MAIN_MENU_DRAWN_GAME ? game_finished_menu_status::EXIT_GAME_DRAWN_GAME : game_finished_menu_status::BACK_TO_MAIN_MENU_DRAWN_GAME;
				}
				if (GetKey(olc::Key::UP).bPressed) {
					this->DrawnGameMenuStatus = this->DrawnGameMenuStatus == game_finished_menu_status::BACK_TO_MAIN_MENU_DRAWN_GAME ? game_finished_menu_status::EXIT_GAME_DRAWN_GAME : game_finished_menu_status::BACK_TO_MAIN_MENU_DRAWN_GAME;
				}
				if (GetKey(olc::ENTER).bPressed) {
					if (this->DrawnGameMenuStatus == game_finished_menu_status::EXIT_GAME_DRAWN_GAME) {
						return false;
					}
					else if (this->DrawnGameMenuStatus == game_finished_menu_status::BACK_TO_MAIN_MENU_DRAWN_GAME) {
						this->GameStatus = game_status::NOT_PLAYING;
					}
				}
				break;
			}
			}


		}
		return true;
	}
	bool OnUserDestroy()override {
		this->Client.stop();
		if (this->Thread.joinable()) {
			this->Thread.join();
		}
		return true;
	}
	bool ConnectoToServer() {
		bool Success = true;
		Success = this->Client.initialize();
		if (Success) {
			Success = this->Client.connect_to_server("127.0.0.1", "27015");
		}
		if (Success) {
			this->Thread = std::thread([&](void) {
				while (this->Client.Running.load()) {
					if (!this->Client.listen_on_port()) {
						this->Client.stop();
					}
				}
			});
		}
		else {
			this->Client.stop();
		}
		return Success;
	}
private:
	std::string Status;

	std::thread Thread;
	net_client::client Client;
	bool ConnectedToServer = false;
	connection_status ConnectionStatus = connection_status::CONNECTING;
private:
	//Misc
	const unsigned int PixelPerAlphabet = 8;
private:
	//Main menu
	std::string Option1 = "1. Create a new game";
	std::string Option2 = "2. Exit";
	//Waiting menu
	std::string WaitingMessage = "Waiting for server!";
	main_menu_status MenuStatus = main_menu_status::PLAY_GAME;
	//Game interrupted menu
	std::string GameInterruptedMessage;
	std::string InterruptedOption1 = "1. Go back to main menu.";
	std::string InterruptedOption2 = "2. Exit the game.";
	interrupted_menu_status InterruptedMenuStatus = interrupted_menu_status::BACK_TO_MAIN_MENU_INTERRUPTED;
	//Game Drawn
	std::string GameDrawnMessage = "Game has been drawn!";
	std::string GameLostMessage = "You have lost the game!";
	std::string GameWonMessage = "You have won the game!";
	std::string GameDrawnOption1 = "1. Return to the main menu";
	std::string GameDrawnOption2 = "2. Exit the game";
	game_finished_menu_status DrawnGameMenuStatus = game_finished_menu_status::BACK_TO_MAIN_MENU_DRAWN_GAME;
private:
	game::game_t Game;
	game::player_type Side;
	//Game status
	game_status GameStatus = game_status::NOT_PLAYING;
};
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow) {
	tic_tac_toe TicTacToe;
	if (TicTacToe.Construct(800, 600, 1, 1)) {
		TicTacToe.Start();
	}
}