cmake_minimum_required(VERSION 3.13.4)

set(target raspberry-car)

project(${target})

set(CMAKE_C_STANDARD 99)
#set(CMAKE_CXX_FLAGS -fsanitize=memory -fno-omit-frame-pointer -g)

include_directories(./)
include_directories(/usr/local/include)
link_directories(/usr/local/lib)

link_libraries(bcm2835)

add_executable(${target}
        car.c
        iobyte.c
        server.c
        main.c)
