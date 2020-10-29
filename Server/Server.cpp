#include <thread>
#include "tcp_server.h"
// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

int main(void) {
	net_server::server Server;
	std::thread Thread([&] {
		bool success = Server.start("27015");
		if (success) {
			Server.listen_on_port();
		}
		else {
			net_server::errors Error = Server.get_last_error();
			switch (Error) {
			case net_server::errors::WSA_STARTUP_FAILED: {
				std::cout << "WSA_STARTUP_FAILED with error code " << Server.Error << std::endl;
			}
			}
		}
		std::cout << "Exiting thread..";
	});
	//Temp code to debug server
	while (Server.Running.load()) {
		int UserAnswer;
		const char* Menu = "Enter one of the following options:\n"
			"1. Send number of clients to all clients\n"
			"2. Total number of clients\n"
			"3. Exit the server.";
		std::cout << Menu << std::endl;
		std::cin >> UserAnswer;
		switch (UserAnswer) {
		case 1: {
			Server.brodcast_message();
			break;
		}
		case 2: {
			std::cout << "Total number of clients is: " << Server.get_number_of_clients() << std::endl;
			break;
		}
		case 3: {
			std::cout << "Stopping the server.." << std::endl;
			Server.stop();
			break;
		}
		}
	}
	if (Thread.joinable()) {
		Thread.join();
	}
}