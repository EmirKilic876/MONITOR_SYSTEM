cmake_minimum_required(VERSION 3.10)
project(MonitorSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header faýllary üçin papkalar
include_directories(common)
include_directories(server)
include_directories(client)

# Server programmasy
add_executable(server_app server/server.cpp)

# Client programmasy
add_executable(client_app client/client.cpp)

# --- IŇ MÖHÜM BÖLÜM: Kitaphanalary baglamak ---
# LNK1120 ýalňyşlygyny aýyrmak üçin şu aşakdakylar hökmandyr:

set(WIN_LIBS 
    ws2_32       # Tor (Network) üçin
    advapi32     # Registry we Sistema maglumatlary üçin
    shell32      # Shell funksiýalary üçin
    pdh          # CPU we Performance Monitoring üçin (MÖHÜM!)
    version      # Faýl wersiýalary üçin
    iphlpapi     # IP we Network adapter maglumatlary üçin
    user32       # Windows GUI/System metricleri üçin
)

target_link_libraries(server_app PRIVATE ${WIN_LIBS})
target_link_libraries(client_app PRIVATE ${WIN_LIBS})