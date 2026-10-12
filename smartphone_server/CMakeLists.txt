cmake_minimum_required(VERSION 3.22)
project(servidor_grafico_vulkan CXX)

set(CMAKE_CXX_STANDARD 17)

# Correção: Primeiro o nome do binário alvo, depois os arquivos fontes
add_executable(servidor_grafico_vulkan servidor_grafico_vulkan.cpp)

# Vincula a biblioteca de carregamento dinâmico (dl) e o sistema de log do Android
target_link_libraries(servidor_grafico_vulkan dl log)

