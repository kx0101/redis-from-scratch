#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class RedisServer {
public:
    RedisServer(int port);
    void run();

private:
    int port_;
    int server_fd_;
    sockaddr_in address_;
    std::unordered_map<std::string, std::vector<uint8_t>> kv_store_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> expiry_store_;
    std::mutex kv_mutex_;

    void handle_client(int client_socket);
    void handle_command(const std::vector<std::vector<uint8_t>>& args, int client_socket);
    void handle_ping(int client_socket);
    void handle_echo(const std::vector<std::vector<uint8_t>>& args, int client_socket);
    void handle_set(const std::vector<std::vector<uint8_t>>& args, int client_socket);
    void handle_get(const std::vector<std::vector<uint8_t>>& args, int client_socket);
    void handle_del(const std::vector<std::vector<uint8_t>>& args, int client_socket);
    void send_response(int client_socket, const std::string& response);
    void send_raw(int client_socket, const uint8_t* data, size_t size);
};
