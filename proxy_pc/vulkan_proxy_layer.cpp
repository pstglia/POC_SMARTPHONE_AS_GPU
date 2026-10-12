#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstdlib>

static PFN_vkGetInstanceProcAddr nextGIPA = nullptr;
static PFN_vkGetDeviceProcAddr   nextGDPA = nullptr;
static PFN_vkDestroyInstance nextDestroyInstance = nullptr;
static PFN_vkDestroyDevice   nextDestroyDevice = nullptr;
static PFN_vkCmdDraw         nextCmdDraw = nullptr;

// Nova assinatura para interceptar o fim do frame
static PFN_vkQueuePresentKHR nextQueuePresentKHR = nullptr;

static VkInstance currentInstance = VK_NULL_HANDLE;
static VkDevice   currentDevice = VK_NULL_HANDLE;

static int sockfd = -1;
static struct sockaddr_in servaddr;
static bool rede_inicializada = false;

static void* ponteiro_vertices_jogo = nullptr;
static VkDeviceSize tamanho_buffer_vertices = 0;

void inicializarRedeProxy() {
    if (rede_inicializada) return;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    std::memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(65432); 

    const char* ip_env = std::getenv("SMARTPHONE_IP");
    const char* ip_final = (ip_env != nullptr) ? ip_env : "10.170.47.49";

    servaddr.sin_addr.s_addr = inet_addr(ip_final);
    rede_inicializada = true;
    std::fprintf(stderr, "[USB_SMARTPHONE_PROXY] Tunel eletrico ativado para o IP: %s\n", ip_final);
}

static VkLayerInstanceCreateInfo* getInstanceChainInfo(const VkInstanceCreateInfo* pCreateInfo, VkLayerFunction function) {
    auto* item = reinterpret_cast<VkLayerInstanceCreateInfo*>(const_cast<void*>(pCreateInfo->pNext));
    while (item) {
        if (item->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO && item->function == function) return item;
        item = reinterpret_cast<VkLayerInstanceCreateInfo*>(const_cast<void*>(item->pNext));
    }
    return nullptr;
}

static VkLayerDeviceCreateInfo* getDeviceChainInfo(const VkDeviceCreateInfo* pCreateInfo, VkLayerFunction function) {
    auto* item = reinterpret_cast<VkLayerDeviceCreateInfo*>(const_cast<void*>(pCreateInfo->pNext));
    while (item) {
        if (item->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO && item->function == function) return item;
        item = reinterpret_cast<VkLayerDeviceCreateInfo*>(const_cast<void*>(item->pNext));
    }
    return nullptr;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkMapMemory(
    VkDevice device,
    VkDeviceMemory memory,
    VkDeviceSize offset,
    VkDeviceSize size,
    VkMemoryMapFlags flags,
    void** ppData)
{
    auto nextMapMemory = reinterpret_cast<PFN_vkMapMemory>(nextGDPA(device, "vkMapMemory"));
    VkResult res = nextMapMemory(device, memory, offset, size, flags, ppData);
    
    if (res == VK_SUCCESS && ppData && *ppData) {
        ponteiro_vertices_jogo = *ppData;
        tamanho_buffer_vertices = size;
    }
    return res;
}

static VKAPI_ATTR void VKAPI_CALL MyHook_vkCmdDraw(
    VkCommandBuffer commandBuffer,
    uint32_t vertexCount,
    uint32_t instanceCount,
    uint32_t firstVertex,
    uint32_t firstInstance)
{
    if (rede_inicializada && ponteiro_vertices_jogo && tamanho_buffer_vertices > 0) {
        const size_t MAX_PACKET_SIZE = 1400; 
        size_t bytes_despachados = 0;
        char* ponteiro_dados = (char*)ponteiro_vertices_jogo;

        while (bytes_despachados < tamanho_buffer_vertices) {
            size_t tamanho_bloco = std::min(MAX_PACKET_SIZE, (size_t)tamanho_buffer_vertices - bytes_despachados);
            sendto(sockfd, ponteiro_dados + bytes_despachados, tamanho_bloco, 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));
            bytes_despachados += tamanho_bloco;
        }
    }

    if (nextCmdDraw) {
        nextCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    }
}

// --- NOSSO NOVO GANCHO DE FIM DE QUADRO (PRESENT) ---
static VKAPI_ATTR VkResult VKAPI_CALL MyHook_vkQueuePresentKHR(
    VkQueue queue,
    const VkPresentInfoKHR* pPresentInfo)
{
    // Esse print vai provar que o PC terminou de processar o frame e está sincronizado com o tempo de tela
    std::fprintf(stderr, "[USB_SMARTPHONE_PROXY] vkQueuePresentKHR! Fim de frame alcancado no PC.\n");

    // No próximo passo, inseriremos aqui o bloco receptor UDP para resgatar os pixels do Moto G71

    if (nextQueuePresentKHR) {
        return nextQueuePresentKHR(queue, pPresentInfo);
    }
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(
    const VkInstanceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkInstance* pInstance)
{
    if (!pCreateInfo || !pInstance) return VK_ERROR_INITIALIZATION_FAILED;
    auto* chain = getInstanceChainInfo(pCreateInfo, VK_LAYER_LINK_INFO);
    if (!chain || !chain->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;

    auto* link = chain->u.pLayerInfo;
    auto nextCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(link->pfnNextGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
    nextGIPA = link->pfnNextGetInstanceProcAddr;

    chain->u.pLayerInfo = link->pNext;
    VkResult result = nextCreateInstance(pCreateInfo, pAllocator, pInstance);

    if (result == VK_SUCCESS) {
        currentInstance = *pInstance;
        inicializarRedeProxy(); 
        nextDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(nextGIPA(*pInstance, "vkDestroyInstance"));
    }
    return result;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator) {
    if (nextDestroyInstance) nextDestroyInstance(instance, pAllocator);
    if (instance == currentInstance) {
        currentInstance = VK_NULL_HANDLE;
        if (sockfd != -1) close(sockfd);
        rede_inicializada = false;
    }
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(
    VkPhysicalDevice physicalDevice,
    const VkDeviceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDevice* pDevice)
{
    if (!pCreateInfo || !pDevice || !nextGIPA) return VK_ERROR_INITIALIZATION_FAILED;
    auto* chain = getDeviceChainInfo(pCreateInfo, VK_LAYER_LINK_INFO);
    if (!chain || !chain->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;

    auto* link = chain->u.pLayerInfo;
    PFN_vkGetInstanceProcAddr layerNextGIPA = link->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr layerNextGDPA = link->pfnNextGetDeviceProcAddr;

    auto nextCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(layerNextGIPA(currentInstance, "vkCreateDevice"));
    if (!nextCreateDevice || !layerNextGDPA) return VK_ERROR_INITIALIZATION_FAILED;

    chain->u.pLayerInfo = link->pNext;
    VkResult result = nextCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);

    if (result == VK_SUCCESS) {
        currentDevice = *pDevice;
        nextGDPA = layerNextGDPA;
        nextDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(nextGDPA(*pDevice, "vkDestroyDevice"));
        nextCmdDraw = reinterpret_cast<PFN_vkCmdDraw>(nextGDPA(*pDevice, "vkCmdDraw"));
        
        // Mapeia e intercepta a função de apresentação
        nextQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(nextGDPA(*pDevice, "vkQueuePresentKHR"));
        
        std::fprintf(stderr, "[USB_SMARTPHONE_PROXY] vkCreateDevice OK; Tunel Grafico e Apresentacao Prontos!\n");
    }
    return result;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) {
    if (nextDestroyDevice) nextDestroyDevice(device, pAllocator);
    if (device == currentDevice) {
        currentDevice = VK_NULL_HANDLE;
        nextCmdDraw = nullptr;
        nextQueuePresentKHR = nullptr;
        nextGDPA = nullptr;
    }
}

extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
    if (!pName) return nullptr;
    if (std::strcmp(pName, "vkGetInstanceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vkGetInstanceProcAddr);
    if (std::strcmp(pName, "vkGetDeviceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vkGetDeviceProcAddr);
    if (std::strcmp(pName, "vkCreateInstance") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vkCreateInstance);
    if (std::strcmp(pName, "vkDestroyInstance") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vkDestroyInstance);
    if (std::strcmp(pName, "vkCreateDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vkCreateDevice);

    if (nextGIPA) return nextGIPA(instance, pName);
    return nullptr;
}

extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char* pName) {
    if (!pName) return nullptr;
    if (std::strcmp(pName, "vkGetDeviceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vkGetDeviceProcAddr);
    if (std::strcmp(pName, "vkDestroyDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vkDestroyDevice);
    if (std::strcmp(pName, "vkMapMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vkMapMemory); 
    if (std::strcmp(pName, "vkCmdDraw") == 0) return reinterpret_cast<PFN_vkVoidFunction>(MyHook_vkCmdDraw);
    
    // Vincula o gancho de apresentação na tabela de despacho do dispositivo
    if (std::strcmp(pName, "vkQueuePresentKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(MyHook_vkQueuePresentKHR);

    if (nextGDPA) return nextGDPA(device, pName);
    return nullptr;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* pVersionStruct) {
    if (!pVersionStruct || pVersionStruct->loaderLayerInterfaceVersion < 2) return VK_ERROR_INITIALIZATION_FAILED;
    pVersionStruct->loaderLayerInterfaceVersion = 2;
    pVersionStruct->pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    pVersionStruct->pfnGetDeviceProcAddr = vkGetDeviceProcAddr;
    pVersionStruct->pfnGetPhysicalDeviceProcAddr = nullptr;
    return VK_SUCCESS;
}
