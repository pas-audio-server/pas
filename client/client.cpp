#include <iostream>
#include <iomanip>
#include <cctype>
#include <map>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
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
#include <json/json.h>

using namespace std;

#define	LOG(s)		(string(__FILE__) + string(":") + string(__FUNCTION__) + string("() line: ") + to_string(__LINE__) + string(" msg: ") + string(s))

const int BS = 2048;

bool keep_going = true;

map<string, string> simple_commands;
map<string, string> one_arg_commands;
map<string, string> two_arg_commands;

void SIGHandler(int signal_number)
{
	keep_going = false;
	cout << endl;
	throw string("");	
}

int InitializeNetworkConnection(int argc, char * argv[])
{
	int server_socket = -1;
	int port = 5077;
	char * ip = (char *) ("127.0.0.1");
	int opt;

	while ((opt = getopt(argc, argv, "h:p:")) != -1) 
	{
		switch (opt) 
		{
			case 'h':
				ip = optarg;
				break;

			case 'p':
				port = atoi(optarg);
				break;
		}
	}

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0)
	{
		perror("Failed to open socket");
		throw string("");
	}

	hostent * server_hostent = gethostbyname(ip);
	if (server_hostent == nullptr)
	{
		close(server_socket);
		throw  "Failed gethostbyname()";
	}

	sockaddr_in server_sockaddr;
	memset(&server_sockaddr, 0, sizeof(sockaddr_in));
	server_sockaddr.sin_family = AF_INET;
	memmove(&server_sockaddr.sin_addr.s_addr, server_hostent->h_addr, server_hostent->h_length);
	server_sockaddr.sin_port = htons(port);

	if (connect(server_socket, (struct sockaddr*) &server_sockaddr, sizeof(sockaddr_in)) == -1)
	{
		perror("Connection failed");
		throw string("");
	}

	return server_socket;
}

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

void HandleSearch(int server_socket)
{
	assert(server_socket >= 0);
	string l1;

	cout << "Column: ";
	getline(cin, l1);

	string l2;
	cout << "Pattern (no spaces): ";
	getline(cin, l2);

	string command = "se " + l1 + " " + l2;
	size_t bytes_written = send(server_socket, (const void *) command.c_str(), command.size(), 0);
	if (bytes_written != command.size())
		throw LOG("send() of search");

	cout << "Sent: " << command << endl;
	size_t result_size;
	size_t bytes_read = recv(server_socket, (void *) &result_size, sizeof(size_t), 0);
	if (bytes_read != sizeof(size_t))
		throw LOG("rcv size");
	string buffer;
	buffer.resize(result_size);
	bytes_read = recv(server_socket, (void *) &buffer[0], result_size, 0);
	if (bytes_read != result_size)
		throw LOG("rcv size");

	json_object * all = json_tokener_parse(buffer.c_str());
	if (all == nullptr)
		throw LOG("json_tokener_parse");
	json_object * rows;
	json_object_object_get_ex(all, "rows", &rows);
	if (rows == nullptr)
		throw LOG("json");
	//cout << json_object_to_json_string(rows) << endl;	
	int n = json_object_array_length(rows);

	cout << setw(48) << setfill('-') << "-" << endl;
	cout << setfill(' ');
	for (int i = 0; i < n; i++)
	{
		json_object * j = json_object_array_get_idx(rows, i);
		//cout << json_object_to_json_string(j) << endl;
		json_object_object_foreach(j, key, val)
		{
			cout << setw(18) << left << key;
			cout << json_object_get_string(val) << endl;
		}
		cout << setw(48) << setfill('-') << "-" << endl;
		cout << setfill(' ');
	}
	json_object_put(all);
}

bool HandleArgCommand(int server_socket, string command, map<string, string> & m, int argc)
{
	assert(server_socket >= 0);

	bool rv = false;
	if (m.find(command) != m.end())
	{
		string cmd = m[command];
		cmd.erase(2, 1);
		rv = true;

		string l1;
		cout << "Argument 1: ";
		getline(cin, l1);

		string l2;
		if (argc == 2)
		{
			cout << "Argument 2: ";
			getline(cin, l2);
		}

		char buffer[BS];
		memset(buffer, 0, BS);

		command = cmd + l1 + ((argc > 1) ? (string(" ") + l2) : string(""));

		send(server_socket, (const void *) command.c_str(), command.size(), 0);
	}
	return rv;
}

bool HandleSimple(int server_socket, string command)
{
	assert(server_socket >= 0);

	bool rv = false;
	if (simple_commands.find(command) != simple_commands.end())
	{
		rv = true;

		string temp;
		char buffer[BS];
		memset(buffer, 0, BS);

		//cout << LOG(temp) << endl;
		temp = command = simple_commands[command];
		if (temp.size() > 2 && temp[2] == '_') 
			temp.erase(2, 1);
			//cout << LOG(temp) << endl;

		size_t bytes_sent = send(server_socket, (const void *) temp.c_str(), temp.size(), 0);
		if (bytes_sent == temp.size())
		{
			//cout << LOG("") << endl;
			if (command.size() < 3 || command.at(2) != '_')
			{
				//cout << LOG("") << endl;
				if (command != "_sq" && bytes_sent == command.size())
				{
					//cout << LOG("") << endl;
					size_t bytes_read = recv(server_socket, (void *) buffer, BS, 0);
					if (bytes_read > 0)
					{
						string s(buffer);
						cout << s;
						if (s.at(s.size() - 1) != '\n')
							cout << endl;
					}
				}
			}
		}
	}
	return rv;
}

void OrganizeCommands()
{
	simple_commands.insert(make_pair("sq", "_sq"));
	simple_commands.insert(make_pair("tc", "tc"));
	simple_commands.insert(make_pair("ac", "ac"));

	simple_commands.insert(make_pair("next 0", "0 _next"));
	simple_commands.insert(make_pair("next 1", "1 _next"));
	simple_commands.insert(make_pair("next 2", "2 _next"));
	simple_commands.insert(make_pair("next 3", "3 _next"));

	simple_commands.insert(make_pair("clear 0", "0 _clear"));
	simple_commands.insert(make_pair("clear 1", "1 _clear"));
	simple_commands.insert(make_pair("clear 2", "2 _clear"));
	simple_commands.insert(make_pair("clear 3", "3 _clear"));

	simple_commands.insert(make_pair("ti 0", "0 ti"));
	simple_commands.insert(make_pair("ti 1", "1 ti"));
	simple_commands.insert(make_pair("ti 2", "2 ti"));
	simple_commands.insert(make_pair("ti 3", "3 ti"));

	simple_commands.insert(make_pair("s 0", "0 _S"));
	simple_commands.insert(make_pair("s 1", "1 _S"));
	simple_commands.insert(make_pair("s 2", "2 _S"));
	simple_commands.insert(make_pair("s 3", "3 _S"));

	simple_commands.insert(make_pair("z 0", "0 _Z"));
	simple_commands.insert(make_pair("z 1", "1 _Z"));
	simple_commands.insert(make_pair("z 2", "2 _Z"));
	simple_commands.insert(make_pair("z 3", "3 _Z"));

	simple_commands.insert(make_pair("r 0", "0 _R"));
	simple_commands.insert(make_pair("r 1", "1 _R"));
	simple_commands.insert(make_pair("r 2", "2 _R"));
	simple_commands.insert(make_pair("r 3", "3 _R"));

	simple_commands.insert(make_pair("what 0", "0 what"));
	simple_commands.insert(make_pair("what 1", "1 what"));
	simple_commands.insert(make_pair("what 2", "2 what"));
	simple_commands.insert(make_pair("what 3", "3 what"));

	simple_commands.insert(make_pair("who 0", "0 who"));
	simple_commands.insert(make_pair("who 1", "1 who"));
	simple_commands.insert(make_pair("who 2", "2 who"));
	simple_commands.insert(make_pair("who 3", "3 who"));

	one_arg_commands.insert(make_pair("p 0", "0 _P "));
	one_arg_commands.insert(make_pair("p 1", "1 _P "));
	one_arg_commands.insert(make_pair("p 2", "2 _P "));
	one_arg_commands.insert(make_pair("p 3", "3 _P "));
}

int main(int argc, char * argv[])
{
	int server_socket = -1;
	bool connected = false;
	char buffer[BS];
	string l;

	OrganizeCommands();
	try
	{
		if (signal(SIGINT, SIGHandler) == SIG_ERR)
			throw LOG("");

		server_socket = InitializeNetworkConnection(argc, argv);
		connected = true;
		while (keep_going)
		{
			memset(buffer, 0, BS);
			cout << "Command: ";
			getline(cin, l);

			if (l == "quit")
				break;

			if (l == "rc")
			{
				if (!connected)
				{
					server_socket = InitializeNetworkConnection(argc, argv);
					if (server_socket < 0)
						break;
					connected = true;
					cout << "Connected" << endl;
				}
				continue;
			}

			if (!connected)
				continue;

			if (HandleSimple(server_socket, l))
			{
				if (l == "sq")
				{
					cout << "Disconnected" << endl;
					connected = false;
					server_socket = -1;
				}
				continue;
			}

			if (HandleArgCommand(server_socket, l, one_arg_commands, 1))
				continue;

			if (HandleArgCommand(server_socket, l, two_arg_commands, 2))
				continue;

			if (l == "se")
				HandleSearch(server_socket);
		}
	}
	catch (string s)
	{
		if (s.size() > 0)
			cerr << s << endl;
	}

	if (server_socket >= 0)
		close(server_socket);

	return 0;
}
