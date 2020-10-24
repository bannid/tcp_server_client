#pragma once
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <vector>
#include <iostream>
#include <string>
#include "tcp_common.h"
// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
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
	struct client {
		SOCKET Socket;
		int Id;
		bool Active = true;
	};
	struct msg {
		client* Client;//Client who owns the message.
		std::vector<char> MsgBody;
	};
	class server;
	static void process_messages(server * Server);
	static void process_clients(server * Server);
	class server {
	public:
		bool start(const char* PortNumber);
		bool listen_on_port();
		void stop();
		void send_message(client Client);
		void send_message(int Id);
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
	private:
		std::thread ClientProcessingThread;
		std::thread MessageProcessingThread;
	public:
		std::mutex ClientsMutex;
		std::mutex MessagesMutex;
		std::vector<client> Clients;
		std::vector<msg> Messages;
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
			client Client;
			//TODO: Create a system to create unique ids.
			Client.Id = this->Clients.size();
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
			if (!this->Clients[i].Active)continue;
			int SendResult;
			std::string Message = "We have ";
			Message.append(std::to_string(this->Clients.size()));
			Message.append(" clients\r\n");
			SendResult = send(this->Clients[i].Socket, Message.data(), Message.size(), 0);
			if (SendResult == SOCKET_ERROR) {
				std::cout << "Send failed with error: " << WSAGetLastError();
				closesocket(this->Clients[i].Socket);
				this->Clients[i].Active = false;
			}
		}
		this->ClientsMutex.unlock();
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
				int Result;
				Result = recv(ClientSocket, recvbuf, recvbuflen, 0);
				int lastError = WSAGetLastError();
				if (Result > 0) {
					msg Message;
					for (int i = 0; i < Result; i++) {
						std::cout << recvbuf[i];
						Message.MsgBody.push_back(recvbuf[i]);
					}
					Message.Client = &Server->Clients[i];
					Server->MessagesMutex.lock();
					Server->Messages.push_back(Message);
					Server->MessagesMutex.unlock();

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
			}
			Sleep(100);
		}
	}
	static void process_messages(server * Server) {
		while (1) {
			for (int i = 0; i < Server->Messages.size(); i++) {
				uint8_t MsgType;
				MsgType = (uint8_t)Server->Messages[i].MsgBody[0];
				switch (MsgType) {
				case tcp_common::MARK: {
					std::cout << "Mark message received" << std::endl;
					break;
				}
				case tcp_common::CREATE_GAME: {
					std::cout << "Create game message received" << std::endl;
					break;
				}
				}
				Server->MessagesMutex.lock();
				Server->Messages.erase(Server->Messages.begin() + i);
				Server->MessagesMutex.unlock();
			}
			Sleep(10);
		}
	}
}