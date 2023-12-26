#include <iostream>
#include <cstring>
#include <string>
#include <cstdlib>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <map>

const int MAX_EVENTS = 1024;

struct ClientInfo {
    int clientSocket;
    std::string name;
};

int createServerSocket(int port) {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(port);

    int bind_result = bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    if (bind_result < 0) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    int listen_result = listen(serverSocket, SOMAXCONN);
    if (listen_result < 0) {
        perror("Error listening on socket");
        exit(EXIT_FAILURE);
    }

    return serverSocket;
}

int createEpollInstance() {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("Error creating epoll instance");
        exit(EXIT_FAILURE);
    }

    return epoll_fd;
}

void addSocketToEpoll(int epoll_fd, int socket_fd) {
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = socket_fd;

    int epoll_ctl_result = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event);
    if (epoll_ctl_result < 0) {
        perror("Error adding socket to epoll");
        exit(EXIT_FAILURE);
    }
}

int acceptClientConnection(int serverSocket) {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (clientSocket < 0) {
        perror("Error accepting connection");
        exit(EXIT_FAILURE);
    }

    return clientSocket;
}

int main() {
    int serverSocket = createServerSocket(9090);
    int epoll_fd = createEpollInstance();

    addSocketToEpoll(epoll_fd, serverSocket);

    std::map<int, ClientInfo> clients;

    while (true) {
        struct epoll_event events[MAX_EVENTS];
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events < 0) {
            perror("Error in epoll_wait");
            break;
        }

        for (int i = 0; i < num_events; ++i) {
            int current_fd = events[i].data.fd;

            if (current_fd == serverSocket) {
                int clientSocket = acceptClientConnection(serverSocket);
                addSocketToEpoll(epoll_fd, clientSocket);

                ClientInfo client;
                client.clientSocket = clientSocket;
                client.name = "";
                clients[clientSocket] = client;
            }
            else {
                char buffer[1024];
                int bytes_received = read(current_fd, buffer, sizeof(buffer));
                if (bytes_received < 0) {
                    perror("Error reading from client");
                    close(current_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, 0);
                    clients.erase(current_fd);
                }
                else if (bytes_received == 0) {
                    close(current_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, 0);
                    clients.erase(current_fd);
                }
                else {
                    std::string msg(buffer, bytes_received);
                    if (clients[current_fd].name.empty()) {
                        clients[current_fd].name = msg;
                    }
                    else {
                        std::string name = clients[current_fd].name;
                        for (auto& client : clients) {
                            if (client.first != current_fd) {
                                std::string message = "[" + name + "]: " + msg;
                                write(client.first, message.c_str(), message.size());
                            }
                        }
                    }
                }
            }
        }
    }

    close(epoll_fd);
    close(serverSocket);

    return 0;
}
