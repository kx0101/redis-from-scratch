cmake_minimum_required(VERSION 3.10)
project(redis-cli)

set(CMAKE_CXX_STANDARD 17)

add_executable(redis-cli
    main.cpp
    parser/parser.cpp
    redis_server.cpp
    utils/utils.cpp
)
