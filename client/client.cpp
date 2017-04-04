#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unordered_set>
#include <time.h>
#include <assert.h>

using namespace std;

inline bool BeginsWith(string const & fullString, string const & prefix)
{
	bool rv = false;

    if (fullString.length() >= prefix.length())
	{
        rv = (0 == fullString.compare(0, prefix.length(), prefix));
    }
	return rv;
}

bool HasEnding (string const & fullString, string const & ending)
{
	bool rv = false;

    if (fullString.length() >= ending.length())
	{
        rv = (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    }
	return rv;
}

int main(int argc, char * argv[])
{
	int port = 5077;
    char * ip = (char *) ("127.0.0.1");

	int server_socket;
	hostent * server_hostent;
	sockaddr_in server_sockaddr;

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0)
	{
		perror("Client failed to open socket");
		exit(1);
	}

	server_hostent = gethostbyname(ip);
	if (server_hostent == nullptr)
	{
		cerr << "Client failed in gethostbyname()" << endl;
		close(server_socket);
		exit(1);
	}

	memset(&server_sockaddr, 0, sizeof(server_sockaddr));
	server_sockaddr.sin_family = AF_INET;
	memmove(&server_sockaddr.sin_addr.s_addr, server_hostent->h_addr, server_hostent->h_length);
	server_sockaddr.sin_port = htons(port);

	if (connect(server_socket, (struct sockaddr*)&server_sockaddr, sizeof(server_sockaddr)) == -1)
	{
		perror("Client connection failure:");
		close(server_socket);
		exit(1);
	}

	const int BS = 2048;
	char buffer[BS];
	ssize_t bytes_sent;
	ssize_t bytes_read;
	string l;
	cout << "Command: ";
	getline(cin, l);
	while (l != "quit")
	{
		memset(buffer, 0, BS);

		if (l == "se")
		{
			string column, pattern;
			cout << "Column: ";
			cin >> column;
			cout << "Pattern (no spaces): ";
			cin >> pattern;
			l = "se " + column + " " + pattern;
			bytes_sent = send(server_socket, (const void *) l.c_str(), l.size(), 0);
			if (bytes_sent != (int) l.size())
				break;

			string s;
			while (s.find("****") == string::npos)
			{
				memset(buffer, 0, BS);
				bytes_read = recv(server_socket, (void *) buffer, BS, 0);
				s = string(buffer);
				cout << s;
			}
			cout << endl;
			cin.ignore(9999, '\n');
		}
		if (l == "ac")
		{
			bytes_sent = send(server_socket, (const void *) l.c_str(), l.size(), 0);
			if (bytes_sent != (int) l.size())
				break;
			bytes_read = recv(server_socket, (void *) buffer, BS, 0);
			if (bytes_read > 0)
			{
				string s(buffer);
				cout << "Artist count: " << s;
			}
		}
		if (l == "tc")
		{
			bytes_sent = send(server_socket, (const void *) l.c_str(), l.size(), 0);
			if (bytes_sent != (int) l.size())
				break;
			bytes_read = recv(server_socket, (void *) buffer, BS, 0);
			if (bytes_read > 0)
			{
				string s(buffer);
				cout << "Track count: " << s;
			}
		}
		cout << "Command: ";
		getline(cin, l);
	}
	close(server_socket);
	return 0;
}
