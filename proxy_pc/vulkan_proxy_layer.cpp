#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstdlib>
#include <fcntl.h> 
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

static PFN_vkGetInstanceProcAddr nextGIPA = nullptr;
static PFN_vkGetDeviceProcAddr   nextGDPA = nullptr;
static PFN_vkDestroyInstance nextDestroyInstance = nullptr;
static PFN_vkDestroyDevice   nextDestroyDevice = nullptr;
static PFN_vkCmdDraw         nextCmdDraw = nullptr;
static PFN_vkQueuePresentKHR nextQueuePresentKHR = nullptr;

// Tabela de despacho para o encaminhamento do Shader Module
static PFN_vkCreateShaderModule nextCreateShaderModule = nullptr;
static PFN_vkDestroyShaderModule nextDestroyShaderModule = nullptr;

static VkInstance currentInstance = VK_NULL_HANDLE;
static VkDevice   currentDevice = VK_NULL_HANDLE;

static int sockfd = -1;
static struct sockaddr_in servaddr;
static bool rede_inicializada = false;

// --- CANAL DE INICIALIZAÇÃO E COMANDOS PESADOS TCP ---
static int tcp_client_fd = -1;
static bool tcp_inicializado = false;

static char buffer_envio_global[64 * 1024];
static std::vector<char> cache_geometria_local;
static std::atomic<bool> tem_geometria_pronta(false); // Garante visibilidade entre threads da CPU

static VkDeviceSize tamanho_mapeado_atual = 0;
static void* ponteiro_mapeado_atual = nullptr;

void inicializarRedeProxy();

static std::atomic<uint32_t> frame_enviado_id(0);
static std::atomic<uint32_t> frame_confirmado_id(0);
static const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

void threadEscutaACK() {
    int udp_receive_fd = socket(AF_INET, SOCK_DGRAM, 0);
    int opt_reuse = 1;
    setsockopt(udp_receive_fd, SOL_SOCKET, SO_REUSEADDR, &opt_reuse, sizeof(opt_reuse));

    struct sockaddr_in local_recv_addr;
    std::memset(&local_recv_addr, 0, sizeof(local_recv_addr));
    local_recv_addr.sin_family = AF_INET;
    
    // CORREÇÃO: Força o socket do PC a escutar em modo promíscuo aberto para qualquer 
    // IP vindo da sub-rede do cabo USB, casando estritamente apenas a porta 65433
    local_recv_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    local_recv_addr.sin_port = htons(65433); 

    if (bind(udp_receive_fd, (struct sockaddr*)&local_recv_addr, sizeof(local_recv_addr)) < 0) {
        std::fprintf(stderr, "[USB_SMARTPHONE_PROXY] Erro ao fixar porta de escuta UDP 65433.\n");
        return;
    }

    uint32_t ack_frame_received = 0;
    struct sockaddr_in de_addr;
    socklen_t de_len = sizeof(de_addr);

    // Loop paralelo bloqueante estável
    while (rede_inicializada) {
        ssize_t ack_res = recvfrom(udp_receive_fd, &ack_frame_received, sizeof(ack_frame_received), 0, (struct sockaddr *)&de_addr, &de_len);
        if (ack_res > 0) {
            // Destrava o Sliding Window de forma atômica
            frame_confirmado_id.store(ack_frame_received);
        }
    }
    close(udp_receive_fd);
}

void inicializarRedeProxyComThread() {
    if (rede_inicializada) return;
    inicializarRedeProxy();
    std::thread(threadEscutaACK).detach();
}

// --- INICIALIZADOR DE REDE PURIFICADO DO PC ---
void inicializarRedeProxy() {
    if (rede_inicializada) return;
    
    // 1. CONEXÃO MESTRE TCP (Porta 65431 - Controle)
    tcp_client_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in tcp_servaddr;
    std::memset(&tcp_servaddr, 0, sizeof(tcp_servaddr));
    tcp_servaddr.sin_family = AF_INET;
    tcp_servaddr.sin_port = htons(65431);
    
    const char* ip_env = std::getenv("SMARTPHONE_IP");
    const char* ip_final = (ip_env != nullptr) ? ip_env : "10.170.47.49";
    tcp_servaddr.sin_addr.s_addr = inet_addr(ip_final);

    std::fprintf(stderr, "[USB_SMARTPHONE_PROXY] Conectando ao canal TCP de controle: %s...\n", ip_final);
    if (connect(tcp_client_fd, (struct sockaddr*)&tcp_servaddr, sizeof(tcp_servaddr)) >= 0) {
        tcp_inicializado = true;
        std::fprintf(stderr, "[USB_SMARTPHONE_PROXY] Canal TCP de controle ativado com Sucesso!\n");
    }

    // 2. CONEXÃO DE STREAMING UDP (Porta 65432 - Geometria de Entrada)
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    std::memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(65432); 
    servaddr.sin_addr.s_addr = inet_addr(ip_final);
    
    rede_inicializada = true;
    std::fprintf(stderr, "[USB_SMARTPHONE_PROXY] Tunel UDP de Geometria ativado para o IP: %s\n", ip_final);
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

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) {
    auto nextMapMemory = reinterpret_cast<PFN_vkMapMemory>(nextGDPA(device, "vkMapMemory"));
    VkResult res = nextMapMemory(device, memory, offset, size, flags, ppData);
    if (res == VK_SUCCESS && ppData && *ppData) {
        ponteiro_mapeado_atual = *ppData;
        tamanho_mapeado_atual = size;
    }
    return res;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkUnmapMemory(VkDevice device, VkDeviceMemory memory) {
    auto nextUnmap = reinterpret_cast<PFN_vkUnmapMemory>(nextGDPA(device, "vkUnmapMemory"));
    if (ponteiro_mapeado_atual && tamanho_mapeado_atual > 0) {
        cache_geometria_local.resize(tamanho_mapeado_atual);
        std::memcpy(cache_geometria_local.data(), ponteiro_mapeado_atual, tamanho_mapeado_atual);
        tem_geometria_pronta = true;
        std::fprintf(stderr, "[USB_SMARTPHONE_PROXY] Geometria do Cubo capturada com sucesso no Unmap! (%lu bytes)\n", (unsigned long)tamanho_mapeado_atual);
        
        // --- APAGUE OU COMENTE ESTAS DUAS LINHAS ABAIXO: ---
        // ponteiro_mapeado_atual = nullptr;
        // tamanho_mapeado_atual = 0;
    }
    nextUnmap(device, memory);
}

static VKAPI_ATTR void VKAPI_CALL MyHook_vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    if (nextCmdDraw) nextCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

static VKAPI_ATTR VkResult VKAPI_CALL MyHook_vkQueuePresentKHR(
    VkQueue queue,
    const VkPresentInfoKHR* pPresentInfo)
{
    static uint32_t global_frame_id = 0;
    static uint32_t fps_counter = 0;
    static auto tempo_anterior = std::chrono::high_resolution_clock::now();
    
    fps_counter++;
    auto tempo_atual = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> tempo_decorrido = tempo_atual - tempo_anterior;

    if (tempo_decorrido.count() >= 1000.0) {
        double fps_real = (fps_counter * 1000.0) / tempo_decorrido.count();
        std::fprintf(stderr, "\n[METRICA GPU EXTERNA] FPS Real no PC Real: %.2f | Frame ID: %u\n\n", fps_real, global_frame_id);
        fps_counter = 0;
        tempo_anterior = tempo_atual;
    }

    uint32_t current_frame = frame_enviado_id.load();
    while ((current_frame - frame_confirmado_id.load()) >= MAX_FRAMES_IN_FLIGHT) {
        usleep(500); 
    }

    // CONDICIONAL PURIFICADA: Se o cache na RAM do PC contém os 262KB do cubo, 
    // bombardeia o cabo USB de forma incondicional frame por frame!
    if (rede_inicializada && !cache_geometria_local.empty()) {
        const size_t MAX_PAYLOAD_SIZE = 1400; 
        size_t bytes_despachados = 0;
        size_t tamanho_total_vbo = cache_geometria_local.size();
        char* ponteiro_dados = cache_geometria_local.data();
        uint32_t chunk_id = 0;
        uint32_t tamanho_total = (uint32_t)tamanho_total_vbo;

        while (bytes_despachados < tamanho_total_vbo) {
            size_t tamanho_bloco = std::min(MAX_PAYLOAD_SIZE, tamanho_total_vbo - bytes_despachados);
            
            std::vector<char> pacote_envio(12 + tamanho_bloco);
            std::memcpy(pacote_envio.data(), &tamanho_total, 4);
            std::memcpy(pacote_envio.data() + 4, &current_frame, 4); 
            std::memcpy(pacote_envio.data() + 8, &chunk_id, 4);
            std::memcpy(pacote_envio.data() + 12, ponteiro_dados + bytes_despachados, tamanho_bloco);

            sendto(sockfd, pacote_envio.data(), pacote_envio.size(), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));
            
            bytes_despachados += tamanho_bloco;
            chunk_id++;
            usleep(5);
        }
        frame_enviado_id.fetch_add(1);
        global_frame_id++;
    }

    if (nextQueuePresentKHR) {
        return nextQueuePresentKHR(queue, pPresentInfo);
    }
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
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
        inicializarRedeProxyComThread(); 
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

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
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
        nextQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(nextGDPA(*pDevice, "vkQueuePresentKHR"));
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
    if (std::strcmp(pName, "vkUnmapMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vkUnmapMemory);
    if (std::strcmp(pName, "vkCmdDraw") == 0) return reinterpret_cast<PFN_vkVoidFunction>(MyHook_vkCmdDraw);
    if (std::strcmp(pName, "vkQueuePresentKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(MyHook_vkQueuePresentKHR);
    if (std::strcmp(pName, "vkCreateShaderModule") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vkCreateShaderModule);
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

// --- GANCHO DE SHADERS CORRIGIDO COM FECHAMENTO DE CONEXÃO CONTROLOGADA ---
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(
    VkDevice device,
    const VkShaderModuleCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkShaderModule* pShaderModule)
{
    if (nextCreateShaderModule == nullptr) {
        nextCreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(nextGDPA(device, "vkCreateShaderModule"));
    }

    std::fprintf(stderr, "[USB_SMARTPHONE_PROXY] vkCreateShaderModule: %lu bytes\n", pCreateInfo->codeSize);

    // Rastreia estaticamente quantos shaders o vkcube já compilou (são 2 no total)
    static int shaders_compilados_count = 0;

    if (tcp_inicializado && pCreateInfo->codeSize > 0) {
        uint32_t tipo_comando = 0x53484452; // Tag "[SHDR]"
        uint32_t tamanho_shader = (uint32_t)pCreateInfo->codeSize;

        send(tcp_client_fd, &tipo_comando, 4, 0);
        send(tcp_client_fd, &tamanho_shader, 4, 0);
        send(tcp_client_fd, pCreateInfo->pCode, pCreateInfo->codeSize, 0);

        uint32_t status_compilacao = 0;
        recv(tcp_client_fd, &status_compilacao, 4, MSG_WAITALL);
        
        if (status_compilacao == 0x4B4F2B5B) { // "[+OK]"
            shaders_compilados_count++;
        }

        // XEQUE-MATE DO SINCRONISMO: Se compilou os dois shaders obrigatórios, desliga o TCP limpo!
        if (shaders_compilados_count >= 2) {
            std::fprintf(stderr, "[USB_SMARTPHONE_PROXY] Todos os Shaders enviados. Fechando canal TCP mestre...\n");
            
            uint32_t comando_exit = 0x45584954; // Tag "[EXIT]" (4 bytes)
            send(tcp_client_fd, &comando_exit, 4, 0);
            
            // Fecha o socket fisicamente no PC real e desativa a flag de controle
            close(tcp_client_fd);
            tcp_client_fd = -1;
            tcp_inicializado = false;
        }
    }

    return nextCreateShaderModule(device, pCreateInfo, pAllocator, pShaderModule);
}
