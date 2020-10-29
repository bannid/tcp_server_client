#include "olcPixelGameEngine.h"
#include "tcp_client.h"
#include "Game.h"

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
	BACK_TO_MAIN_MENU,
	EXIT_GAME_INTERRUPTED
};
enum game_status {
	PLAYING,
	NOT_PLAYING,
	WAITING_FOR_RESPONSE,
	OTHER_PLAYER_LEFT
};
class tic_tac_toe : public olc::PixelGameEngine {
public:
	tic_tac_toe() {
		sAppName = "Online Tic tac toe";
	}
public:
	bool OnUserCreate()override {
		Status = "Connecting to server";
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
			this->Side = (game::player_type)Data[1];
		});
		this->Client.on(tcp_common::message_type::OTHER_PLAYER_LEFT, [&](std::vector<int> Data) {
			this->GameStatus = game_status::OTHER_PLAYER_LEFT;
			this->GameInterruptedMessage = "Other player has left the game!";

		});
		return true;
	}
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
		if (this->InterruptedMenuStatus == interrupted_menu_status::BACK_TO_MAIN_MENU) {
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2 + 20, this->InterruptedOption1, olc::RED);
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2 + 40, this->InterruptedOption2);
		}
		else if (this->InterruptedMenuStatus == interrupted_menu_status::EXIT_GAME_INTERRUPTED) {
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2 + 20, this->InterruptedOption1);
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2 + 40, this->InterruptedOption2,olc::RED);
		}
	}
	void DrawGame() {
		std::string temp = "Connected to the game";
		this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2, temp, olc::RED);
		if (this->Side == game::player_type::CIRCLE) {
			std::string temp = "You are circle!!";
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2 + 20, temp, olc::RED);
		}
		else if (this->Side == game::player_type::CROSS) {
			std::string temp = "You are cross!!";
			this->DrawString(ScreenWidth() / 2 - (this->Option1.size() * PixelPerAlphabet / 2), ScreenHeight() / 2 + 20, temp, olc::RED);
		}
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
				break;
			}
			case game_status::OTHER_PLAYER_LEFT: {
				this->DrawGameInterruptedMenu();
				if (GetKey(olc::Key::DOWN).bPressed) {
					this->InterruptedMenuStatus = this->InterruptedMenuStatus == interrupted_menu_status::BACK_TO_MAIN_MENU? interrupted_menu_status::EXIT_GAME_INTERRUPTED : interrupted_menu_status::BACK_TO_MAIN_MENU;
				}
				if (GetKey(olc::Key::UP).bPressed) {
					this->InterruptedMenuStatus = this->InterruptedMenuStatus == interrupted_menu_status::BACK_TO_MAIN_MENU ? interrupted_menu_status::EXIT_GAME_INTERRUPTED : interrupted_menu_status::BACK_TO_MAIN_MENU;
				}
				if (GetKey(olc::ENTER).bPressed) {
					if (this->InterruptedMenuStatus == interrupted_menu_status::EXIT_GAME_INTERRUPTED) {
						return false;
					}
					else if (this->InterruptedMenuStatus == interrupted_menu_status::BACK_TO_MAIN_MENU) {
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
	interrupted_menu_status InterruptedMenuStatus = interrupted_menu_status::BACK_TO_MAIN_MENU;
private:
	game::game_t Game;
	game::player_type Side;
	//Game status
	game_status GameStatus = game_status::NOT_PLAYING;
};
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow) {
	tic_tac_toe TicTacToe;
	if (TicTacToe.Construct(400, 400, 1, 1)) {
		TicTacToe.Start();
	}
}