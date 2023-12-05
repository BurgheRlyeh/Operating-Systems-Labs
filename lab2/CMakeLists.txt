cmake_minimum_required(VERSION 3.16)
project(lab2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Debug)#-Wall -Werror)

if(CMAKE_VERSION VERSION_LESS "3.7.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

find_package (Threads)
find_library (LIBRT rt)

file(GLOB CONNECTIONS connections/conn_*.h)
file(GLOB HOST proc_queue.h host/*.cpp host/*.h)
file(GLOB CLIENT proc_queue.h client/*.cpp client/*.h)

foreach(PATH_TO_CON ${CONNECTIONS})
    get_filename_component(FILE_NAME ${PATH_TO_CON} NAME_WLE )
    string(REGEX REPLACE "conn_" "" CONN_NAME ${FILE_NAME})

    add_executable(host_${CONN_NAME} ${HOST}
            connections/conn.h connections/conn_${CONN_NAME}.h connections/conn_${CONN_NAME}.cpp connections/connection.h)
    add_executable(client_${CONN_NAME}  ${CLIENT}
            connections/conn.h connections/conn_${CONN_NAME}.h connections/conn_${CONN_NAME}.cpp connections/connection.h host/host.h)
    target_link_libraries(host_${CONN_NAME} PUBLIC pthread rt )
    target_link_libraries(client_${CONN_NAME} PUBLIC pthread rt )
endforeach()
