# =====================================================
#  SysMonitor -- CMakeLists.txt (ROOT)
#  cmake -B build -G "Visual Studio 17 2022" -A x64
#  cmake --build build --config Release
# =====================================================
cmake_minimum_required(VERSION 3.16)
project(SysMonitor C)

set(CMAKE_C_STANDARD 11)

# SQLite amalgamation -- server/sqlite3.c we server/sqlite3.h
# https://www.sqlite.org/download.html -> amalgamation indir

# ---------- CLIENT ----------
add_executable(SysMonitorClient
    client/client.c
)
target_include_directories(SysMonitorClient PRIVATE common)
target_link_libraries(SysMonitorClient
    ws2_32 iphlpapi pdh powrprof wbemuuid psapi
    advapi32 setupapi
)
# Windows subsystem (konsol ýok)
set_target_properties(SysMonitorClient PROPERTIES
    WIN32_EXECUTABLE TRUE
    LINK_FLAGS "/SUBSYSTEM:WINDOWS"
)

# ---------- SERVER ----------
add_executable(SysMonitorServer
    server/server.c
    server/sqlite3.c     # <- amalgamation
)
target_include_directories(SysMonitorServer PRIVATE common server)
target_link_libraries(SysMonitorServer
    ws2_32 advapi32
)
set_target_properties(SysMonitorServer PROPERTIES
    WIN32_EXECUTABLE FALSE
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

cmake_minimum_required(VERSION 3.10)
project(MonitorSystem C)

set(CMAKE_C_STANDARD 11)

include_directories(common)
include_directories(server)

# Server programmasy
add_executable(server_app server/server.c server/sqlite3.c)

# Client programmasy
add_executable(client_app client/client.c)