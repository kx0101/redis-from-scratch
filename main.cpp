#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "parser/parser.h"

using namespace std;

constexpr int BUFFER_SIZE = 512;

void handle_connection(int client_socket) {
    vector<uint8_t> buffer(BUFFER_SIZE);
    size_t buf_len = 0;

    while (true) {
        ssize_t read_count = read(client_socket, buffer.data() + buf_len, buffer.size() - buf_len);
        if (read_count <= 0) {
            cout << "client disconnected" << endl;
            break;
        }

        buf_len += read_count;
        size_t start = 0;

        while (true) {
            auto result = parse_resp_command({buffer.begin() + start, buffer.begin() + buf_len}, buf_len - start);

            if (!result) {
                break;
            }

            auto [args, parsed_len] = *result;
            start += parsed_len;

            if (args.empty()) {
                cout << "empty command" << endl;
                continue;
            }

            string command(args[0].begin(), args[0].end());
            transform(command.begin(), command.end(), command.begin(), ::toupper);

            cout << "command: " << command << endl;

            if (command == "PING") {
                const char* pong = "+PONG\r\n";
                write(client_socket, pong, strlen(pong));
            } else if (command == "ECHO") {
                if (args.size() > 1) {
                    string response(args[1].begin(), args[1].end());
                    response += "\r\n";

                    write(client_socket, response.c_str(), response.size());
                } else {
                    string err = "-ERR ECHO requires argument\r\n";
                    write(client_socket, err.c_str(), err.size());
                }
            } else {
                string err = "-ERR unknown command\r\n";
                write(client_socket, err.c_str(), err.size());
            }
        }

        if (start < buf_len) {
            memmove(buffer.data(), buffer.data() + start, buf_len - start);
            buf_len -= start;
        } else {
            buf_len = 0;
        }
    }

    shutdown(client_socket, SHUT_RDWR);
    close(client_socket);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(6379);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    cout << "listening on port 6379" << endl;

    while (true) {
        socklen_t addrlen = sizeof(address);
        int new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (new_socket < 0) {
            perror("accept failed");
            continue;
        }

        cout << "accepted new connection" << endl;
        thread(handle_connection, new_socket).detach();
    }

    return 0;
}
