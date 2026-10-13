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
typedef VkResult (*PFN_vkCreateBuffer)(VkDevice, const VkBufferCreateInfo*, const VkAllocationCallbacks*, VkBuffer*);
typedef void (*PFN_vkGetBufferMemoryRequirements)(VkDevice, VkBuffer, VkMemoryRequirements*);
typedef VkResult (*PFN_vkBindBufferMemory)(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize);
typedef VkResult (*PFN_vkMapMemory)(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void**);
typedef void (*PFN_vkUnmapMemory)(VkDevice, VkDeviceMemory);
typedef void (*PFN_vkDestroyBuffer)(VkDevice, VkBuffer, const VkAllocationCallbacks*);

typedef VkResult (*PFN_vkCreateCommandPool)(VkDevice, const VkCommandPoolCreateInfo*, const VkAllocationCallbacks*, VkCommandPool*);
typedef VkResult (*PFN_vkAllocateCommandBuffers)(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer*);
typedef VkResult (*PFN_vkBeginCommandBuffer)(VkCommandBuffer, const VkCommandBufferBeginInfo*);
typedef VkResult (*PFN_vkEndCommandBuffer)(VkCommandBuffer);
typedef void (*PFN_vkDestroyCommandPool)(VkDevice, VkCommandPool, const VkAllocationCallbacks*);
typedef VkResult (*PFN_vkCreateRenderPass)(VkDevice, const VkRenderPassCreateInfo*, const VkAllocationCallbacks*, VkRenderPass*);
typedef void (*PFN_vkDestroyRenderPass)(VkDevice, VkRenderPass, const VkAllocationCallbacks*);

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
static PFN_vkCreateBuffer real_vkCreateBuffer = nullptr;
static PFN_vkGetBufferMemoryRequirements real_vkGetBufferMemoryRequirements = nullptr;
static PFN_vkBindBufferMemory real_vkBindBufferMemory = nullptr;
static PFN_vkMapMemory real_vkMapMemory = nullptr;
static PFN_vkUnmapMemory real_vkUnmapMemory = nullptr;
static PFN_vkDestroyBuffer real_vkDestroyBuffer = nullptr;

static PFN_vkCreateCommandPool real_vkCreateCommandPool = nullptr;
static PFN_vkAllocateCommandBuffers real_vkAllocateCommandBuffers = nullptr;
static PFN_vkBeginCommandBuffer real_vkBeginCommandBuffer = nullptr;
static PFN_vkEndCommandBuffer real_vkEndCommandBuffer = nullptr;
static PFN_vkDestroyCommandPool real_vkDestroyCommandPool = nullptr;

static PFN_vkCreateRenderPass real_vkCreateRenderPass = nullptr;
static PFN_vkDestroyRenderPass real_vkDestroyRenderPass = nullptr;

void* vulkan_lib = nullptr;

bool carregarFuncoesVulkan() {
    vulkan_lib = dlopen("/system/lib64/libvulkan.so", RTLD_NOW);
    if (!vulkan_lib) return false;
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
    real_vkCreateBuffer = (PFN_vkCreateBuffer)dlsym(vulkan_lib, "vkCreateBuffer");
    real_vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)dlsym(vulkan_lib, "vkGetBufferMemoryRequirements");
    real_vkBindBufferMemory = (PFN_vkBindBufferMemory)dlsym(vulkan_lib, "vkBindBufferMemory");
    real_vkMapMemory = (PFN_vkMapMemory)dlsym(vulkan_lib, "vkMapMemory");
    real_vkUnmapMemory = (PFN_vkUnmapMemory)dlsym(vulkan_lib, "vkUnmapMemory");
    real_vkDestroyBuffer = (PFN_vkDestroyBuffer)dlsym(vulkan_lib, "vkDestroyBuffer");
    real_vkCreateCommandPool = (PFN_vkCreateCommandPool)dlsym(vulkan_lib, "vkCreateCommandPool");
    real_vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)dlsym(vulkan_lib, "vkAllocateCommandBuffers");
    real_vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)dlsym(vulkan_lib, "vkBeginCommandBuffer");
    real_vkEndCommandBuffer = (PFN_vkEndCommandBuffer)dlsym(vulkan_lib, "vkEndCommandBuffer");
    real_vkDestroyCommandPool = (PFN_vkDestroyCommandPool)dlsym(vulkan_lib, "vkDestroyCommandPool");
    real_vkCreateRenderPass = (PFN_vkCreateRenderPass)dlsym(vulkan_lib, "vkCreateRenderPass");
    real_vkDestroyRenderPass = (PFN_vkDestroyRenderPass)dlsym(vulkan_lib, "vkDestroyRenderPass");

    return true;
}

struct ContextoVulkanHeadless {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkImage headlessImage = VK_NULL_HANDLE;
    VkDeviceMemory headlessMemory = VK_NULL_HANDLE;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkDeviceSize tamanhoBufferAlocado = 0;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
};

uint32_t encontrarTipoMemoria(VkPhysicalDevice physicalDevice, uint32_t tipoFiltro, VkMemoryPropertyFlags propriedades) {
    VkPhysicalDeviceMemoryProperties memProperties;
    real_vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((tipoFiltro & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & propriedades) == propriedades) return i;
    }
    return 0;
}

bool alocarVertexBufferDinamico(ContextoVulkanHeadless& vk, VkDeviceSize tamanhoNecessario) {
    if (vk.vertexBuffer != VK_NULL_HANDLE && vk.tamanhoBufferAlocado >= tamanhoNecessario) return true;
    if (vk.vertexBuffer != VK_NULL_HANDLE) real_vkDestroyBuffer(vk.device, vk.vertexBuffer, nullptr);
    if (vk.vertexBufferMemory != VK_NULL_HANDLE) real_vkFreeMemory(vk.device, vk.vertexBufferMemory, nullptr);
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = tamanhoNecessario;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (real_vkCreateBuffer(vk.device, &bufferInfo, nullptr, &vk.vertexBuffer) != VK_SUCCESS) return false;
    VkMemoryRequirements memRequirements;
    real_vkGetBufferMemoryRequirements(vk.device, vk.vertexBuffer, &memRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = encontrarTipoMemoria(vk.physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (real_vkAllocateMemory(vk.device, &allocInfo, nullptr, &vk.vertexBufferMemory) != VK_SUCCESS) return false;
    real_vkBindBufferMemory(vk.device, vk.vertexBuffer, vk.vertexBufferMemory, 0);
    vk.tamanhoBufferAlocado = tamanhoNecessario;
    return true;
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
    std::cout << "[+] GPU Fisica Detectada: " << deviceProperties.deviceName << std::endl;
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
    if (real_vkCreateImage(vk.device, &imageInfo, nullptr, &vk.headlessImage) != VK_SUCCESS) return false;
    VkMemoryRequirements memRequirements;
    real_vkGetImageMemoryRequirements(vk.device, vk.headlessImage, &memRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = encontrarTipoMemoria(vk.physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (real_vkAllocateMemory(vk.device, &allocInfo, nullptr, &vk.headlessMemory) != VK_SUCCESS) return false;
    real_vkBindImageMemory(vk.device, vk.headlessImage, vk.headlessMemory, 0);
    std::cout << "[+] Contexto Vulkan e Render Target Alocados!" << std::endl;
    return true;
}

void limparVulkan(ContextoVulkanHeadless& vk) {
    if (vk.commandPool != VK_NULL_HANDLE) real_vkDestroyCommandPool(vk.device, vk.commandPool, nullptr);

    if (vk.vertexBuffer != VK_NULL_HANDLE) real_vkDestroyBuffer(vk.device, vk.vertexBuffer, nullptr);
    if (vk.vertexBufferMemory != VK_NULL_HANDLE) real_vkFreeMemory(vk.device, vk.vertexBufferMemory, nullptr);
    if (vk.headlessImage != VK_NULL_HANDLE) real_vkDestroyImage(vk.device, vk.headlessImage, nullptr);
    if (vk.headlessMemory != VK_NULL_HANDLE) real_vkFreeMemory(vk.device, vk.headlessMemory, nullptr);
    if (vk.device != VK_NULL_HANDLE) real_vkDestroyDevice(vk.device, nullptr);
    if (vk.instance != VK_NULL_HANDLE) real_vkDestroyInstance(vk.instance, nullptr);
    if (vulkan_lib) dlclose(vulkan_lib);
    if (vk.renderPass != VK_NULL_HANDLE) real_vkDestroyRenderPass(vk.device, vk.renderPass, nullptr);
}

// --- FUNÇÃO DE CRIAÇÃO DA FILA DE COMANDOS HEADLESS ---
bool inicializarFilaComandosHeadless(ContextoVulkanHeadless& vk) {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = 0; // Fila grafica padrao carregada no main
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Permite re-gravar a cada frame

    if (real_vkCreateCommandPool(vk.device, &poolInfo, nullptr, &vk.commandPool) != VK_SUCCESS) {
        std::cerr << "[-] Falha ao alocar VkCommandPool na Adreno." << std::endl;
        return false;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = vk.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (real_vkAllocateCommandBuffers(vk.device, &allocInfo, &vk.commandBuffer) != VK_SUCCESS) {
        std::cerr << "[-] Falha ao instanciar VkCommandBuffer Headless." << std::endl;
        return false;
    }

    std::cout << "[+] Factory de Comandos e VkCommandBuffer instanciados na GPU do smartphone!" << std::endl;
    return true;
}

bool inicializarRenderPassHeadless(ContextoVulkanHeadless& vk) {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R8G8B8A8_UNORM; // Casado com o formato da nossa VkImage Headless
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // Limpa a tela no inicio do frame!
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Salva o resultado na VRAM no final
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // Pronto para copia/retorno

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (real_vkCreateRenderPass(vk.device, &renderPassInfo, nullptr, &vk.renderPass) != VK_SUCCESS) {
        std::cerr << "[-] Falha ao instanciar VkRenderPass na Adreno." << std::endl;
        return false;
    }

    std::cout << "[+] VkRenderPass Headless alocado com sucesso na memoria da GPU Qualcomm!" << std::endl;
    return true;
}

int main() {
    ContextoVulkanHeadless vk;
    if (!inicializarVulkanHeadless(vk)) return -1;

    // --- 1. CONFIGURAÇÃO DO SERVIDOR TCP DE INICIALIZAÇÃO (Porta 65431) ---
    int tcp_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt_tcp = 1;
    setsockopt(tcp_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt_tcp, sizeof(opt_tcp));

    struct sockaddr_in tcp_serv_addr, tcp_cli_addr;
    std::memset(&tcp_serv_addr, 0, sizeof(tcp_serv_addr));
    tcp_serv_addr.sin_family = AF_INET;
    tcp_serv_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_serv_addr.sin_port = htons(65431);

    bind(tcp_listen_fd, (struct sockaddr*)&tcp_serv_addr, sizeof(tcp_serv_addr));
    listen(tcp_listen_fd, 1);
    std::cout << "[*] Servidor TCP de Inicializacao Ativo na porta 65431..." << std::endl;

    socklen_t tcp_len = sizeof(tcp_cli_addr);
    int tcp_client_fd = accept(tcp_listen_fd, (struct sockaddr*)&tcp_cli_addr, &tcp_len);
    
    if (tcp_client_fd >= 0) {
        std::cout << "[+] Canal TCP Conectado! Aguardando Shaders dinâmicos do PC..." << std::endl;
        
        while (true) {
            uint32_t tag_comando = 0;
            ssize_t r_tag = recv(tcp_client_fd, &tag_comando, 4, MSG_WAITALL);
            if (r_tag <= 0) break; // Conexão fechada ao fim do setup inicial

            if (tag_comando == 0x53484452) { // Tag "[SHDR]"
                uint32_t tamanho_shader = 0;
                recv(tcp_client_fd, &tamanho_shader, 4, MSG_WAITALL);

                std::vector<uint32_t> spirv_buffer(tamanho_shader / 4);
                recv(tcp_client_fd, spirv_buffer.data(), tamanho_shader, MSG_WAITALL);

                // COMPILAÇÃO DINÂMICA DIRETO NO SILÍCIO DA QUALCOMM!
                VkShaderModuleCreateInfo shaderInfo{};
                static PFN_vkCreateShaderModule real_vkCreateShaderModule = (PFN_vkCreateShaderModule)dlsym(vulkan_lib, "vkCreateShaderModule");
                shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                shaderInfo.codeSize = tamanho_shader;
                shaderInfo.pCode = spirv_buffer.data();

                VkShaderModule modulo_temporario;
                if (real_vkCreateShaderModule(vk.device, &shaderInfo, nullptr, &modulo_temporario) == VK_SUCCESS) {
                    std::cout << "[Vulkan GPU] SUCESSO: Shader de " << tamanho_shader << " bytes compilado na Adreno 619!" << std::endl;
                    uint32_t status_ok = 0x4B4F2B5B; // "[+OK]"
                    send(tcp_client_fd, &status_ok, 4, 0);
                }
            }
        }
        close(tcp_client_fd);
    }
    close(tcp_listen_fd);

    // --- 2. CONFIGURAÇÃO DO STREAMING UDP GEOMÉTRICO (Porta 65432) ---
    int sockfd; struct sockaddr_in servaddr, cliaddr;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    int optval_rcv = 4 * 1024 * 1024;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &optval_rcv, sizeof(optval_rcv));
    std::memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);
    bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));

    if (!inicializarFilaComandosHeadless(vk)) return -1;
    if (!inicializarRenderPassHeadless(vk)) return -1;

    std::cout << "[*] Canal UDP pronto para receber VBOs na porta " << PORT << "..." << std::endl;

    std::vector<char> socket_read_buffer(MAX_BUFFER_SIZE);
    std::vector<char> frame_geometry_accumulator;
    socklen_t len = sizeof(cliaddr);

    while (true) {
        frame_geometry_accumulator.clear();
        size_t bytes_recebidos_vbo = 0; uint32_t tamanho_esperado = 0; uint32_t current_frame_id = 0; bool frame_concluido = false;
        while (true) {
            ssize_t n = recvfrom(sockfd, socket_read_buffer.data(), MAX_BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
            if (n <= 0) continue;
            if (n >= 12) {
                std::memcpy(&tamanho_esperado, socket_read_buffer.data(), 4);
                std::memcpy(&current_frame_id, socket_read_buffer.data() + 4, 4);
                uint32_t chunk_id; std::memcpy(&chunk_id, socket_read_buffer.data() + 8, 4);
                size_t tamanho_payload = n - 12;
                frame_geometry_accumulator.insert(frame_geometry_accumulator.end(), socket_read_buffer.begin() + 12, socket_read_buffer.begin() + n);
                bytes_recebidos_vbo += tamanho_payload;
                if (bytes_recebidos_vbo >= tamanho_esperado) { frame_concluido = true; break; }
            }
        }
        if (frame_concluido && bytes_recebidos_vbo == tamanho_esperado) {
            if (alocarVertexBufferDinamico(vk, bytes_recebidos_vbo)) {
                void* ptrVRAM = nullptr;
                if (real_vkMapMemory(vk.device, vk.vertexBufferMemory, 0, bytes_recebidos_vbo, 0, &ptrVRAM) == VK_SUCCESS && ptrVRAM) {
                    std::memcpy(ptrVRAM, frame_geometry_accumulator.data(), bytes_recebidos_vbo);
                    real_vkUnmapMemory(vk.device, vk.vertexBufferMemory);
                }
            }
            // --- SUBSTITUA A LINHA DO SENDTO ANTIGA NO CELULAR POR ESTA: ---
            // Força o envio do ACK especificamente para a porta 65433 aberta na thread do PC
            struct sockaddr_in target_ack_addr = cliaddr;
            target_ack_addr.sin_port = htons(65433);

            sendto(sockfd, &current_frame_id, sizeof(current_frame_id), 0, (const struct sockaddr *)&target_ack_addr, sizeof(target_ack_addr));
        }
    }
    close(sockfd); limparVulkan(vk); return 0;
}
