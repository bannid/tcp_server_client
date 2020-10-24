#pragma once
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include <tcp_common.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
namespace net_client {
	enum errors {

	};
	class client {
	public:
		bool initialize();
		bool connect_to_server(const char* IpAddress,const char* PortNumber);
		bool send_message(std::vector<char> Message);
		bool listen_on_port();
		void stop();
	private:
		WSADATA wsaData;
		SOCKET ConnectSocket = INVALID_SOCKET;
		struct addrinfo *result = NULL,
			hints;
	private:
		errors Error;
		int ErrorCode;
	};
	bool client::initialize() {
		// Initialize Winsock
		ErrorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (ErrorCode != 0) {
			printf("WSAStartup failed with error: %d\n", ErrorCode);
			return false;
		}

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
	}
	bool client::connect_to_server(const char* IpAddress, const char* PortNumber) {
		// Resolve the server address and port
		ErrorCode = getaddrinfo("127.0.0.1", "27015", &hints, &result);
		if (ErrorCode != 0) {
			printf("getaddrinfo failed with error: %d\n", ErrorCode);
			return false;
		}
		// Create a SOCKET for connecting to server
		ConnectSocket = socket(result->ai_family, result->ai_socktype,
			result->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			ErrorCode = WSAGetLastError();
			printf("socket failed with error: %ld\n", WSAGetLastError());
			return false;
		}
		// Connect to server.
		ErrorCode = connect(ConnectSocket, result->ai_addr, (int)result->ai_addrlen);
		if (ErrorCode == SOCKET_ERROR) {
			freeaddrinfo(result);
			return false;
		}
		
		freeaddrinfo(result);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("Unable to connect to server!\n");
			return false;
		}
		return true;
	}
	bool client::send_message(std::vector<char> Message) {
		// Send an initial buffer
		int ErrorCode = send(ConnectSocket, (const char*)Message.data(), Message.size(), 0);
		if (ErrorCode == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			return false;
		}
		return true;
	}
	bool client::listen_on_port() {
		char recvbuf[512];
		int recvbuflen = 512;
		int ErrorCode;
		// Receive until the peer closes the connection
		do {

			ErrorCode = recv(ConnectSocket, recvbuf, recvbuflen, 0);
			if (ErrorCode > 0) {
				std::string Message;
				printf("Bytes received: %d\n", ErrorCode);
				for (int i = 0; i < ErrorCode; i++) {
					Message.push_back(recvbuf[i]);
				}
				std::cout << Message.c_str() << std::endl;
			}
			else if (ErrorCode == 0) {
				printf("Connection closed\n");
			}
			else {
				printf("recv failed with error: %d\n", WSAGetLastError());
				return false;
			}

		} while (ErrorCode > 0);
		return true;
	}
	void client::stop() {
		shutdown(ConnectSocket, SD_SEND);
		closesocket(ConnectSocket);
		WSACleanup();
	}
}