#include "tcp_client.h"

int main(int argc, char **argv)
{
	net_client::client Client;
	Client.on(tcp_common::message_type::GAME_CREATED, [&](void) {
		std::cout << "Game has been create!!! Yoohay";
	});
	Client.initialize();
	Client.connect_to_server("127.0.0.1", "27015");
	int x = 42234;
	int y = 454345;
	for (int i = 0; i < 1; i++) {
		std::vector<int> data;
		data.push_back(x);
		data.push_back(y);
		Client.send_message(tcp_common::create_message(tcp_common::message_type::CREATE_GAME, data));
	}
	Client.listen_on_port();
	Client.stop();
	system("pause");
	return 0;
}