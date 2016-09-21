#include <iostream>

//////////////////////////////////////////////////////////////////////////
//Test

#include <event2/event.h>
#include <event2/listener.h>
#include <string.h>			//memset
#include "Server.h"

//////////////////////////////////////////////////////////////////////////


using namespace std;

int main(int argc, char *argv[])
{
	char sz[] = "Hello, World!";	//Hover mouse over "sz" while debugging to see its contents
	cout << sz << endl;	//<================= Put a breakpoint here

	/*
	auto base = event_base_new();

	auto support = event_get_supported_methods();

	cout << event_base_get_method(base) << endl;


	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_port = htons(9988);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htons(0);

	auto listener = evconnlistener_new_bind(base, [](auto listener, auto fd, auto addr_in, auto addr_len, auto ptr) {

		cout << "new connection accepted!" << endl;

	}, 0, 0, -1, (sockaddr*)&addr, sizeof(addr));



	//timeval ten_sec;
	//ten_sec.tv_sec = 3;
	//ten_sec.tv_usec = 0;

	//auto evt2 = event_new(base, -1, EV_PERSIST | EV_READ, [](auto sock, auto ev, auto arg) {
	//	cout << "Event!!!!2222222222222" << endl;
	//}, 0);

	//auto evt1 = event_new(base, -1, EV_PERSIST | EV_READ, [](auto sock, auto ev, auto arg) {
	//	cout << "Event!!!!" << endl;
	//	event_active((event*)arg, EV_PERSIST, 0);
	//}, evt2);

	//struct timeval five_seconds = { 5,0 };
	//int n = event_add(evt1, nullptr);
	//n = event_add(evt2, &five_seconds);
	//n = event_assign(evt1, base, -1, 0, nullptr, 0);

	while (1) {
		//event_base_loopexit(base, &ten_sec);

		//event_active(evt1, EV_READ, 0);
		

		event_base_dispatch(base);
		puts("Tick!");
	}



	//int i = 0;
	//while (support[i]) {
	//	cout << support[i] << endl;
	//	i++;
	//}

	event_base_free(base);

	*/

	Server server;
	server.CreateSubServer(9988, nullptr);

	server.Start();

	return 0;
}