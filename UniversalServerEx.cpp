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
	
	char sz[] = "Hardware concurrency=";
	cout << sz << std::thread::hardware_concurrency() << endl;


	Server server;
	server.CreateSubServer(9988, nullptr);

	server.Start();

	return 0;
}