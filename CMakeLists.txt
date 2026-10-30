cmake_minimum_required(VERSION 3.20)
project(StudentNotesChatbot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Crow is expected in the local "crow" folder (cloned from its official repository).
# This avoids relying on a Crow package being available in MSYS2.
add_executable(chatbot main.cpp)
target_include_directories(chatbot PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/crow/include")
target_compile_definitions(chatbot PRIVATE ASIO_STANDALONE)

if(WIN32)
    target_link_libraries(chatbot PRIVATE ws2_32 mswsock)
endif()
