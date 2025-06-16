cmake_minimum_required(VERSION 3.10)
project(myproject LANGUAGES CXX)

# Устанавливаем стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# libpqxx через pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)
if (NOT PQXX_FOUND)
    message(FATAL_ERROR "libpqxx not found. Установите libpqxx и убедитесь, что pkg-config видит его.")
endif()
include_directories(${PQXX_INCLUDE_DIRS})
link_directories(${PQXX_LIBRARY_DIRS})

# Crow
find_package(Crow REQUIRED)
if (NOT TARGET Crow::Crow)
    message(FATAL_ERROR "Crow library not found. Установите Crow и убедитесь, что CrowConfig.cmake в путях поиска.")
endif()

# hiredis
find_path(HIREDIS_INCLUDE_DIR
    NAMES hiredis/hiredis.h
    PATHS /usr/include /usr/local/include
)
find_library(HIREDIS_LIB
    NAMES hiredis
    PATHS /usr/lib /usr/local/lib
)
if (NOT HIREDIS_INCLUDE_DIR OR NOT HIREDIS_LIB)
    message(FATAL_ERROR "hiredis not found. Установите пакет hiredis.")
endif()
include_directories(${HIREDIS_INCLUDE_DIR})

# redis-plus-plus
find_path(REDIS_PLUS_PLUS_INCLUDE_DIR
    NAMES sw/redis++/redis++.h
    PATHS /usr/include /usr/local/include
)
find_library(REDIS_PLUS_PLUS_LIB
    NAMES redis++
    PATHS /usr/lib /usr/local/lib
)
if (NOT REDIS_PLUS_PLUS_INCLUDE_DIR OR NOT REDIS_PLUS_PLUS_LIB)
    message(FATAL_ERROR "redis-plus-plus not found. Установите redis-plus-plus и убедитесь, что заголовки/библиотеки доступны.")
endif()
include_directories(${REDIS_PLUS_PLUS_INCLUDE_DIR})

# Threads
find_package(Threads REQUIRED)

# Исполняемый файл
add_executable(myproject_exec
    src/main.cpp
)

# Include dirs для таргета
target_include_directories(myproject_exec PRIVATE
    ${PQXX_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIR}
    ${REDIS_PLUS_PLUS_INCLUDE_DIR}
)

# Линкуем библиотеки
target_link_libraries(myproject_exec PRIVATE
    Crow::Crow
    ${PQXX_LIBRARIES}
    ${HIREDIS_LIB}
    ${REDIS_PLUS_PLUS_LIB}
    Threads::Threads
)

# Диагностические сообщения
message(STATUS "libpqxx include dirs: ${PQXX_INCLUDE_DIRS}")
message(STATUS "libpqxx library dirs: ${PQXX_LIBRARY_DIRS}")
message(STATUS "hiredis include dir: ${HIREDIS_INCLUDE_DIR}")
message(STATUS "hiredis lib: ${HIREDIS_LIB}")
message(STATUS "redis-plus-plus include dir: ${REDIS_PLUS_PLUS_INCLUDE_DIR}")
message(STATUS "redis-plus-plus lib: ${REDIS_PLUS_PLUS_LIB}")
message(STATUS "Crow target: Crow::Crow")
