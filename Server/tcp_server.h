#pragma once
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <iostream>
#include <string>
#include <deque>
#include <assert.h>
#include "tcp_common.h"
#include "ts_vector.h"
#include "Game.h"

namespace net_server {
	enum errors {
		WSA_STARTUP_FAILED,
		ADDRESS_RESO_FAILED,
		LISTEN_SOCKET_FAILED,
		LISTEN_FAILED,
		BIND_FAILED,
		ACCEPT_FAILED,
		ASYNC_CREATION_FAILED,
		SUCCESS
	};

	class server;
	static void process_messages(server * Server);
	static void process_clients(server * Server);
	static void process_games(server * Server);
	class server {
		friend void process_messages(server * Server);
		friend void process_clients(server * Server);
		friend void process_games(server * Server);
	public:
		bool start(const char* PortNumber);
		bool listen_on_port();
		void stop();
		bool send_message(tcp_common::client Client,
			std::vector<unsigned char> msg);
		void brodcast_message();
		void add_client(tcp_common::client Client);
		void deactivate_client(int ClientId, bool ClosedGracefully);
		void close_inactive_clients();
		void close_all_clients();
		void set_client_game(int ClientId, int GameId, game::player_type Side);
		void set_client_game_thread_unsafe(int ClientId, int GameId, game::player_type Side);

		bool get_client(int ClientId, tcp_common::client& Output);
		bool get_client_thread_unsafe(int ClientId, tcp_common::client& Output);
		int get_number_of_clients();
		bool get_game(int Id, game::game_t& Game);
		bool mark_on_game(int Id, int x, int y, game::player_type Side);
		void push_message(tcp_common::msg Message);
		tcp_common::msg pop_front_message();
		void push_game(game::game_t Game);
		errors get_last_error();
		errors Error = errors::SUCCESS;
		int ErrorCode;
	private:
		WSADATA WSAData;
		SOCKET ListenSocket;
		SOCKET ClientSocket;
		struct addrinfo *Result = NULL;
		struct addrinfo Hints;


	public:
		std::atomic<bool> Running = true;
	private:
		HANDLE MessageSemaphore;
		HANDLE GamesSempahore;
		int ClientId = 0;
		int GameId = 0;
		std::thread ClientProcessingThread;
		std::thread MessageProcessingThread;
		std::thread GamesProcessingThread;
	private:
		std::mutex ClientsMutex;
		std::vector<tcp_common::client> Clients;
		std::mutex MessageMutex;
		std::deque<tcp_common::msg> Messages;
		std::deque<tcp_common::msg> GameCreationMessage;
		std::mutex GamesMutex;
		std::vector<game::game_t> Games;
	};
	bool server::start(const char* portNumber) {
		this->MessageSemaphore = CreateSemaphoreExA(
			NULL,
			0,
			1000,
			NULL,
			NULL,
			SEMAPHORE_ALL_ACCESS
		);
		this->GamesSempahore = CreateSemaphoreExA(
			NULL,
			0,
			1000,
			NULL,
			NULL,
			SEMAPHORE_ALL_ACCESS
		);
		ErrorCode = WSAStartup(MAKEWORD(2, 2), &WSAData);
		if (ErrorCode != 0) {
			std::cout << "[Debug]WSA initialization failed\n";
			Error = errors::WSA_STARTUP_FAILED;
			return false;
		}
		ZeroMemory(&Hints, sizeof(Hints));
		Hints.ai_family = AF_INET;
		Hints.ai_socktype = SOCK_STREAM;
		Hints.ai_protocol = IPPROTO_TCP;
		Hints.ai_flags = AI_PASSIVE;

		// Resolve the server address and port
		ErrorCode = getaddrinfo(NULL, portNumber, &Hints, &Result);
		if (ErrorCode != 0) {
			std::cout << "[Debug]Address resolution failed\n";
			Error = errors::ADDRESS_RESO_FAILED;
			WSACleanup();
			return false;
		}
		ListenSocket = socket(Result->ai_family, Result->ai_socktype, Result->ai_protocol);
		if (ListenSocket == INVALID_SOCKET) {
			std::cout << "[Debug]Creation of listen socket failed\n";
			freeaddrinfo(Result);
			Error = LISTEN_SOCKET_FAILED;
			ErrorCode = WSAGetLastError();
			return false;
		}
		ErrorCode = bind(ListenSocket, Result->ai_addr, (int)Result->ai_addrlen);
		if (ErrorCode == SOCKET_ERROR) {
			std::cout << "[Debug]Binding listen socket failed failed\n";
			Error = errors::BIND_FAILED;
			ErrorCode = WSAGetLastError();
			freeaddrinfo(Result);
			closesocket(ListenSocket);
			WSACleanup();
			return false;
		}
		freeaddrinfo(Result);
		return true;
	}

	errors server::get_last_error() {
		return this->Error;
	}

	void server::push_message(tcp_common::msg Message) {
		std::lock_guard<std::mutex> LocalGuard(this->MessageMutex);
		this->Messages.push_back(Message);
	}
	void server::push_game(game::game_t Game) {
		std::lock_guard<std::mutex> LocalGuard(this->GamesMutex);
		this->Games.push_back(Game);
	}

	tcp_common::msg server::pop_front_message() {
		std::lock_guard<std::mutex> LocalGuard(this->MessageMutex);
		tcp_common::msg ToReturn = *this->Messages.begin();
		this->Messages.pop_front();
		return ToReturn;
	}

	bool server::listen_on_port() {
		this->ClientProcessingThread = std::thread(process_clients, this);
		this->MessageProcessingThread = std::thread(process_messages, this);
		this->GamesProcessingThread = std::thread(process_games, this);
		SOCKADDR_IN AddrIn;
		int AddrLength = sizeof(AddrIn);
		while (this->Running.load() == true) {
			ErrorCode = listen(ListenSocket, SOMAXCONN);
			if (ErrorCode == SOCKET_ERROR) {
				Error = errors::LISTEN_FAILED;
				ErrorCode = WSAGetLastError();
				closesocket(ListenSocket);
				return false;
			}
			// Accept a client socket
			ClientSocket = accept(ListenSocket, (SOCKADDR*)&AddrIn, &AddrLength);
			if (ClientSocket == INVALID_SOCKET) {
				Error = errors::ACCEPT_FAILED;
				closesocket(ListenSocket);
				return false;
			}
			u_long mode = 1;
			if (ioctlsocket(ClientSocket, FIONBIO, &mode) != 0) {
				std::cout << "Async failed with error: %d\n" << WSAGetLastError();
				Error = errors::ASYNC_CREATION_FAILED;
				ErrorCode = WSAGetLastError();
				closesocket(ListenSocket);
				return false;
			}
			tcp_common::client Client;
			Client.Id = this->ClientId++;
			Client.Socket = ClientSocket;
			this->add_client(Client);
		}
		return true;
	}
	bool server::get_game(int Id, game::game_t& Game) {
		for (auto It = this->Games.begin(); It != this->Games.end(); It++) {
			if (It->Id == Id) {
				Game = *It;
				return true;
			}
		}
		return false;
	}

	bool server::mark_on_game(int Id, int x, int y, game::player_type Side) {
		for (auto It = this->Games.begin(); It != this->Games.end(); It++) {
			if (It->Id == Id) {
				char c;
				if (Side == game::player_type::CIRCLE) {
					c = 'O';
				}
				else {
					c = 'X';
				}
				It->Board[y * 3 + x] = c;
				It->NumberOfMoves++;
				return true;
			}
		}
		return false;
	}

	void server::set_client_game(int ClientId, int GameId, game::player_type Side) {
		std::lock_guard<std::mutex> LocalGuard(this->ClientsMutex);
		for (auto ClientIt = this->Clients.begin(); ClientIt != this->Clients.end(); ClientIt++) {
			if (ClientIt->Id == ClientId) {
				ClientIt->GameId = GameId;
				ClientIt->Side = Side;
				break;
			}
		}
	}
	void server::set_client_game_thread_unsafe(int ClientId, int GameId, game::player_type Side) {
		for (auto ClientIt = this->Clients.begin(); ClientIt != this->Clients.end(); ClientIt++) {
			if (ClientIt->Id == ClientId) {
				ClientIt->GameId = GameId;
				ClientIt->Side = Side;
				break;
			}
		}
	}
	bool server::get_client(int ClientId, tcp_common::client& Client) {
		std::lock_guard<std::mutex> LocalGuard(this->ClientsMutex);
		for (auto ClientIt = this->Clients.begin(); ClientIt != this->Clients.end(); ClientIt++) {
			if (ClientIt->Id == ClientId) {
				Client = *ClientIt;
				return true;
			}
		}
		return false;
	}
	bool server::get_client_thread_unsafe(int ClientId, tcp_common::client& Client) {
		for (auto ClientIt = this->Clients.begin(); ClientIt != this->Clients.end(); ClientIt++) {
			if (ClientIt->Id == ClientId) {
				Client = *ClientIt;
				return true;
			}
		}
		return false;
	}
	int server::get_number_of_clients() {
		return this->Clients.size();
	}
	void server::close_inactive_clients() {
		std::lock_guard<std::mutex> LocalGuard(this->ClientsMutex);
		std::vector<std::vector<tcp_common::client>::iterator> IteratorsToRemove;
		for (auto ClientIt = this->Clients.begin(); ClientIt != this->Clients.end(); ClientIt++) {
			if (!ClientIt->Active) {
				if (ClientIt->ClosedGracefully) {
					shutdown(ClientIt->Socket, SD_SEND);
				}
				game::game_t Game;
				if (ClientIt->GameId != -1 &&
					get_game(ClientIt->GameId, Game)) {
					for (auto It2 = Game.ClientsIds.begin(); It2 != Game.ClientsIds.end(); It2++) {
						if (*It2 != ClientIt->Id) {
							std::vector<int> Data;
							Data.push_back(0);
							Data.push_back(0);
							tcp_common::client Client;
							if (get_client_thread_unsafe(*It2, Client) &&
								Client.Active) {
								//Reset the game for client so that he can join
								//another game. Player type doesnt matter
								this->set_client_game_thread_unsafe(*It2, -1, game::player_type::CIRCLE);
								this->send_message(Client, tcp_common::create_message(tcp_common::message_type::OTHER_PLAYER_LEFT, Data));
							}
						}
					}
				}
				closesocket(ClientIt->Socket);
				IteratorsToRemove.push_back(ClientIt);
			}
		}
		for (auto It = IteratorsToRemove.begin(); It != IteratorsToRemove.end(); It++) {
			this->Clients.erase(*It);
		}
	}
	void server::deactivate_client(int Id, bool ClosedGracefully) {
		std::lock_guard<std::mutex> LocalGuard(this->ClientsMutex);
		for (auto ClientIt = this->Clients.begin(); ClientIt != this->Clients.end(); ClientIt++) {
			if (ClientIt->Id == Id) {
				ClientIt->Active = false;
				ClientIt->ClosedGracefully = ClosedGracefully;
			}
		}
	}
	void server::close_all_clients() {
		std::lock_guard<std::mutex> LocalGuard(this->ClientsMutex);
		std::vector<std::vector<tcp_common::client>::iterator> IteratorsToRemove;
		for (auto ClientIt = this->Clients.begin(); ClientIt != this->Clients.end(); ClientIt++) {
			if (!ClientIt->Active) {
				if (ClientIt->ClosedGracefully) {
					shutdown(ClientIt->Socket, SD_SEND);
				}
				closesocket(ClientIt->Socket);
			}
			else {
				shutdown(ClientIt->Socket, SD_SEND);
				closesocket(ClientIt->Socket);
			}
		}
	}
	void server::stop() {
		this->Running = false;
		//Release the semaphore so the waiting threads can exit.
		ReleaseSemaphore(this->MessageSemaphore, 1, 0);
		ReleaseSemaphore(this->GamesSempahore, 1, 0);
		if (this->ClientProcessingThread.joinable()) {
			this->ClientProcessingThread.join();
		}
		if (this->MessageProcessingThread.joinable()) {
			this->MessageProcessingThread.join();
		}
		if (this->GamesProcessingThread.joinable()) {
			this->GamesProcessingThread.join();
		}
		close_all_clients();
		WSACleanup();
	}
	void server::add_client(tcp_common::client Client) {
		std::lock_guard<std::mutex> LocalGuard(this->ClientsMutex);
		this->Clients.push_back(Client);
	}
	void server::brodcast_message() {
		std::lock_guard<std::mutex> LocalGuard(this->ClientsMutex);
		for (int i = 0; i < this->Clients.size(); i++) {
			tcp_common::client * Client = &this->Clients[i];
			if (!Client->Active)continue;
			//this->send_message(Client);
		}
	}

	bool server::send_message(tcp_common::client Client, std::vector<unsigned char> Msg) {
		int BytesToSend = Msg.size();
		unsigned char* Ptr = (unsigned char*)Msg.data();
		int BytesSent = 0;
		while (BytesSent < BytesToSend) {
			BytesSent = send(Client.Socket, (const char*)Ptr, BytesToSend - BytesSent, 0);
			if (BytesSent == SOCKET_ERROR) {
				std::cout << "Send failed with error: " << WSAGetLastError();
				this->deactivate_client(Client.Id, false);
				return false;
			}
			Ptr += BytesSent;
		}
		return true;
	}
	//Running on seprate thread.
	static void process_clients(server * Server) {
		while (Server->Running.load()) {
			auto Size = Server->Clients.size();
			for (int i = 0; i < Size; i++) {
				if (!Server->Clients[i].Active) {
					continue;
				}
				char recvbuf[512];
				int recvbuflen = 512;
				SOCKET ClientSocket = Server->Clients[i].Socket;

				int Result = 0;
				Result = recv(ClientSocket, recvbuf, recvbuflen, 0);
				if (Result > 0) {
					std::string temp(recvbuf, Result);
					std::cout << "[Debug]" << temp.c_str();
					for (int j = 0; j < Result; j++) {
						unsigned char Byte = recvbuf[j];
						Server->Clients[i].ByteStream.push_back(Byte);
					}
				}
				else if (Result == 0) {
					std::cout << "Connection closing...\n";
					Server->deactivate_client(Server->Clients[i].Id, true);
				}
				else if (WSAGetLastError() != WSAEWOULDBLOCK) {
					std::cout << "recv failed with error: " << WSAGetLastError();
					Server->deactivate_client(Server->Clients[i].Id, false);
				}
				if (Server->Clients[i].Active) {
					//Process the messages
					while (Server->Clients[i].ByteStream.size() >= tcp_common::SIZE_OF_MESSAGE) {
						int HeaderSize = sizeof(tcp_common::message_type);
						tcp_common::msg Message;
						Message.Header = (tcp_common::message_type)Server->Clients[i].ByteStream[0];
						for (int k = HeaderSize; k < tcp_common::SIZE_OF_MESSAGE; k++) {
							Message.MsgBody.push_back(Server->Clients[i].ByteStream[k]);
						}
						Message.ClientId = Server->Clients[i].Id;
						Server->push_message(Message);
						ReleaseSemaphore(Server->MessageSemaphore, 1, 0);
						auto Begin = Server->Clients[i].ByteStream.begin();
						Server->Clients[i].ByteStream.erase(Begin, Begin + tcp_common::SIZE_OF_MESSAGE);
					}
				}
			}
			Server->close_inactive_clients();
			Sleep(1);
		}
	}
	static void process_messages(server * Server) {
		while (Server->Running.load()) {
			DWORD Status = WaitForSingleObject(Server->MessageSemaphore, INFINITE);

			if (Status == 0xFFFFFFFF) {
				assert(0);
			}
			tcp_common::msg Message;
			if (Server->Messages.size() > 0) {
				Message = Server->pop_front_message();
				tcp_common::message_type Header = Message.Header;
				switch (Header) {
				case tcp_common::message_type::CREATE_GAME: {
					bool ValidMessage = true;
					for (int i = 0; i < Server->GameCreationMessage.size(); i++) {
						if (Server->GameCreationMessage[i].ClientId == Message.ClientId) {
							tcp_common::client Client;
							if (Server->get_client(Message.ClientId, Client)) {
								std::vector<int> Data;
								Data.push_back(0);
								Data.push_back(0);
								Server->send_message(Client, tcp_common::create_message(tcp_common::message_type::GAME_REQUEST_EXISTS, Data));
								ValidMessage = false;
								break;
							}
						}
					}
					tcp_common::client Client;
					if (!Server->get_client(Message.ClientId, Client)) {
						ValidMessage = false;
					}
					if (ValidMessage &&
						Client.GameId == -1) {
						Server->GameCreationMessage.push_back(Message);
					}
					else if (ValidMessage && Client.GameId != -1) {
						std::vector<int> Data;
						Data.push_back(0);
						Data.push_back(0);
						Server->send_message(Client, tcp_common::create_message(tcp_common::message_type::GAME_ALREADY_EXISTS, Data));
					}
					break;
				}
				case tcp_common::message_type::MARK: {
					std::vector<int> Data = tcp_common::decode_data(Header, Message.MsgBody);
					std::cout << "Mark message received. Value of x and y is: " << Data[0] << ", " << Data[1];
					int x = Data[0];
					int y = Data[1];
					if (x > 2 || x < 0 || y > 2 || y < 0) {

					}
					else {
						tcp_common::client Client;
						if (Server->get_client(Message.ClientId, Client)) {
							if (Server->mark_on_game(Client.GameId, x, y, Client.Side)) {
								game::game_t Game;
								Server->get_game(Client.GameId, Game);
								for (int i = 0; i < 2; i++) {
									if (Game.ClientsIds[i] != Client.Id) {
										tcp_common::client Client2;
										if (Server->get_client(Game.ClientsIds[i], Client2)) {
											Server->send_message(Client2,
												tcp_common::create_message(tcp_common::message_type::MARK, Data));
											ReleaseSemaphore(Server->GamesSempahore, 1, 0);
										}
									}
								}
							}
						}
					}
					break;
				}
				}
			}
			//Create games
			if (Server->GameCreationMessage.size() >= 2) {
				//TODO: We have to see if the both clients are active.
				//If one or both are inactive remove the messages and continue processing.
				tcp_common::client ClientFirst;
				tcp_common::client ClientSecond;
				bool ClientFirstFound = Server->get_client(Server->GameCreationMessage[0].ClientId, ClientFirst);
				bool ClientSecondFound = Server->get_client(Server->GameCreationMessage[1].ClientId, ClientSecond);
				if (ClientFirstFound &&
					ClientSecondFound &&
					ClientFirst.Active &&
					ClientSecond.Active) {
					game::game_t Game;
					Game.Id = Server->GameId++;
					Game.ClientsIds.push_back(Server->GameCreationMessage[0].ClientId);
					Game.ClientsIds.push_back(Server->GameCreationMessage[1].ClientId);
					Game.GameState = game::game_state::ACTIVE;
					Server->push_game(Game);
					ReleaseSemaphore(Server->GamesSempahore, 1, 0);
					Server->set_client_game(Game.ClientsIds[0], Game.Id, game::player_type::CIRCLE);
					Server->set_client_game(Game.ClientsIds[1], Game.Id, game::player_type::CROSS);
					std::vector<int> data1;
					data1.push_back(Game.Id);
					data1.push_back(game::player_type::CIRCLE);

					Server->send_message(ClientFirst, tcp_common::create_message(tcp_common::GAME_CREATED, data1));
					std::vector<int> data2;
					data2.push_back(Game.Id);
					data2.push_back(game::player_type::CROSS);
					Server->send_message(ClientSecond, tcp_common::create_message(tcp_common::GAME_CREATED, data2));
					Server->GameCreationMessage.pop_front();
					Server->GameCreationMessage.pop_front();
				}
				else if (!ClientFirstFound || !ClientFirst.Active) {
					Server->GameCreationMessage.pop_front();
				}
				else if (!ClientSecondFound || !ClientSecond.Active) {
					Server->GameCreationMessage.erase(Server->GameCreationMessage.begin() + 1);
				}
			}
		}
	}
	static void process_games(server * Server) {
		int WinningPatterns[] = {
					0,0, 1,0 ,2,0,
					0,1, 1,1 ,2,1,
					0,2, 1,2 ,2,2,

					0,0, 0,1, 0,2,
					1,0, 1,1, 1,2,
					2,0, 2,1, 2,2,

					0,0, 1,1, 2,2,
					0,2, 1,1, 2,0

		};
		while (Server->Running.load()) {
			DWORD Status = WaitForSingleObject(Server->GamesSempahore, INFINITE);
			if (Status == 0xFFFFFFFF) {
				assert(0);
			}
			std::lock_guard<std::mutex> LocalGuard(Server->GamesMutex);
			std::vector<std::vector<game::game_t>::iterator> GamesToRemove;
			for (int i = 0; i < Server->Games.size(); i++) {
				bool GameWon = false;
				//Check if the game is won
				for (int j = 0; j < ARRAYSIZE(WinningPatterns); j += 6) {
					int XFirstIndex = j;
					int XSecondIndex = j + 2;
					int XThirdIndex = j + 4;
					int YFirstIndex = j + 1;
					int YSecondIndex = j + 3;
					int YThirdIndex = j + 5;

					int XFirst = WinningPatterns[XFirstIndex];
					int XSecond = WinningPatterns[XSecondIndex];
					int XThird = WinningPatterns[XThirdIndex];

					int YFirst = WinningPatterns[YFirstIndex];
					int YSecond = WinningPatterns[YSecondIndex];
					int YThird = WinningPatterns[YThirdIndex];

					assert(XFirst >= 0 && XFirst <= 2);
					assert(XSecond >= 0 && XSecond <= 2);
					assert(XThird >= 0 && XThird <= 2);

					assert(YFirst >= 0 && YFirst <= 2);
					assert(YSecond >= 0 && YSecond <= 2);
					assert(YThird >= 0 && YThird <= 2);

					if (Server->Games[i].Board[XFirst + YFirst * 3] == Server->Games[i].Board[XSecond + YSecond * 3] &&
						Server->Games[i].Board[XSecond + YSecond * 3] == Server->Games[i].Board[XThird + YThird * 3] &&
						Server->Games[i].Board[XFirst + YFirst * 3] != ' ') {
						std::cout << "[Debug]Game has been won!\n";
						char C = Server->Games[i].Board[XFirst + YFirst * 3];
						game::player_type WinnerType = C == 'X' ? game::player_type::CROSS : game::player_type::CIRCLE;
						tcp_common::client Client;
						if (Server->get_client(Server->Games[i].ClientsIds[0], Client)) {
							std::vector<int> Data;
							Data.push_back(0);
							Data.push_back(0);
							if (Client.Side == WinnerType) {
								Server->send_message(Client, tcp_common::create_message(tcp_common::WINNER, Data));
							}
							else {
								Server->send_message(Client, tcp_common::create_message(tcp_common::LOSER, Data));
							}
						}
						if (Server->get_client(Server->Games[i].ClientsIds[1], Client)) {
							std::vector<int> Data;
							Data.push_back(0);
							Data.push_back(0);
							if (Client.Side == WinnerType) {
								Server->send_message(Client, tcp_common::create_message(tcp_common::WINNER, Data));
							}
							else {
								Server->send_message(Client, tcp_common::create_message(tcp_common::LOSER, Data));
							}
						}
						GamesToRemove.push_back(Server->Games.begin() + i);
						GameWon = true;
						break;
					}
				}
				//If the game is won, then no need for further processing.
				if (GameWon)break;

				bool GameOver = true;
				for (int jk = 0; jk < 9; jk++) {
					GameOver = !(Server->Games[i].Board[jk] == ' ');
					if (!GameOver) {
						break;
					}
				}
				if (GameOver) {
					std::cout << "[Debug]Game is drawn!\n";
					GamesToRemove.push_back(Server->Games.begin() + i);
					tcp_common::client Client;
					std::vector<int> Data;
					Data.push_back(0);
					Data.push_back(0);
					if (Server->get_client(Server->Games[i].ClientsIds[0], Client)) {
						Server->send_message(Client, tcp_common::create_message(tcp_common::message_type::DRAW, Data));
					}
					if (Server->get_client(Server->Games[i].ClientsIds[1], Client)) {
						Server->send_message(Client, tcp_common::create_message(tcp_common::message_type::DRAW, Data));
					}
				}
			}
			//Remove the won and drawn games.
			for (auto It = GamesToRemove.begin(); It != GamesToRemove.end(); It++) {
				Server->set_client_game((*It)->ClientsIds[0], -1, game::player_type::CIRCLE);
				Server->set_client_game((*It)->ClientsIds[1], -1, game::player_type::CIRCLE);
				Server->Games.erase(*It);
			}
		}
	}
}