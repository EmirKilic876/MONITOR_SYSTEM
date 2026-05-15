cmake_minimum_required(VERSION 3.10)
project(MonitorSystem)

# C++17 standartyny ulanmak
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header (.h) faýllarynyň papkalary
include_directories(common)
include_directories(server)
include_directories(client)

# Server programmasy
add_executable(server_app server/server.cpp third_party/sqlite/sqlite3.c)

# Client programmasy
add_executable(client_app client/client.cpp)

# --- Windows Kitaphanalaryny Baglamak (LNK1120-ni düzetmek üçin) ---
# Pdh: CPU monitoring üçin
# Iphlpapi: IP/Network maglumatlary üçin
# Ws2_32: Soketler üçin
# Advapi32: Registry we sistema maglumatlary üçin
set(WIN_LIBS 
    ws2_32 
    advapi32 
    shell32 
    pdh 
    version 
    iphlpapi 
    user32
    target_include_directories(server_app PRIVATE third_party/sqlite)

    target_sources(server_app PRIVATE
    third_party/sqlite/sqlite3.c
)

target_link_libraries(server_app PRIVATE ${WIN_LIBS})
target_link_libraries(client_app PRIVATE ${WIN_LIBS})
target_include_directories(server_app PRIVATE
    third_party/sqlite
)