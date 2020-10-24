#include <tcp_common.h>
#include "tcp_client.h"

int main(int argc, char **argv)
{
	net_client::client Client;
	Client.initialize();
	Client.connect_to_server("127.0.0.1", "27015");
	int x = 4;
	int y = 4;
	std::vector<int> data;
	data.push_back(x);
	data.push_back(y);
	Client.send_message(tcp_common::create_message(tcp_common::message_type::CREATE_GAME,data));
	Client.listen_on_port();
	Client.stop();
	system("pause");
	return 0;
}