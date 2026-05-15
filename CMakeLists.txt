cmake_minimum_required(VERSION 3.10)
project(MonitorSystem)

# C++ standartyny bellemek
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Header (.h) faýllarynyň duran papkalaryny görkezmek (MÖHÜM!)
include_directories(common)
include_directories(server)
include_directories(client)

# 2. Server programmasy
# SQLite3 (C faýly) we Server (CPP faýly) bilelikde goşulmaly
add_executable(server_app 
    server/server.cpp 
    server/sqlite3.c
)

# 3. Client programmasy
add_executable(client_app 
    client/client.cpp
)

# 4. Windows-yň ulgam kitaphanalaryny baglamak
# ws2_32: Soketler (Network) üçin gerek
# shell32, advapi32: Sistem maglumatlaryny almak üçin gerek bolup biler
target_link_libraries(server_app ws2_32)
target_link_libraries(client_app ws2_32 shell32 advapi32)

# SQLite3 üçin Windows-da käwagt şu hem gerek bolýar
if(WIN32)
    target_compile_definitions(server_app PRIVATE SQLITE_ENABLE_RTREE)
endif()