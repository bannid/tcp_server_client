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
	});
	//Temp code to debug server
	while (1) {
		int userAnswer;
		std::cout << "Enter something" << std::endl;
		std::cin >> userAnswer;
		switch (userAnswer) {
		case 1: {
			Server.brodcast_message();
			break;
		}
		}
	}
	Thread.join();
	Server.stop();
}