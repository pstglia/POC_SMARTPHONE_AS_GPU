g++ -std=c++17 -O2 -fPIC -shared \
    -o /home/paulo/projetos/vulkan_proxy/libvulkan_proxy_layer.so \
    ~/projetos/vulkan_proxy/vulkan_proxy_layer.cpp

cp VkLayer_vulkan_proxy.json ~/.local/share/vulkan/explicit_layer.d/
