#include "redis_server.h"

#include <cstring>
#include <iostream>

#include "parser/parser.h"
#include "utils/utils.h"

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
    constexpr int BUFFER_SIZE = 10;
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

        if (start > 0) {
            memmove(buffer.data(), buffer.data() + start, buf_len - start);
            buf_len -= start;
        }

        if (buf_len == buffer.size()) {
            buffer.resize(buffer.size() * 2);
        }
    }

    shutdown(client_socket, SHUT_RDWR);
    close(client_socket);
}

void RedisServer::handle_command(const vector<vector<uint8_t>>& args, int client_socket) {
    if (args.empty()) {
        return send_response(client_socket, "-ERR empty command\r\n");
    }

    string command = to_upper(bytes_to_string(args[0]));
    cout << "command: " << command << endl;

    if (command == "PING") {
        return handle_ping(client_socket);
    }

    if (command == "ECHO") {
        return handle_echo(args, client_socket);
    }

    if (command == "SET") {
        return handle_set(args, client_socket);
    }

    if (command == "GET") {
        return handle_get(args, client_socket);
    }

    if (command == "DEL") {
        return handle_del(args, client_socket);
    }

    if (command == "EXISTS") {
        return handle_exists(args, client_socket);
    }

    send_response(client_socket, "-ERR unknown command\r\n");
}

void RedisServer::handle_ping(int client_socket) {
    send_response(client_socket, "+PONG\r\n");
}

void RedisServer::handle_del(const vector<vector<uint8_t>>& args, int client_socket) {
    if (args.size() < 2) {
        send_response(client_socket, "-ERR DEL requires key\r\n");
        return;
    }

    int deleted_count = 0;

    string key = bytes_to_string(args[1]);
    {
        lock_guard<mutex> lock(kv_mutex_);

        for (size_t i = 1; i < args.size(); ++i) {
            string key = bytes_to_string(args[i]);

            auto it = kv_store_.find(key);
            if (it != kv_store_.end()) {
                kv_store_.erase(it);
                expiry_store_.erase(key);

                deleted_count++;
            }
        }
    }

    send_response(client_socket, "$" + to_string(deleted_count) + "\r\n");
}

void RedisServer::handle_exists(const vector<vector<uint8_t>>& args, int client_socket) {
    if (args.size() < 2) {
        send_response(client_socket, "-ERR EXISTS requires key\r\n");
        return;
    }

    int exists_count = 0;

    {
        lock_guard<mutex> lock(kv_mutex_);

        for (size_t i = 1; i < args.size(); ++i) {
            string key = bytes_to_string(args[i]);

            auto it = kv_store_.find(key);
            if (it != kv_store_.end()) {
                auto exp_it = expiry_store_.find(key);

                if (exp_it != expiry_store_.end() && chrono::steady_clock::now() >= exp_it->second) {
                    kv_store_.erase(it);
                    expiry_store_.erase(exp_it);

                    continue;
                }

                exists_count++;
            }
        }
    }

    send_response(client_socket, "$" + to_string(exists_count) + "\r\n");
}

void RedisServer::handle_echo(const vector<vector<uint8_t>>& args, int client_socket) {
    if (args.size() < 2) {
        send_response(client_socket, "-ERR ECHO requires argument\r\n");
        return;
    }

    string message = bytes_to_string(args[1]);
    send_response(client_socket, "+" + message + "\r\n");
}

void RedisServer::handle_set(const vector<vector<uint8_t>>& args, int client_socket) {
    if (args.size() < 3) {
        send_response(client_socket, "-ERR SET requires key and value\r\n");
        return;
    }

    string key = bytes_to_string(args[1]);
    vector<uint8_t> value(args[2]);

    optional<chrono::steady_clock::time_point> expiry_time;

    if (args.size() >= 5) {
        string option = to_upper(bytes_to_string(args[3]));

        if (option == "PX") {
            string px_str = bytes_to_string(args[4]);
            try {
                int64_t px = stoll(px_str);
                expiry_time = chrono::steady_clock::now() + chrono::milliseconds(px);
            } catch (const invalid_argument&) {
                send_response(client_socket, "-ERR invalid PX value\r\n");
                return;
            }
        }
    }

    {
        lock_guard<mutex> lock(kv_mutex_);
        kv_store_[key] = std::move(value);

        if (expiry_time) {
            expiry_store_[key] = *expiry_time;
        }
    }

    send_response(client_socket, "+OK\r\n");
}

void RedisServer::handle_get(const vector<vector<uint8_t>>& args, int client_socket) {
    if (args.size() < 2) {
        send_response(client_socket, "-ERR GET requires key\r\n");
        return;
    }

    string key = bytes_to_string(args[1]);
    vector<uint8_t> value;
    bool found = false;

    {
        lock_guard<mutex> lock(kv_mutex_);
        auto it = kv_store_.find(key);
        if (it != kv_store_.end()) {
            auto exp_it = expiry_store_.find(key);

            if (exp_it != expiry_store_.end() && chrono::steady_clock::now() >= exp_it->second) {
                kv_store_.erase(it);
                expiry_store_.erase(exp_it);
            } else {
                value = it->second;
                found = true;
            }
        }
    }

    if (found) {
        send_response(client_socket, "$" + to_string(value.size()) + "\r\n");
        send_raw(client_socket, value.data(), value.size());
    } else {
        send_response(client_socket, "$-1\r\n");
    }
}

void RedisServer::send_response(int client_socket, const std::string& response) {
    write(client_socket, response.c_str(), response.size());
}

void RedisServer::send_raw(int client_socket, const uint8_t* data, size_t size) {
    write(client_socket, data, size);
    write(client_socket, "\r\n", 2);
}
