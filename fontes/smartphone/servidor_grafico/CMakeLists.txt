cmake_minimum_required(VERSION 3.22)
project(servidor_grafico CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(servidor_grafico servidor_grafico.cpp)

# Vincula as bibliotecas gráficas nativas do Android e de log do sistema
target_link_libraries(servidor_grafico EGL GLESv3 log)

