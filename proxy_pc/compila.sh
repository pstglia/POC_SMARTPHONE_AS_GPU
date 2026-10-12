g++ -std=c++17 -O2 -fPIC -shared \
    -o libvulkan_proxy_layer.so \
    vulkan_proxy_layer.cpp

cp VkLayer_vulkan_proxy.json ~/.local/share/vulkan/explicit_layer.d/
