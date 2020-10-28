#pragma once
#define WIN32_LEAN_AND_MEAN

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <tcp_common.h>

namespace net_client {
	typedef std::function<void(std::vector<int>) > event_callback;
	class client;
	static void process_messages(client * Client);
	class client {
	public:
		bool initialize();
		bool connect_to_server(const char* IpAddress, const char* PortNumber);
		bool send_message(std::vector<unsigned char > Message);
		bool listen_on_port();
		void stop();
		void on(tcp_common::message_type Type, event_callback Callback);
		void push_message(tcp_common::msg Message);
		tcp_common::msg pop_front_message();
	private:
		WSADATA wsaData;
		SOCKET ConnectSocket = INVALID_SOCKET;
		struct addrinfo *result = NULL,
			hints;
	public:
		std::atomic<bool> Running = true;
	public:
		std::map <tcp_common::message_type, std::vector<event_callback>> MessageEvents;
	public:
		HANDLE MessageSemaphore;
		std::mutex MessagesMutex;
		std::thread MessageProcessingThread;
	public:
		std::deque<tcp_common::msg> Messages;
		std::vector<unsigned char> ByteStream;
	private:
		int ErrorCode;
	};
	bool client::initialize() {
		this->MessageSemaphore = CreateSemaphoreExA(
			NULL,
			0,
			1000,
			NULL,
			NULL,
			SEMAPHORE_ALL_ACCESS
		);
		this->MessageProcessingThread = std::thread(process_messages, this);
		// Initialize Winsock
		ErrorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (ErrorCode != 0) {
			std::cout << "WSAStartup failed with error: " << ErrorCode << std::endl;
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
			std::cout << "getaddrinfo failed with error: " << ErrorCode << std::endl;
			return false;
		}
		// Create a SOCKET for connecting to server
		ConnectSocket = socket(result->ai_family, result->ai_socktype,
			result->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			ErrorCode = WSAGetLastError();
			std::cout << "socket failed with error: " << WSAGetLastError() << std::endl;
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
			std::cout << "Unable to connect to server!" << std::endl;
			return false;
		}
		return true;
	}
	void client::push_message(tcp_common::msg Message) {
		std::lock_guard<std::mutex> LocalGuard(this->MessagesMutex);
		this->Messages.push_back(Message);
	}
	tcp_common::msg client::pop_front_message() {
		std::lock_guard<std::mutex> LocalGuard(this->MessagesMutex);
		tcp_common::msg Message = this->Messages[0];
		this->Messages.pop_front();
		return Message;
	}
	bool client::send_message(std::vector<unsigned char> Message) {
		unsigned char * Ptr = (unsigned char*)Message.data();
		uint32_t Size = Message.size();
		uint32_t BytesSent = 0;
		while (BytesSent < Size) {
			// Send an initial buffer
			int ErrorCode = send(ConnectSocket, (char*)Ptr, Size - BytesSent, 0);
			if (ErrorCode == SOCKET_ERROR) {
				std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
				return false;
			}
			BytesSent += ErrorCode;
			Ptr += ErrorCode;
		}
		std::cout << "Bytes sent: " << BytesSent << std::endl;
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
				std::cout << "Bytes received: " << ErrorCode << std::endl;
				for (int i = 0; i < ErrorCode; i++) {
					this->ByteStream.push_back(recvbuf[i]);
				}
			}
			else if (ErrorCode == 0) {
				std::cout << "Connection closed\n" << std::endl;
			}
			else {
				std::cout << "recv failed with error: " << WSAGetLastError() << std::endl;
				return false;
			}
			while (this->ByteStream.size() >= tcp_common::SIZE_OF_MESSAGE) {
				int HeaderSize = sizeof(tcp_common::message_type);
				tcp_common::msg Message;
				Message.Header = (tcp_common::message_type)this->ByteStream[0];
				for (int k = HeaderSize; k < tcp_common::SIZE_OF_MESSAGE; k++) {
					Message.MsgBody.push_back(this->ByteStream[k]);
				}
				this->push_message(Message);
				ReleaseSemaphore(this->MessageSemaphore, 1, 0);
				auto Begin = this->ByteStream.begin();
				this->ByteStream.erase(Begin, Begin + tcp_common::SIZE_OF_MESSAGE);
			}


		} while (ErrorCode > 0 && this->Running.load());
		return true;
	}
	void client::stop() {
		this->Running = false;
		//Release the semaphore so the waiting threads can exit.
		ReleaseSemaphore(this->MessageSemaphore, 1, 0);
		this->MessageProcessingThread.join();
		shutdown(ConnectSocket, SD_SEND);
		closesocket(ConnectSocket);
		WSACleanup();
	}
	void client::on(tcp_common::message_type MessageType, event_callback Callback) {
		bool found = false;
		for (auto CallbackIt = this->MessageEvents.begin(); CallbackIt != this->MessageEvents.end(); CallbackIt++) {
			if (CallbackIt->first == MessageType) {
				CallbackIt->second.push_back(Callback);
				found = true;
				break;
			}
		}
		if (!found) {
			std::vector<event_callback> temp;
			temp.push_back(Callback);
			this->MessageEvents.insert({ MessageType,temp });
		}
	}
	static void process_messages(client * Client) {
		while (Client->Running.load()) {
		    DWORD Status = WaitForSingleObjectEx(Client->MessageSemaphore, INFINITE, NULL);
			tcp_common::msg Message;
			if (Client->Messages.size() > 0) {
				Message = Client->pop_front_message();
				tcp_common::message_type Header = Message.Header;
				std::vector<int> Data = tcp_common::decode_data(Header, Message.MsgBody);
				for (auto CallbackIt = Client->MessageEvents.begin(); CallbackIt != Client->MessageEvents.end(); CallbackIt++) {
					if (CallbackIt->first == Header) {
						for (auto CallbacksIt = CallbackIt->second.begin(); CallbacksIt != CallbackIt->second.end(); CallbacksIt++) {
							(*CallbacksIt)(Data);
						}
					}
				}
			}
		}
	}
}