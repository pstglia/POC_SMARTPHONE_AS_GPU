#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>

#include <cstdio>
#include <cstring>

// Protótipo de uma layer explícita para teste.
// Limitação intencional: tabelas globais, para um único dispositivo.

static PFN_vkGetInstanceProcAddr nextGIPA = nullptr;
static PFN_vkGetDeviceProcAddr   nextGDPA = nullptr;

static PFN_vkDestroyInstance nextDestroyInstance = nullptr;
static PFN_vkDestroyDevice   nextDestroyDevice = nullptr;
static PFN_vkCmdDraw         nextCmdDraw = nullptr;

static VkInstance currentInstance = VK_NULL_HANDLE;
static VkDevice   currentDevice = VK_NULL_HANDLE;

// ------------------------------------------------------------------
// Busca informações de encadeamento fornecidas pelo Vulkan Loader.
// ------------------------------------------------------------------

static VkLayerInstanceCreateInfo* getInstanceChainInfo(
    const VkInstanceCreateInfo* pCreateInfo,
    VkLayerFunction function)
{
    auto* item = reinterpret_cast<VkLayerInstanceCreateInfo*>(
        const_cast<void*>(pCreateInfo->pNext));

    while (item) {
        if (item->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO &&
            item->function == function) {
            return item;
        }

        item = reinterpret_cast<VkLayerInstanceCreateInfo*>(
            const_cast<void*>(item->pNext));
    }

    return nullptr;
}

static VkLayerDeviceCreateInfo* getDeviceChainInfo(
    const VkDeviceCreateInfo* pCreateInfo,
    VkLayerFunction function)
{
    auto* item = reinterpret_cast<VkLayerDeviceCreateInfo*>(
        const_cast<void*>(pCreateInfo->pNext));

    while (item) {
        if (item->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO &&
            item->function == function) {
            return item;
        }

        item = reinterpret_cast<VkLayerDeviceCreateInfo*>(
            const_cast<void*>(item->pNext));
    }

    return nullptr;
}

// ------------------------------------------------------------------
// Hook de desenho.
// ------------------------------------------------------------------

static VKAPI_ATTR void VKAPI_CALL MyHook_vkCmdDraw(
    VkCommandBuffer commandBuffer,
    uint32_t vertexCount,
    uint32_t instanceCount,
    uint32_t firstVertex,
    uint32_t firstInstance)
{
    std::fprintf(stderr,
        "[USB_SMARTPHONE_PROXY] vkCmdDraw interceptado! "
        "vertices=%u instances=%u firstVertex=%u firstInstance=%u\n",
        vertexCount, instanceCount, firstVertex, firstInstance);

    if (nextCmdDraw) {
        nextCmdDraw(commandBuffer,
                    vertexCount,
                    instanceCount,
                    firstVertex,
                    firstInstance);
    }
}

// ------------------------------------------------------------------
// Criação e destruição da instância.
// ------------------------------------------------------------------

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(
    const VkInstanceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkInstance* pInstance)
{
    if (!pCreateInfo || !pInstance) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    auto* chain = getInstanceChainInfo(pCreateInfo, VK_LAYER_LINK_INFO);

    if (!chain || !chain->u.pLayerInfo) {
        std::fprintf(stderr,
            "[USB_SMARTPHONE_PROXY] Link de instância não encontrado.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    auto* link = chain->u.pLayerInfo;
    auto nextCreateInstance =
        reinterpret_cast<PFN_vkCreateInstance>(
            link->pfnNextGetInstanceProcAddr(
                VK_NULL_HANDLE, "vkCreateInstance"));

    // Preserva o GIPA da próxima camada.
    nextGIPA = link->pfnNextGetInstanceProcAddr;

    if (!nextCreateInstance || !nextGIPA) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Permite que a próxima layer leia o seu próprio nó.
    chain->u.pLayerInfo = link->pNext;

    VkResult result = nextCreateInstance(
        pCreateInfo, pAllocator, pInstance);

    if (result == VK_SUCCESS) {
        currentInstance = *pInstance;

        nextDestroyInstance =
            reinterpret_cast<PFN_vkDestroyInstance>(
                nextGIPA(*pInstance, "vkDestroyInstance"));

        std::fprintf(stderr,
            "[USB_SMARTPHONE_PROXY] vkCreateInstance OK\n");
    }

    return result;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(
    VkInstance instance,
    const VkAllocationCallbacks* pAllocator)
{
    if (nextDestroyInstance) {
        nextDestroyInstance(instance, pAllocator);
    }

    if (instance == currentInstance) {
        currentInstance = VK_NULL_HANDLE;
        nextDestroyInstance = nullptr;
    }
}

// ------------------------------------------------------------------
// Criação e destruição do dispositivo.
// ------------------------------------------------------------------

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(
    VkPhysicalDevice physicalDevice,
    const VkDeviceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDevice* pDevice)
{
    if (!pCreateInfo || !pDevice || !nextGIPA) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    auto* chain = getDeviceChainInfo(pCreateInfo, VK_LAYER_LINK_INFO);

    if (!chain || !chain->u.pLayerInfo) {
        std::fprintf(stderr,
            "[USB_SMARTPHONE_PROXY] Link de dispositivo não encontrado.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    auto* link = chain->u.pLayerInfo;

    PFN_vkGetInstanceProcAddr layerNextGIPA =
        link->pfnNextGetInstanceProcAddr;

    PFN_vkGetDeviceProcAddr layerNextGDPA =
        link->pfnNextGetDeviceProcAddr;

    auto nextCreateDevice =
        reinterpret_cast<PFN_vkCreateDevice>(
            layerNextGIPA(currentInstance, "vkCreateDevice"));

    if (!nextCreateDevice || !layerNextGDPA) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Avança o encadeamento antes de chamar a próxima layer.
    chain->u.pLayerInfo = link->pNext;

    VkResult result = nextCreateDevice(
        physicalDevice, pCreateInfo, pAllocator, pDevice);

    if (result == VK_SUCCESS) {
        currentDevice = *pDevice;
        nextGDPA = layerNextGDPA;

        nextDestroyDevice =
            reinterpret_cast<PFN_vkDestroyDevice>(
                nextGDPA(*pDevice, "vkDestroyDevice"));

        nextCmdDraw =
            reinterpret_cast<PFN_vkCmdDraw>(
                nextGDPA(*pDevice, "vkCmdDraw"));

        std::fprintf(stderr,
            "[USB_SMARTPHONE_PROXY] vkCreateDevice OK; "
            "vkCmdDraw %s\n",
            nextCmdDraw ? "encontrado" : "NAO encontrado");
    }

    return result;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(
    VkDevice device,
    const VkAllocationCallbacks* pAllocator)
{
    if (nextDestroyDevice) {
        nextDestroyDevice(device, pAllocator);
    }

    if (device == currentDevice) {
        currentDevice = VK_NULL_HANDLE;
        nextDestroyDevice = nullptr;
        nextCmdDraw = nullptr;
        nextGDPA = nullptr;
    }
}

// ------------------------------------------------------------------
// Resolução dos comandos da instância.
// ------------------------------------------------------------------

extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vkGetInstanceProcAddr(VkInstance instance, const char* pName)
{
    if (!pName) {
        return nullptr;
    }

    if (std::strcmp(pName, "vkGetInstanceProcAddr") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(
            vkGetInstanceProcAddr);

    if (std::strcmp(pName, "vkGetDeviceProcAddr") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(
            vkGetDeviceProcAddr);

    if (std::strcmp(pName, "vkCreateInstance") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(vkCreateInstance);

    if (std::strcmp(pName, "vkDestroyInstance") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(vkDestroyInstance);

    if (std::strcmp(pName, "vkCreateDevice") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(vkCreateDevice);

    // Demais comandos de instância: encaminha para a próxima layer.
    if (nextGIPA) {
        return nextGIPA(instance, pName);
    }

    return nullptr;
}

// ------------------------------------------------------------------
// Resolução dos comandos do dispositivo.
// ------------------------------------------------------------------

extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vkGetDeviceProcAddr(VkDevice device, const char* pName)
{
    if (!pName) {
        return nullptr;
    }

    if (std::strcmp(pName, "vkGetDeviceProcAddr") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(
            vkGetDeviceProcAddr);

    if (std::strcmp(pName, "vkDestroyDevice") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(vkDestroyDevice);

    if (std::strcmp(pName, "vkCmdDraw") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(MyHook_vkCmdDraw);

    // Todos os outros comandos de dispositivo são encaminhados.
    if (nextGDPA) {
        return nextGDPA(device, pName);
    }

    return nullptr;
}

// ------------------------------------------------------------------
// Negociação da interface layer/loader. Não é negociação de ICD.
// ------------------------------------------------------------------

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
vkNegotiateLoaderLayerInterfaceVersion(
    VkNegotiateLayerInterface* pVersionStruct)
{
    if (!pVersionStruct) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (pVersionStruct->loaderLayerInterfaceVersion < 2) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    pVersionStruct->loaderLayerInterfaceVersion = 2;
    pVersionStruct->pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    pVersionStruct->pfnGetDeviceProcAddr = vkGetDeviceProcAddr;
    pVersionStruct->pfnGetPhysicalDeviceProcAddr = nullptr;

    return VK_SUCCESS;
}

