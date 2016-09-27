

#include <iostream>
#include "Server.h"
using namespace std;

class MyClientContext : public ClientContext
{

};

class MySubServer : public SubServer
{

protected:
	virtual ClientContext* onCreateNewClientContext() override
	{
		return new MyClientContext();
	}

	virtual size_t isClientDataReadable(ClientContext *clientContext)
	{
		//TODO: return the size of a single package
		auto &buf = clientContext->GetReceiveBuffer();
		return buf.GetDataLength();
	}

	virtual void onClientReceived(ClientContext *clientContext, char *buffer, size_t len) override
	{
		auto tid = std::this_thread::get_id();
		std::cout << len << " bytes of data received in MySubServer! thread=" << tid << ", IP=" << clientContext->GetPeerIP() << std::endl;

		SendDataToClient(clientContext, buffer, len);
	}
};


int main(int argc, char *argv[])
{
	
	char sz[] = "Hardware concurrency=";
	cout << sz << std::thread::hardware_concurrency() << endl;


	Server server;
	server.CreateSubServer<MySubServer>(9988, nullptr);

	server.Start();

	return 0;
}