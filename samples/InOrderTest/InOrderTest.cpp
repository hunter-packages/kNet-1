/* Copyright The kNet Project.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

/** @file InOrderTest.cpp
	@brief Tests that message ordering works properly. */

#include <iostream>
#include <string>
#include <cmath>

#include "kNet.h"
#include "kNet\UDPMessageConnection.h"

using namespace std;
using namespace kNet;

class NetworkApp : public IMessageHandler, public INetworkServerListener
{
	Network network;
	NetworkServer *server;

	/// For client, stores the most recently sent message number.
	/// For server, stores the most recently received message number.
	u32 lastMessageNumber;
public:
	NetworkApp()
	{
		lastMessageNumber = 0;
	}

	void SendMessage(MessageConnection *connection)
	{
		NetworkMessage *msg = connection->StartNewMessage(191 /*A custom message number*/, 4 /*size of this message in bytes*/);
		msg->priority = 100;
		msg->reliable = true;
		msg->inOrder = true;
		msg->contentID = 1;
		DataSerializer ds(msg->data, msg->Size());
		++lastMessageNumber;
		ds.Add<u32>(lastMessageNumber);
		connection->EndAndQueueMessage(msg);
	}

	/// Called to notify the listener that a new connection has been established.
	void NewConnectionEstablished(MessageConnection *connection)
	{
		connection->RegisterInboundMessageHandler(this);
	}

	u32 ComputeContentID(message_id_t messageId, const char *data, size_t numBytes)
	{
		if (messageId == 191) // The magic number for our custom message type.
			return 1;
		else
			return 0;
	}

	void HandleMessage(MessageConnection *source, packet_id_t packetId, message_id_t messageId, const char *data, size_t numBytes)
	{
		if (messageId != 191) // The magic number for our custom message type.
			return;
		DataDeserializer dd(data, numBytes);
		u32 recvMessageNumber = dd.Read<u32>();
		if (recvMessageNumber <= lastMessageNumber)
			cout << "Message received out-of-order! Got " << recvMessageNumber << ", previously received was " << lastMessageNumber << endl;
		else
			cout << "Prev " << lastMessageNumber << ", now " << recvMessageNumber << endl;
		lastMessageNumber = recvMessageNumber;
	}

	void RunServer(unsigned short port, SocketTransportLayer transport)
	{
		// Start the server either in TCP or UDP mode.
		server = network.StartServer(port, transport, this, true);
		if (!server)
		{
			cout << "Unable to start server in port " << port << "!" << endl;
			return;
		}

		cout << "Server waiting for connection in port " << port << "." << endl;

		server->RunModalServer();
	}

	void RunClient(const char *address, unsigned short port, SocketTransportLayer transport)
	{
		Ptr(MessageConnection) connection = network.Connect(address, port, transport, this);
		if (!connection)
		{
			cout << "Unable to connect to " << address << ":" << port << "." << endl;
			return;
		}

		cout << "Waiting for connection.." << endl;
		while(connection->GetConnectionState() == ConnectionPending)
			Clock::Sleep(100);

		if (connection->GetConnectionState() != ConnectionOK)
		{
			cout << "Failed to connect to server!" << endl;
			return;
		}

		cout << "Connected to " << connection->ToString() << "." << endl;

		const int numMessagesToSend = 100000;

		connection->NetworkSendSimulator().enabled = true;
		connection->NetworkSendSimulator().constantPacketSendDelay = 50.f;
		connection->NetworkSendSimulator().packetLossRate = 0.1f;
		connection->NetworkSendSimulator().uniformRandomPacketSendDelay = 100.f;

		for(int i = 0; i < numMessagesToSend; ++i)
		{
			connection->Process();
			if (connection->NumOutboundMessagesPending() < 1000)
				SendMessage(connection);
			Clock::Sleep(1);
		}

		connection->Disconnect();
	}
};

void PrintUsage()
{
	cout << "Usage: " << endl;
	cout << "       server tcp|udp port" << endl;
	cout << "       client tcp|udp hostname port" << endl;
}

int main(int argc, char **argv)
{
	if (argc < 4)
	{
		PrintUsage();
		return 0;
	}

	EnableMemoryLeakLoggingAtExit();

	SocketTransportLayer transport = StringToSocketTransportLayer(argv[2]);
	if (transport == InvalidTransportLayer)
	{
		cout << "The second parameter is either 'tcp' or 'udp'!" << endl;
		return 0;
	}
	NetworkApp app;
	if (!_stricmp(argv[1], "server"))
	{
		unsigned short port = atoi(argv[3]);

		app.RunServer(port, transport);
	}
	else if (!_stricmp(argv[1], "client"))
	{
		if (argc < 5)
		{
			PrintUsage();
			return 0;
		}

		const char *hostname = argv[3];
		unsigned short port = atoi(argv[4]);

		app.RunClient(hostname, port, transport);
	}
	else
		cout << "The second parameter is either 'server' or 'client'!" << endl;

	return 0;
}
