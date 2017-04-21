#include "connection_manager.hpp"
#include "network_component.hpp"
#include "audio_component.hpp"
#include "logger.hpp"

/*	This file is part of pas.

    pas is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    pas is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with pas.  If not, see <http://www.gnu.org/licenses/>.
*/

/*	pas is Copyright 2017 by Perry Kivolowitz.
*/

using namespace std;
using namespace pas;

extern Logger _log_;

bool NetworkComponent::keep_going = true;

NetworkComponent::NetworkComponent()
{
	listening_socket = -1;
	port = 5077;
}

void NetworkComponent::SIGINTHandler(int)
{
    keep_going = false;
}

/*
   AcceptConnections() is called after all other parts of the server are initialized.
   It listens for connection requests from clients. When one is received, it spins
   off a thread to service the connection.

   Servicing is performed elsewhere.

   This function should not return. Doing so signifies an error.
*/

void NetworkComponent::AcceptConnections(void * dacs, int ndacs)
{
	// By default, Linux apparently retries  interrupted system calls.  This defeats the
	// purpose of interrupting them, doesn't it? The call to siginterrupt disables this.
	signal(SIGINT, SIGINTHandler);
	siginterrupt(SIGINT, 1);
	int incoming_socket = -1;

	try
	{
		listening_socket = socket(AF_INET, SOCK_STREAM, 0);
		if (listening_socket < 0)
		{
			// TODO - fod perror into log
			perror("Opening socket failed");
			throw LOG2(_log_, "opening listening socket failed", LogLevel::FATAL);
		}

		int optval = 1;
		optval = setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
		if (optval < 0)
		{
			// TODO - fod perror into log
			perror("sockopt failed");
			throw LOG2(_log_, "setting socket opt failed", LogLevel::FATAL);
		}

		sockaddr_in listening_sockaddr;

		memset(&listening_sockaddr, 0, sizeof(sockaddr_in));
		listening_sockaddr.sin_family = AF_INET;
		listening_sockaddr.sin_port = htons(port);
		listening_sockaddr.sin_addr.s_addr = INADDR_ANY;

		if (bind(listening_socket, (sockaddr *) &listening_sockaddr, sizeof(sockaddr_in)) < 0)
		{
			// TODO - fod perror into log
			perror("Bind failed");
			throw LOG2(_log_, "bind failed", LogLevel::FATAL);
		}

		if (listen(listening_socket , max_connections) != 0)
		{
			// TODO - fod perror into log
			perror("Error attempting to listen to socket:");
			throw LOG2(_log_, "listen failed", LogLevel::FATAL);
		}

		sockaddr_in client_info;
		memset(&client_info, 0, sizeof(sockaddr_in));
		int c = sizeof(sockaddr_in);

		int connection_counter = 0;

		while ((incoming_socket = accept(listening_socket, (sockaddr *) &client_info, (socklen_t *) &c)) > 0)
		{
			thread * t = new thread(ConnectionHandler, &client_info, incoming_socket, dacs, ndacs, connection_counter++);
			threads.push_back(t);
			if (!keep_going)
				break;
		}
		// We ought never get here. This last throw ensures we get to the cleanup taking place in
		// the catch.
		throw LOG(_log_, nullptr);
	}
	catch (LoggedException e)
	{
		AudioCommand cmd;
		cmd.cmd = QUIT;
		for (int i = 0; i < ndacs; i++)
		{
			AudioComponent * ac = (AudioComponent *) *(((AudioComponent **) dacs) + i);
			if (ac == nullptr)
				continue;
			ac->AddCommand(cmd);
			pthread_yield();
			pthread_yield();
			usleep(100000);
		}
		usleep(100000);
		pthread_yield();
		pthread_yield();

		if (incoming_socket >= 0)
			close(incoming_socket);

		if (listening_socket >= 0)
			close(listening_socket);

		while (threads.size() > 0)
		{
			threads.at(0)->join();
			threads.erase(threads.begin());
			LOG(_log_, nullptr);
		}

		LOG2(_log_, "Server shutting down", LogLevel::MINIMAL);
	}
}

