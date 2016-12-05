project(tokenizer)
cmake_minimum_required(VERSION 2.8)

find_package(Threads)

#add_definitions(-std=c++11 -O2)
add_definitions(-std=c++11 -g)


file(GLOB_RECURSE SRC_LIST "src/*.h" "src/*.cpp" "../notes.mkd" "../data/datasets.mkd" "../SourcererCC/clone-detector/sourcerer-cc.properties")

#aux_source_directory(. SRC_LIST)

add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(${PROJECT_NAME} mysqlclient)

