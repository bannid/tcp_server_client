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
		ASYNC_CREATION_FAILED
	};

	class server;
	static void process_messages(server * Server);
	static void process_clients(server * Server);
	class server {
		friend void process_messages(server * Server);
		friend void process_clients(server * Server);
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
		void push_message(tcp_common::msg Message);
		tcp_common::msg pop_front_message();
	private:
		WSADATA WSAData;
		SOCKET ListenSocket;
		SOCKET ClientSocket;
		struct addrinfo *Result = NULL;
		struct addrinfo Hints;
		errors Error;
		int ErrorCode;

	public:
		std::atomic<bool> Running = true;
	private:
		HANDLE MessageSemaphore;
		int ClientId = 0;
		int GameId = 0;
		std::thread ClientProcessingThread;
		std::thread MessageProcessingThread;
	private:
		std::mutex ClientsMutex;
		std::vector<tcp_common::client> Clients;
		std::mutex MessageMutex;
		std::mutex GameCreationMessageMutex;
		std::deque<tcp_common::msg> Messages;
		std::deque<tcp_common::msg> GameCreationMessage;
		std::vector<game::game_t> Games;
	};
	bool server::start(const char* portNumber) {
		this->MessageSemaphore = CreateSemaphoreExA(
			NULL,
			0,
			1000,
			NULL,
			NULL,
			NULL
		);
		ErrorCode = WSAStartup(MAKEWORD(2, 2), &WSAData);
		if (ErrorCode != 0) {
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
			Error = errors::ADDRESS_RESO_FAILED;
			WSACleanup();
			return false;
		}
		ListenSocket = socket(Result->ai_family, Result->ai_socktype, Result->ai_protocol);
		if (ListenSocket == INVALID_SOCKET) {
			freeaddrinfo(Result);
			Error = LISTEN_SOCKET_FAILED;
			ErrorCode = WSAGetLastError();
			return false;
		}
		ErrorCode = bind(ListenSocket, Result->ai_addr, (int)Result->ai_addrlen);
		if (ErrorCode == SOCKET_ERROR) {
			Error = errors::BIND_FAILED;
			ErrorCode = WSAGetLastError();
			freeaddrinfo(Result);
			closesocket(ListenSocket);
			WSACleanup();
			return false;
		}
		freeaddrinfo(Result);
	}

	void server::push_message(tcp_common::msg Message) {
		std::lock_guard<std::mutex> LocalGuard(this->MessageMutex);
		this->Messages.push_back(Message);
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
								this->set_client_game_thread_unsafe(*It2, -1,game::player_type::CIRCLE);
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
			else{
				shutdown(ClientIt->Socket, SD_SEND);
				closesocket(ClientIt->Socket);
			}
		}
	}
	void server::stop() {
		this->Running = false;
		this->ClientProcessingThread.join();
		this->MessageProcessingThread.join();
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
				this->deactivate_client(Client.Id,false);
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
			WaitForSingleObjectEx(Server->MessageSemaphore, INFINITE, NULL);
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
				if ( ClientFirstFound &&
					 ClientSecondFound &&
					ClientFirst.Active &&
					ClientSecond.Active) {
					game::game_t Game;
					Game.Id = Server->GameId++;
					Game.ClientsIds.push_back(Server->GameCreationMessage[0].ClientId);
					Game.ClientsIds.push_back(Server->GameCreationMessage[1].ClientId);
					Game.GameState = game::game_state::ACTIVE;
					Server->Games.push_back(Game);
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
}