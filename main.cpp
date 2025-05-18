#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

#include "redis_server.h"

int main() {
    RedisServer server(6379);
    server.run();

    return 0;
}
