cmake_minimum_required(VERSION 3.12.1)

set(target raspberry-car)

project(${target})

#set(CMAKE_C_STANDARD 99)
#set(CMAKE_C_FLAGS -fsanitize=memory -fno-omit-frame-pointer -g)

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

#link_libraries(jpeg)

add_executable(${target}
        bcm2835/bcm2835.c
        iobyte.c
        select.c
        camera.c
        car.c
        server.c
        main.c)

target_link_libraries(${target}
        pthread jpeg)
