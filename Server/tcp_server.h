#pragma once
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <thread>
#include <mutex>
#include <vector>
#include <iostream>
#include <string>
#include <deque>
#include "tcp_common.h"
#include "Game.h"
// Need to link with Ws2_32.lib

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
	public:
		bool start(const char* PortNumber);
		bool listen_on_port();
		void stop();
		bool send_message(int Id,
			std::vector<unsigned char> msg);
		void brodcast_message();
	private:
		WSADATA WSAData;
		SOCKET ListenSocket;
		SOCKET ClientSocket;
		struct addrinfo *Result = NULL;
		struct addrinfo Hints;
		errors Error;
		int ErrorCode;
		
	private:
		bool Running = true;
	public:
		int ClientId = 0;
		int GameId = 0;
		std::thread ClientProcessingThread;
		std::thread MessageProcessingThread;
	public:
		std::mutex ClientsMutex;
		std::vector<tcp_common::client> Clients;
		std::mutex MessageMutex;
		std::mutex GameCreationMessageMutex;
		std::deque<tcp_common::msg> Messages;
		std::deque<tcp_common::msg> GameCreationMessage;
		std::vector<game::game_t> Games;
	};
	bool server::start(const char* portNumber) {
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
			return 1;
		}
		freeaddrinfo(Result);
	}
	bool server::listen_on_port() {
		this->ClientProcessingThread = std::thread(process_clients, this);
		this->MessageProcessingThread = std::thread(process_messages, this);
		SOCKADDR_IN AddrIn;
		int AddrLength = sizeof(AddrIn);
		while (Running) {
			ErrorCode = listen(ListenSocket, SOMAXCONN);
			if (ErrorCode == SOCKET_ERROR) {
				Error = errors::LISTEN_FAILED;
				ErrorCode = WSAGetLastError();
				closesocket(ListenSocket);
				WSACleanup();
				return false;
			}
			// Accept a client socket
			ClientSocket = accept(ListenSocket, (SOCKADDR*)&AddrIn, &AddrLength);
			if (ClientSocket == INVALID_SOCKET) {
				Error = errors::ACCEPT_FAILED;
				closesocket(ListenSocket);
				WSACleanup();
				return false;
			}
			u_long mode = 1;
			if (ioctlsocket(ClientSocket, FIONBIO, &mode) != 0) {
				std::cout << "Async failed with error: %d\n" << WSAGetLastError();
				Error = errors::ASYNC_CREATION_FAILED;
				ErrorCode = WSAGetLastError();
				closesocket(ListenSocket);
				WSACleanup();
				return false;
			}
			tcp_common::client Client;
			Client.Id = this->ClientId++;
			Client.Socket = ClientSocket;
			this->Clients.push_back(Client);
			this->brodcast_message();
		}
		return true;
	}
	void server::stop() {
		closesocket(ListenSocket);
		WSACleanup();
	}

	void server::brodcast_message() {
		this->ClientsMutex.lock();
		for (int i = 0; i < this->Clients.size(); i++) {
			tcp_common::client * Client = &this->Clients[i];
			if (!Client->Active)continue;
			//this->send_message(Client);
		}
		this->ClientsMutex.unlock();
	}

	bool server::send_message(int ClientId,std::vector<unsigned char> Msg) {
		tcp_common::client Client;
		this->ClientsMutex.lock();
		for (auto ClientIt = this->Clients.begin(); ClientIt != this->Clients.end(); ClientIt++) {
			if (ClientIt->Id == ClientId) {
				Client = *ClientIt;
			}
		}
		this->ClientsMutex.unlock();
		int BytesToSend = Msg.size();
		unsigned char* Ptr = (unsigned char*)Msg.data();
		int BytesSent = 0;
		while (BytesSent < BytesToSend) {
			BytesSent = send(Client.Socket, (const char*)Ptr, BytesToSend - BytesSent, 0);
			if (BytesSent == SOCKET_ERROR) {
				std::cout << "Send failed with error: " << WSAGetLastError();
				closesocket(Client.Socket);
				Client.Active = false;
				return false;
			}
			Ptr += BytesSent;
		}
		return true;
	}

	static void process_clients(server * Server) {
		while (1) {
			for (int i = 0; i < Server->Clients.size(); i++) {
				if (!Server->Clients[i].Active) {
					Server->ClientsMutex.lock();
					Server->Clients.erase(Server->Clients.begin() + i);
					Server->ClientsMutex.unlock();
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
					shutdown(ClientSocket, SD_SEND);
					Server->Clients[i].Active = false;
					closesocket(ClientSocket);
				}
				else if (WSAGetLastError() != WSAEWOULDBLOCK) {
					std::cout << "recv failed with error: " << WSAGetLastError();
					Server->Clients[i].Active = false;
					closesocket(ClientSocket);
				}
				if (Server->Clients[i].Active) {
					while (Server->Clients[i].ByteStream.size() >= tcp_common::SIZE_OF_MESSAGE) {
						int HeaderSize = sizeof(tcp_common::message_type);
						tcp_common::msg Message;
						Message.Header = (tcp_common::message_type)Server->Clients[i].ByteStream[0];
						for (int k = HeaderSize; k < tcp_common::SIZE_OF_MESSAGE; k++) {
							Message.MsgBody.push_back(Server->Clients[i].ByteStream[k]);
						}
						Message.ClientId = Server->Clients[i].Id;
						Server->MessageMutex.lock();
						Server->Messages.push_back(Message);
						Server->MessageMutex.unlock();
						auto Begin = Server->Clients[i].ByteStream.begin();
						Server->Clients[i].ByteStream.erase(Begin, Begin + tcp_common::SIZE_OF_MESSAGE);
					}
				}
			}
			Sleep(1);
		}
	}
	static void process_messages(server * Server) {
		while (1) {
			tcp_common::msg Message;
			if (Server->Messages.size() > 0) {
				Server->MessageMutex.lock();
				auto MessageTemp = Server->Messages.begin();
				Message.ClientId = MessageTemp->ClientId;
				Message.MsgBody = MessageTemp->MsgBody;
				Message.Header = MessageTemp->Header;
				Server->Messages.pop_front();
				Server->MessageMutex.unlock();
				
				tcp_common::message_type Header = Message.Header;
				switch (Header) {
				case tcp_common::message_type::CREATE_GAME: {
					Server->GameCreationMessage.push_back(Message);
					break;
				}
				case tcp_common::message_type::MARK: {
					tcp_common::decode_data(Header, Message.MsgBody);
					break;
				}
				}
				
			}
			if (Server->GameCreationMessage.size() >= 2) {
				game::game_t Game;
				Game.Id = Server->GameId++;
				Game.ClientsIds.push_back(Server->GameCreationMessage[0].ClientId);
				Game.ClientsIds.push_back(Server->GameCreationMessage[1].ClientId);
				Game.GameState = game::game_state::ACTIVE;
				Server->Games.push_back(Game);
				std::vector<int> data;
				data.push_back(Game.Id);
				data.push_back(0);
				Server->send_message(Game.ClientsIds[0], tcp_common::create_message(tcp_common::GAME_CREATED, data));
				Server->send_message(Game.ClientsIds[1], tcp_common::create_message(tcp_common::GAME_CREATED, data));
				Server->GameCreationMessage.pop_front();
				Server->GameCreationMessage.pop_front();
			}
			Sleep(1);
		}
	}
}