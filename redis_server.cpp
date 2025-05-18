#include "redis_server.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#include "parser/parser.h"

using namespace std;

RedisServer::RedisServer(int port) : port_(port), server_fd_(-1) {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address_ = {};
    address_.sin_family = AF_INET;
    address_.sin_addr.s_addr = INADDR_ANY;
    address_.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr*)&address_, sizeof(address_)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd_, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
}

void RedisServer::run() {
    cout << "listening on port " << port_ << endl;

    while (true) {
        socklen_t addrlen = sizeof(address_);
        int client_socket = accept(server_fd_, (struct sockaddr*)&address_, &addrlen);
        if (client_socket < 0) {
            perror("accept failed");
            continue;
        }

        cout << "accepted new connection" << endl;
        thread(&RedisServer::handle_client, this, client_socket).detach();
    }
}

void RedisServer::handle_client(int client_socket) {
    constexpr int BUFFER_SIZE = 512;
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

void RedisServer::handle_command(const vector<vector<uint8_t>>& args, int client_socket) {
    if (args.empty()) {
        send_response(client_socket, "-ERR empty command\r\n");
        return;
    }

    string command(args[0].begin(), args[0].end());
    transform(command.begin(), command.end(), command.begin(), ::toupper);

    if (command == "PING") {
        send_response(client_socket, "+PONG\r\n");
        return;
    }

    if (command == "ECHO") {
        if (args.size() > 1) {
            string response(args[1].begin(), args[1].end());
            send_response(client_socket, "+" + response + "\r\n");
            return;
        }

        send_response(client_socket, "-ERR ECHO requires argument\r\n");
        return;
    }

    if (command == "SET") {
        if (args.size() <= 2) {
            send_response(client_socket, "-ERR SET requires key and value\r\n");
            return;
        }

        string key(args[1].begin(), args[1].end());
        vector<uint8_t> value(args[2].begin(), args[2].end());

        {
            lock_guard<mutex> lock(kv_mutex_);
            kv_store_[key] = std::move(value);
        }

        send_response(client_socket, "+OK\r\n");

        return;
    }

    if (command == "GET") {
        if (args.size() <= 1) {
            send_response(client_socket, "-ERR GET requires key\r\n");
            return;
        }

        string key(args[1].begin(), args[1].end());
        vector<uint8_t> value;

        {
            lock_guard<mutex> lock(kv_mutex_);
            auto it = kv_store_.find(key);
            if (it != kv_store_.end()) {
                value = it->second;
            } else {
                send_response(client_socket, "$-1\r\n");
                return;
            }
        }

        send_response(client_socket, "$" + to_string(value.size()) + "\r\n");
        send_raw(client_socket, value.data(), value.size());
        return;
    }

    send_response(client_socket, "-ERR unknown command\r\n");
}

void RedisServer::send_response(int client_socket, const std::string& response) {
    write(client_socket, response.c_str(), response.size());
}

void RedisServer::send_raw(int client_socket, const uint8_t* data, size_t size) {
    write(client_socket, data, size);
    write(client_socket, "\r\n", 2);
}
