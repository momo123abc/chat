#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <stdio.h>
#pragma comment(lib,"Ws2_32.lib")

#define BUF_SIZE 1024

char username[BUF_SIZE];
char messageBuffer[BUF_SIZE];
SOCKET hSocket;

void InitializeSocket() {
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

    wVersionRequested = MAKEWORD(2, 2);

    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        exit(-1);
    }

    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        WSACleanup();
        exit(-1);
    }

    hSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (hSocket == INVALID_SOCKET) {
        printf("套接字创建失败\n");
        WSACleanup();
        exit(-1);
    }
}

void ConnectToServer() {
    SOCKADDR_IN serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(9090);
    inet_pton(AF_INET, "172.26.145.237", &serverAddress.sin_addr);

    if (connect(hSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        printf("连接错误 : %d\n", WSAGetLastError());
        closesocket(hSocket);
        WSACleanup();
        exit(-1);
    }
}

void GetUsernameAndSend() {
    printf("epoll实现聊天室，请输入你的用户名：\n");
    fgets(username, BUF_SIZE, stdin);
    username[strlen(username) - 1] = '\0';
    send(hSocket, username, strlen(username), 0);
}

DWORD WINAPI SendMessageThread(void* arg) {
    while (1) {
        fgets(messageBuffer, BUF_SIZE, stdin);

        if (strcmp(messageBuffer, "quit\n") == 0 || strcmp(messageBuffer, "QUIT\n") == 0) {
            closesocket(hSocket);
            exit(0);
        }

        send(hSocket, messageBuffer, strlen(messageBuffer), 0);
    }

    return 0;
}

DWORD WINAPI ReceiveMessageThread(void* arg) {
    while (1) {
        int receivedLen = recv(hSocket, messageBuffer, sizeof(messageBuffer) - 1, 0);

        if (receivedLen <= 0) {
            closesocket(hSocket);
            exit(0);
        }

        messageBuffer[receivedLen] = '\0';
        if (messageBuffer[0] == '[') {
            printf("%s", messageBuffer);
        }
        else {
            printf("[%s]: %s\n", username, messageBuffer);
        }
    }

    return 0;
}

void StartChat() {
    HANDLE sendThread = CreateThread(NULL, 0, SendMessageThread, NULL, 0, NULL);
    HANDLE receiveThread = CreateThread(NULL, 0, ReceiveMessageThread, NULL, 0, NULL);

    WaitForSingleObject(sendThread, INFINITE);
    WaitForSingleObject(receiveThread, INFINITE);
}

int main() {
    InitializeSocket();
    ConnectToServer();
    GetUsernameAndSend();
    StartChat();

    closesocket(hSocket);
    WSACleanup();

    return 0;
}
