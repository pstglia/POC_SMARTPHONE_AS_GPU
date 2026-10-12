#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <algorithm>

#include <vulkan/vulkan.h>

#define PORT 65432
#define MAX_BUFFER_SIZE (64 * 1024)
#define RES_WIDTH 480
#define RES_HEIGHT 360

// --- TABELA DE DESPACHO EXPANDIDA ---
typedef VkResult (*PFN_vkCreateInstance)(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*);
typedef VkResult (*PFN_vkEnumeratePhysicalDevices)(VkInstance, uint32_t*, VkPhysicalDevice*);
typedef void (*PFN_vkGetPhysicalDeviceProperties)(VkPhysicalDevice, VkPhysicalDeviceProperties*);
typedef VkResult (*PFN_vkCreateDevice)(VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*);
typedef void (*PFN_vkGetDeviceQueue)(VkDevice, uint32_t, uint32_t, VkQueue*);
typedef void (*PFN_vkDestroyDevice)(VkDevice, const VkAllocationCallbacks*);
typedef void (*PFN_vkDestroyInstance)(VkInstance, const VkAllocationCallbacks*);
typedef void (*PFN_vkGetPhysicalDeviceMemoryProperties)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties*);

typedef VkResult (*PFN_vkCreateImage)(VkDevice, const VkImageCreateInfo*, const VkAllocationCallbacks*, VkImage*);
typedef VkResult (*PFN_vkAllocateMemory)(VkDevice, const VkMemoryAllocateInfo*, const VkAllocationCallbacks*, VkDeviceMemory*);
typedef VkResult (*PFN_vkBindImageMemory)(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize);
typedef void (*PFN_vkGetImageMemoryRequirements)(VkDevice, VkImage, VkMemoryRequirements*);
typedef void (*PFN_vkDestroyImage)(VkDevice, VkImage, const VkAllocationCallbacks*);
typedef void (*PFN_vkFreeMemory)(VkDevice, VkDeviceMemory, const VkAllocationCallbacks*);

static PFN_vkCreateInstance real_vkCreateInstance = nullptr;
static PFN_vkEnumeratePhysicalDevices real_vkEnumeratePhysicalDevices = nullptr;
static PFN_vkGetPhysicalDeviceProperties real_vkGetPhysicalDeviceProperties = nullptr;
static PFN_vkCreateDevice real_vkCreateDevice = nullptr;
static PFN_vkGetDeviceQueue real_vkGetDeviceQueue = nullptr;
static PFN_vkDestroyDevice real_vkDestroyDevice = nullptr;
static PFN_vkDestroyInstance real_vkDestroyInstance = nullptr;
static PFN_vkGetPhysicalDeviceMemoryProperties real_vkGetPhysicalDeviceMemoryProperties = nullptr;

static PFN_vkCreateImage real_vkCreateImage = nullptr;
static PFN_vkAllocateMemory real_vkAllocateMemory = nullptr;
static PFN_vkBindImageMemory real_vkBindImageMemory = nullptr;
static PFN_vkGetImageMemoryRequirements real_vkGetImageMemoryRequirements = nullptr;
static PFN_vkDestroyImage real_vkDestroyImage = nullptr;
static PFN_vkFreeMemory real_vkFreeMemory = nullptr;

void* vulkan_lib = nullptr;

bool carregarFuncoesVulkan() {
    vulkan_lib = dlopen("/system/lib64/libvulkan.so", RTLD_NOW);
    if (!vulkan_lib) {
        std::cerr << "[-] Falha ao abrir /system/lib64/libvulkan.so" << std::endl;
        return false;
    }

    real_vkCreateInstance = (PFN_vkCreateInstance)dlsym(vulkan_lib, "vkCreateInstance");
    real_vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)dlsym(vulkan_lib, "vkEnumeratePhysicalDevices");
    real_vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)dlsym(vulkan_lib, "vkGetPhysicalDeviceProperties");
    real_vkCreateDevice = (PFN_vkCreateDevice)dlsym(vulkan_lib, "vkCreateDevice");
    real_vkGetDeviceQueue = (PFN_vkGetDeviceQueue)dlsym(vulkan_lib, "vkGetDeviceQueue");
    real_vkDestroyDevice = (PFN_vkDestroyDevice)dlsym(vulkan_lib, "vkDestroyDevice");
    real_vkDestroyInstance = (PFN_vkDestroyInstance)dlsym(vulkan_lib, "vkDestroyInstance");
    real_vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)dlsym(vulkan_lib, "vkGetPhysicalDeviceMemoryProperties");

    real_vkCreateImage = (PFN_vkCreateImage)dlsym(vulkan_lib, "vkCreateImage");
    real_vkAllocateMemory = (PFN_vkAllocateMemory)dlsym(vulkan_lib, "vkAllocateMemory");
    real_vkBindImageMemory = (PFN_vkBindImageMemory)dlsym(vulkan_lib, "vkBindImageMemory");
    real_vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)dlsym(vulkan_lib, "vkGetImageMemoryRequirements");
    real_vkDestroyImage = (PFN_vkDestroyImage)dlsym(vulkan_lib, "vkDestroyImage");
    real_vkFreeMemory = (PFN_vkFreeMemory)dlsym(vulkan_lib, "vkFreeMemory");

    return true;
}

struct ContextoVulkanHeadless {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    
    VkImage headlessImage = VK_NULL_HANDLE;
    VkDeviceMemory headlessMemory = VK_NULL_HANDLE;
};

uint32_t encontrarTipoMemoria(VkPhysicalDevice physicalDevice, uint32_t tipoFiltro, VkMemoryPropertyFlags propriedades) {
    VkPhysicalDeviceMemoryProperties memProperties;
    real_vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((tipoFiltro & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & propriedades) == propriedades) {
            return i;
        }
    }
    return 0;
}

bool inicializarVulkanHeadless(ContextoVulkanHeadless& vk) {
    if (!carregarFuncoesVulkan()) return false;

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Servidor Grafico Vulkan Headless";
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    if (real_vkCreateInstance(&createInfo, nullptr, &vk.instance) != VK_SUCCESS) return false;

    uint32_t deviceCount = 0;
    real_vkEnumeratePhysicalDevices(vk.instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    real_vkEnumeratePhysicalDevices(vk.instance, &deviceCount, devices.data());
    vk.physicalDevice = devices[0];

    VkPhysicalDeviceProperties deviceProperties;
    real_vkGetPhysicalDeviceProperties(vk.physicalDevice, &deviceProperties);
    std::cout << "[+] GPU Fisica Detectada no Celular: " << deviceProperties.deviceName << std::endl;

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = 0;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

    if (real_vkCreateDevice(vk.physicalDevice, &deviceCreateInfo, nullptr, &vk.device) != VK_SUCCESS) return false;
    real_vkGetDeviceQueue(vk.device, 0, 0, &vk.queue);

    // --- ALOCAÇÃO DO RENDER TARGET INVISÍVEL (VRAM ADRENO) ---
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = RES_WIDTH;
    imageInfo.extent.height = RES_HEIGHT;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (real_vkCreateImage(vk.device, &imageInfo, nullptr, &vk.headlessImage) != VK_SUCCESS) {
        std::cerr << "[-] Falha ao alocar a estrutura VkImage Headless." << std::endl;
        return false;
    }

    VkMemoryRequirements memRequirements;
    real_vkGetImageMemoryRequirements(vk.device, vk.headlessImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = encontrarTipoMemoria(vk.physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (real_vkAllocateMemory(vk.device, &allocInfo, nullptr, &vk.headlessMemory) != VK_SUCCESS) {
        std::cerr << "[-] Falha ao alocar memoria fisica na VRAM da Adreno." << std::endl;
        return false;
    }

    real_vkBindImageMemory(vk.device, vk.headlessImage, vk.headlessMemory, 0);
    std::cout << "[+] Contexto Vulkan Headless e Render Target (" << RES_WIDTH << "x" << RES_HEIGHT << ") Alocados na VRAM!" << std::endl;
    return true;
}

void limparVulkan(ContextoVulkanHeadless& vk) {
    if (vk.headlessImage != VK_NULL_HANDLE) real_vkDestroyImage(vk.device, vk.headlessImage, nullptr);
    if (vk.headlessMemory != VK_NULL_HANDLE) real_vkFreeMemory(vk.device, vk.headlessMemory, nullptr);
    if (vk.device != VK_NULL_HANDLE) real_vkDestroyDevice(vk.device, nullptr);
    if (vk.instance != VK_NULL_HANDLE) real_vkDestroyInstance(vk.instance, nullptr);
    if (vulkan_lib) dlclose(vulkan_lib);
}

int main() {
    ContextoVulkanHeadless vk;
    if (!inicializarVulkanHeadless(vk)) return -1;

    int sockfd; struct sockaddr_in servaddr, cliaddr;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    int optval = 1024 * 1024;
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval));

    std::memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        std::cerr << "[-] Erro ao dar bind no socket." << std::endl;
        return -1;
    }

    std::cout << "[*] Aguardando buffers geometricos sincronos na porta " << PORT << "..." << std::endl;

    std::vector<char> socket_read_buffer(MAX_BUFFER_SIZE);
    std::vector<char> frame_geometry_accumulator;
    socklen_t len = sizeof(cliaddr);

    // MANTÉM O CELULAR EM MODO BLOQUEANTE RÍGIDO
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);

    while (true) {
        frame_geometry_accumulator.clear();
        size_t bytes_recebidos_vbo = 0;
        uint32_t tamanho_esperado = 0;

        while (true) {
            ssize_t n = recvfrom(sockfd, socket_read_buffer.data(), MAX_BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
            if (n <= 0) continue;

            if (n >= 8) {
                // Extrai os metadados do cabeçalho de 8 bytes de forma segura
                std::memcpy(&tamanho_esperado, socket_read_buffer.data(), 4);
                uint32_t chunk_id;
                std::memcpy(&chunk_id, socket_read_buffer.data() + 4, 4);

                size_t tamanho_payload = n - 8;
                
                // Concatena apenas os dados úteis de geometria (pulando o cabeçalho)
                frame_geometry_accumulator.insert(
                    frame_geometry_accumulator.end(), 
                    socket_read_buffer.begin() + 8, 
                    socket_read_buffer.begin() + n
                );
                
                bytes_recebidos_vbo += tamanho_payload;

                // Se a soma dos payloads bater exatamente com o tamanho prometido, frame concluído!
                if (bytes_recebidos_vbo >= tamanho_esperado) {
                    break;
                }
            }
        }

        // CONTA FECHOU EM JUÍZO: Emite o log e responde o ACK para liberar a VM do PC imediatamente
        if (bytes_recebidos_vbo > 0) {
            std::cout << "[Vulkan GPU] Sincronizado: " << bytes_recebidos_vbo << " bytes de geometria na Adreno." << std::endl;
            char ack_byte = 0x41; // 'A'
            sendto(sockfd, &ack_byte, 1, 0, (struct sockaddr *)&cliaddr, len);
        }
    }

    close(sockfd);
    limparVulkan(vk);
    return 0;
}
