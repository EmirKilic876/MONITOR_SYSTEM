cmake_minimum_required(VERSION 3.10)
project(MonitorSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header-ler üçin papkalary anyklamak
include_directories(common)
include_directories(server)
include_directories(client)

# Server programmasy (Diňe .cpp faýlyňyz gerek)
add_executable(server_app server/server.cpp)

# Client programmasy
add_executable(client_app client/client.cpp)

# Windows sistemasyndaky gerekli kitaphanalary baglamak
target_link_libraries(server_app ws2_32 advapi32 shell32)
target_link_libraries(client_app ws2_32 advapi32 shell32 user32)