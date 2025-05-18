#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "parser/parser.h"

using namespace std;

constexpr int BUFFER_SIZE = 512;

unordered_map<string, vector<uint8_t>> kv_store;
mutex kv_mutex;

void send_response(int client_socket, const string& response) {
    write(client_socket, response.c_str(), response.size());
}

void send_raw(int client_socket, const uint8_t* data, size_t size) {
    write(client_socket, data, size);
    write(client_socket, "\r\n", 2);
}

void handle_command(const vector<vector<uint8_t>>& args, int client_socket) {
    if (args.empty()) {
        send_response(client_socket, "-ERR empty command\r\n");
        return;
    }

    string command(args[0].begin(), args[0].end());
    transform(command.begin(), command.end(), command.begin(), ::toupper);

    if (command == "PING") {
        send_response(client_socket, "+PONG\r\n");
    } else if (command == "ECHO") {
        if (args.size() > 1) {
            string response(args[1].begin(), args[1].end());
            send_response(client_socket, "+" + response + "\r\n");
        } else {
            send_response(client_socket, "-ERR ECHO requires argument\r\n");
        }
    } else if (command == "SET") {
        if (args.size() <= 2) {
            send_response(client_socket, "-ERR SET requires key and value\r\n");
            return;
        }

        string key(args[1].begin(), args[1].end());
        vector<uint8_t> value(args[2].begin(), args[2].end());

        {
            lock_guard<mutex> lock(kv_mutex);
            kv_store[key] = std::move(value);
        }

        send_response(client_socket, "+OK\r\n");
    } else if (command == "GET") {
        if (args.size() <= 1) {
            send_response(client_socket, "-ERR GET requires key\r\n");
            return;
        }

        string key(args[1].begin(), args[1].end());
        vector<uint8_t> value;

        {
            lock_guard<mutex> lock(kv_mutex);
            auto it = kv_store.find(key);
            if (it != kv_store.end()) {
                value = it->second;
            } else {
                send_response(client_socket, "$-1\r\n");
                return;
            }
        }

        send_response(client_socket, "$" + to_string(value.size()) + "\r\n");
        send_raw(client_socket, value.data(), value.size());
    } else {
        send_response(client_socket, "-ERR unknown command\r\n");
    }
}

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
            if (!result)
                break;

            auto [args, parsed_len] = *result;
            start += parsed_len;

            handle_command(args, client_socket);
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
        return EXIT_FAILURE;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(6379);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        return EXIT_FAILURE;
    }

    cout << "listening on port 6379" << endl;

    while (true) {
        socklen_t addrlen = sizeof(address);
        int client_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (client_socket < 0) {
            perror("accept failed");
            continue;
        }

        cout << "accepted new connection" << endl;
        thread(handle_connection, client_socket).detach();
    }
}
