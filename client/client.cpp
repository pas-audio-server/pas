#include <iostream>
#include <fstream>
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
#include "../protos/cpp/commands.pb.h"

using namespace std;
using namespace pas;

#define	LOG(s)		(string(__FILE__) + string(":") + string(__FUNCTION__) + string("() line: ") + to_string(__LINE__) + string(" msg: ") + string(s))

const int BS = 2048;

bool keep_going = true;

/*
   map<string, string> simple_commands;
   map<string, string> one_arg_commands;
   map<string, string> two_arg_commands;
 */

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

string GetResponse(int server_socket, Type & type)
{
	size_t length = 0;
	size_t bytes_read = recv(server_socket, (void *) &length, sizeof(length), 0);
	if (bytes_read != sizeof(length))
		throw LOG("bad recv getting length: " + to_string(bytes_read));
	cout << LOG("recv of length: " + to_string(length)) << endl;

	string s;
	s.resize(length);
	cout << LOG("string is now: " + to_string(s.size())) << endl;
	bytes_read = recv(server_socket, (void *) &s[0], length, MSG_WAITALL);
	if (bytes_read != length)
		throw LOG("bad recv getting pbuffer: " + to_string(bytes_read));
	GenericPB g;
	// Start out as generic and return this if a) it IS generic or b) does not parse out.
	type = GENERIC;
	if (g.ParseFromString(s))
		type = g.type();
	return s;
}

int GetDeviceNumber()
{
	string l;
	cout << "Device number: ";
	getline(cin, l);

	int device_id = atoi(l.c_str());
	if (device_id < 0 || device_id > 3)
		device_id = -1;
	return device_id;
}

void SendPB(string & s, int server_socket);

/*	This is broken out to make the playlist feature easier.
 */
static void InnerPlayCommand(int device_id, int track, int server_socket)
{
	assert(server_socket >= 0);

	string s;
	PlayTrackCommand c;
	c.set_type(PLAY_TRACK_DEVICE);
	c.set_device_id(device_id);
	c.set_track_id(track);
	bool outcome = c.SerializeToString(&s);
	if (!outcome)
		throw string("InnerPlayCommand() bad serialize");
	SendPB(s, server_socket);
}

static void PlayCommand(int server_socket)
{
	int device_id = GetDeviceNumber();
	if (device_id >= 0)
	{
		string s;
		cout << "Track number: ";
		getline(cin, s);
		int track = atoi(s.c_str());
		InnerPlayCommand(device_id, track, server_socket);
	}
}

static void SimplePlayList(int server_socket)
{
	int device_id = GetDeviceNumber();
	if (device_id >= 0)
	{
		string l;
		cout << "File: ";
		getline(cin, l);
		ifstream f(l);
		if (f.is_open())
		{
			while (getline(f, l))
			{
				if (l.size() == 0)
					continue;
				int track = atoi(l.c_str());
				InnerPlayCommand(device_id, track, server_socket);
				usleep(900000);
			}
		}
		f.close();
	}
}

void DacInfoCommand(int server_socket)
{
	assert(server_socket >= 0);
	string s;
	DacInfo a;
	a.set_type(DAC_INFO_COMMAND);
	bool outcome = a.SerializeToString(&s);
	if (!outcome)
		throw string("DacInfoCommand() bad serialize");
	SendPB(s, server_socket);
	Type type;
	s = GetResponse(server_socket, type);
	if (type == Type::SELECT_RESULT)
	{
		//cout << LOG("got select result back") << endl;
		SelectResult sr;
		if (!sr.ParseFromString(s))
		{
			throw string("dacinfocomment parsefromstring failed");
		}
		cout << setw(8) << left << "Index";
		cout << setw(24) << "Name";
		cout << setw(24) << "Who";
		cout << setw(24) << "What";
		cout << setw(10) << "When";
		cout << right << endl;
		for (int i = 0; i < sr.row_size(); i++)
		{
			Row r = sr.row(i);
			google::protobuf::Map<string, string> results = r.results();
			cout << setw(8) << left << results[string("index")];
			cout << setw(24) << results[string("name")];
			cout << setw(24) << results[string("who")];
			cout << setw(24) << results[string("what")];
			cout << setw(10) << results[string("when")];
			cout << right << endl;
		}
	}
	else
	{
		throw string("did not get select_result back");
	}
}

void TrackCountCommand(int server_socket)
{
	assert(server_socket >= 0);
	string s;
	TrackCountQuery a;
	a.set_type(TRACK_COUNT);
	bool outcome = a.SerializeToString(&s);
	if (!outcome)
		throw string("TrackCountQuery() bad serialize");
	SendPB(s, server_socket);
	Type type;
	s = GetResponse(server_socket, type);
	if (type == Type::ONE_INT)
	{
		OneInteger o;
		if (o.ParseFromString(s))
		{
			cout << o.value() << endl;
		}
		else
		{
			throw string("track_count parsefromstring failed");
		}
	}
	else
	{
		throw string("track count did not get a ONE_INTEGER back");
	}
}

void ArtistCountCommand(int server_socket)
{
	assert(server_socket >= 0);
	string s;
	ArtistCountQuery a;
	a.set_type(ARTIST_COUNT);
	bool outcome = a.SerializeToString(&s);
	if (!outcome)
		throw string("ArtistCountQuery() bad serialize");
	SendPB(s, server_socket);
	Type type;
	s = GetResponse(server_socket, type);
	if (type == Type::ONE_INT)
	{
		OneInteger o;
		if (o.ParseFromString(s))
		{
			cout << o.value() << endl;
		}
		else
		{
			throw string("artist_count parsefromstring failed");
		}
	}
	else
	{
		throw string("artist did not get a ONE_INTEGER back");
	}
}

void DevCmdForOneString(Type type, int server_socket)
{
	int device_id = GetDeviceNumber();
	if (device_id >= 0)
	{
		string s;
		// Using a WhatCommand container - doesn't matter.
		WhatDeviceCommand sc;
		sc.set_type(type);
		sc.set_device_id(device_id);
		bool outcome = sc.SerializeToString(&s);
		if (!outcome)
			throw string("bad serialize");
		SendPB(s, server_socket);
		Type type;
		s = GetResponse(server_socket, type);
		if (type == Type::ONE_STRING)
		{
			OneString o;
			if (o.ParseFromString(s))
			{
				cout << o.value() << endl;
			}
			else
			{
				throw string("parsefromstring failed");
			}
		}
		else
		{
			throw string("did not get a ONE_STRING back");
		}
	}
}

void SelectCommand(int server_socket)
{
	assert(server_socket >= 0);

	string s;
	SelectQuery c;
	c.set_type(SELECT_QUERY);
	cout << "Column: ";
	getline(cin, s);
	c.set_column(s);
	cout << "Pattern: ";
	getline(cin , s);
	c.set_pattern(s);
	bool outcome = c.SerializeToString(&s);
	cout << c.DebugString() << endl;
	if (!outcome)
		throw string("bad serialize");
	cout << LOG(to_string(s.size())) << endl;
	SendPB(s, server_socket);
	Type type;
	s = GetResponse(server_socket, type);
	if (type == Type::SELECT_RESULT)
	{
		SelectResult sr;
		if (!sr.ParseFromString(s))
		{
			throw string("parsefromstring failed");
		}
		cout << left << setw(8) << "id" << setw(24) << "artist" << setw(24) << "title" << setw(10) << "duration" << endl;
		for (int i = 0; i < sr.row_size(); i++)
		{
			Row r = sr.row(i);
			// r.results_size() tells how many are in map.
			google::protobuf::Map<string, string> m = r.results();
			cout << left << setw(8) << m["id"];
			cout << setw(30) << m["artist"];
			cout << setw(36) << m["title"];
			cout << setw(10) << m["duration"];
			cout << endl;
/* KEEP THIS AROUND FOR REFERENCE
			google::protobuf::Map<string, string>::iterator it = m.begin();
			while (it != m.end())
			{
				cout << it->first << "		" << it->second << endl;
				it++;
			}
*/
		}	
	}
	else
	{
		throw string("did not get back a SELECT_RESULT");
	}
}

void DevCmdNoReply(Type type, int server_socket)
{
	int device_id = GetDeviceNumber();
	if (device_id >= 0)
	{
		string s;
		ClearDeviceCommand c;
		c.set_type(type);
		c.set_device_id(device_id);
		bool outcome = c.SerializeToString(&s);
		if (!outcome)
			throw string("bad serialize");
		SendPB(s, server_socket);
	}
}

void SendPB(string & s, int server_socket)
{
	assert(server_socket >= 0);

	size_t length = s.size();
	size_t bytes_sent = send(server_socket, (const void *) &length, sizeof(length), 0);
	if (bytes_sent != sizeof(length))
		throw string("bad bytes_sent for length");

	bytes_sent = send(server_socket, (const void *) s.data(), length, 0);
	if (bytes_sent != length)
		throw string("bad bytes_sent for message");
}

int main(int argc, char * argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	int server_socket = -1;
	bool connected = false;
	string l;

	try
	{
		if (signal(SIGINT, SIGHandler) == SIG_ERR)
			throw LOG("");

		server_socket = InitializeNetworkConnection(argc, argv);
		connected = true;

		while (keep_going)
		{
			cout << "Command: ";
			getline(cin, l);

			if (l == "stop")
				DevCmdNoReply(STOP_DEVICE, server_socket);
			else if (l == "resume")
				DevCmdNoReply(RESUME_DEVICE, server_socket);
			else if (l == "pause")
				DevCmdNoReply(PAUSE_DEVICE, server_socket);
			else if (l == "play")
				PlayCommand(server_socket);
			else if (l == "playlist")
				SimplePlayList(server_socket);
			else if (l == "next")
				DevCmdNoReply(NEXT_DEVICE, server_socket);
			else if (l == "clear")
				DevCmdNoReply(CLEAR_DEVICE, server_socket);
			else if (l == "select")
				SelectCommand(server_socket);
			else if (l == "what")
				DevCmdForOneString(WHAT_DEVICE, server_socket);
			else if (l == "who")
				DevCmdForOneString(WHO_DEVICE, server_socket);
			else if (l == "tracks")
				TrackCountCommand(server_socket);
			else if (l == "artists")
				ArtistCountCommand(server_socket);
			else if (l == "quit")
				break;
			else if (l == "dacs")
				DacInfoCommand(server_socket);
			else if (l == "timecode")
				DevCmdForOneString(WHEN_DEVICE, server_socket);
			else if (l == "rc")
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
			else if (l == "qs")
			{
				if (!connected)
					continue;
				close(server_socket);
				cout << "Disconnected" << endl;
				connected = false;
				server_socket = -1;
			}
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
