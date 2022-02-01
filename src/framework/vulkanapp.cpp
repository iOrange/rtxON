#include "vulkanapp.h"

// include volk.c for implementation
#include "volk.c"


void FPSMeter::Update(const float dt) {
    this->fpsAccumulator += dt - this->fpsHistory[this->historyPointer];
    this->fpsHistory[this->historyPointer] = dt;
    this->historyPointer = (this->historyPointer + 1) % FPSMeter::kFPSHistorySize;
    this->fps = (this->fpsAccumulator > 0.0f) ? (1.0f / (this->fpsAccumulator / static_cast<float>(FPSMeter::kFPSHistorySize))) : FLT_MAX;
}

float FPSMeter::GetFPS() const {
    return this->fps;
}

float FPSMeter::GetFrameTime() const {
    return 1000.0f / this->fps;
}



VulkanApp::VulkanApp()
    : mSettings({})
    , mWindow(nullptr)
    , mInstance(VK_NULL_HANDLE)
    , mPhysicalDevice(VK_NULL_HANDLE)
    , mDevice(VK_NULL_HANDLE)
    , mSurfaceFormat({})
    , mSurface(VK_NULL_HANDLE)
    , mSwapchain(VK_NULL_HANDLE)
    , mCommandPool(VK_NULL_HANDLE)
    , mSemaphoreImageAcquired(VK_NULL_HANDLE)
    , mSemaphoreRenderFinished(VK_NULL_HANDLE)
    , mGraphicsQueueFamilyIndex(0u)
    , mComputeQueueFamilyIndex(0u)
    , mTransferQueueFamilyIndex(0u)
    , mGraphicsQueue(VK_NULL_HANDLE)
    , mComputeQueue(VK_NULL_HANDLE)
    , mTransferQueue(VK_NULL_HANDLE)
{

}
VulkanApp::~VulkanApp() {
    this->FreeVulkan();
}

void VulkanApp::Run() {
    if (this->Initialize()) {
        this->Loop();
        this->Shutdown();
        this->FreeResources();
    }
}

bool VulkanApp::Initialize() {
    if (!glfwInit()) {
        return false;
    }

    if (!glfwVulkanSupported()) {
        return false;
    }

    if (VK_SUCCESS != volkInitialize()) {
        return false;
    }

    this->InitializeSettings();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(static_cast<int>(mSettings.resolutionX),
                                          static_cast<int>(mSettings.resolutionY),
                                          mSettings.name.c_str(),
                                          nullptr, nullptr);
    if (!window) {
        return false;
    }

    glfwSetWindowUserPointer(window, this);

    glfwSetKeyCallback(window, [](GLFWwindow* wnd, int key, int scancode, int action, int mods) {
        VulkanApp* _this = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(wnd));
        _this->OnKey(key, scancode, action, mods);
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow* wnd, int button, int action, int mods) {
        VulkanApp* _this = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(wnd));
        _this->OnMouseButton(button, action, mods);
    });
    glfwSetCursorPosCallback(window, [](GLFWwindow* wnd, double x, double y) {
        VulkanApp* _this = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(wnd));
        _this->OnMouseMove(static_cast<float>(x), static_cast<float>(y));
    });

    mWindow = window;

    if (!this->InitializeVulkan()) {
        return false;
    }

    volkLoadInstance(mInstance);

    if (!this->InitializeDevicesAndQueues()) {
        return false;
    }
    if (!this->InitializeSurface()) {
        return false;
    }
    if (!this->InitializeSwapchain()) {
        return false;
    }
    if (!this->InitializeFencesAndCommandPool()) {
        return false;
    }

    vulkanhelpers::Initialize(mPhysicalDevice, mDevice, mCommandPool, mGraphicsQueue);

    if (!this->InitializeOffscreenImage()) {
        return false;
    }
    if (!this->InitializeCommandBuffers()) {
        return false;
    }
    if (!this->InitializeSynchronization()) {
        return false;
    }

    this->InitApp();
    this->FillCommandBuffers();

    return true;
}

void VulkanApp::Loop() {
    glfwSetTime(0.0);
    double curTime, prevTime = 0.0, deltaTime = 0.0;
    while (!glfwWindowShouldClose(mWindow)) {
        curTime = glfwGetTime();
        deltaTime = curTime - prevTime;
        prevTime = curTime;

        this->ProcessFrame(static_cast<float>(deltaTime));

        glfwPollEvents();
    }
}

void VulkanApp::Shutdown() {
    vkDeviceWaitIdle(mDevice);

    glfwTerminate();
}

void VulkanApp::InitializeSettings() {
    mSettings.name = "VulkanApp";
    mSettings.resolutionX = 1280;
    mSettings.resolutionY = 720;
    mSettings.surfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;
    mSettings.enableValidation = false;
    mSettings.enableVSync = true;
    mSettings.supportRaytracing = false;
    mSettings.supportDescriptorIndexing = false;

    this->InitSettings();
}

bool VulkanApp::InitializeVulkan() {
    VkApplicationInfo appInfo;
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = mSettings.name.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "VulkanApp";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    uint32_t requiredExtensionsCount = 0;
    const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionsCount);

    Array<const char*> extensions;
    Array<const char*> layers;

    extensions.insert(extensions.begin(), requiredExtensions, requiredExtensions + requiredExtensionsCount);

    if (mSettings.enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    VkInstanceCreateInfo instInfo;
    instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pNext = nullptr;
    instInfo.flags = 0;
    instInfo.pApplicationInfo = &appInfo;
    instInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instInfo.ppEnabledExtensionNames = extensions.data();
    instInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    instInfo.ppEnabledLayerNames = layers.data();

    VkResult error = vkCreateInstance(&instInfo, nullptr, &mInstance);
    if (VK_SUCCESS != error) {
        CHECK_VK_ERROR(error, "vkCreateInstance");
        return false;
    }

    return true;
}

bool VulkanApp::InitializeDevicesAndQueues() {
    uint32_t numPhysDevices = 0;
    VkResult error = vkEnumeratePhysicalDevices(mInstance, &numPhysDevices, nullptr);
    if (VK_SUCCESS != error || !numPhysDevices) {
        CHECK_VK_ERROR(error, "vkEnumeratePhysicalDevices");
        return false;
    }

    Array<VkPhysicalDevice> physDevices(numPhysDevices);
    vkEnumeratePhysicalDevices(mInstance, &numPhysDevices, physDevices.data());
    mPhysicalDevice = physDevices[0];

    // find our queues
    const VkQueueFlagBits askingFlags[3] = { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_TRANSFER_BIT };
    uint32_t queuesIndices[3] = { ~0u, ~0u, ~0u };

    uint32_t queueFamilyPropertyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyPropertyCount, nullptr);
    Array<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());

    for (size_t i = 0; i < 3; ++i) {
        const VkQueueFlagBits flag = askingFlags[i];
        uint32_t& queueIdx = queuesIndices[i];

        if (flag == VK_QUEUE_COMPUTE_BIT) {
            for (uint32_t j = 0; j < queueFamilyPropertyCount; ++j) {
                if ((queueFamilyProperties[j].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                   !(queueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                    queueIdx = j;
                    break;
                }
            }
        } else if (flag == VK_QUEUE_TRANSFER_BIT) {
            for (uint32_t j = 0; j < queueFamilyPropertyCount; ++j) {
                if ((queueFamilyProperties[j].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                   !(queueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                   !(queueFamilyProperties[j].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                    queueIdx = j;
                    break;
                }
            }
        }

        if (queueIdx == ~0u) {
            for (uint32_t j = 0; j < queueFamilyPropertyCount; ++j) {
                if (queueFamilyProperties[j].queueFlags & flag) {
                    queueIdx = j;
                    break;
                }
            }
        }
    }

    mGraphicsQueueFamilyIndex = queuesIndices[0];
    mComputeQueueFamilyIndex = queuesIndices[1];
    mTransferQueueFamilyIndex = queuesIndices[2];

    // create device
    Array<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
    const float priority = 0.0f;

    VkDeviceQueueCreateInfo deviceQueueCreateInfo;
    deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo.pNext = nullptr;
    deviceQueueCreateInfo.flags = 0;
    deviceQueueCreateInfo.queueFamilyIndex = mGraphicsQueueFamilyIndex;
    deviceQueueCreateInfo.queueCount = 1;
    deviceQueueCreateInfo.pQueuePriorities = &priority;
    deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);

    if (mComputeQueueFamilyIndex != mGraphicsQueueFamilyIndex) {
        deviceQueueCreateInfo.queueFamilyIndex = mComputeQueueFamilyIndex;
        deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
    }
    if (mTransferQueueFamilyIndex != mGraphicsQueueFamilyIndex && mTransferQueueFamilyIndex != mComputeQueueFamilyIndex) {
        deviceQueueCreateInfo.queueFamilyIndex = mTransferQueueFamilyIndex;
        deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
    }

    VkPhysicalDeviceFeatures2 features2 = { };
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipeline = {};
    rayTracingPipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR rayTracingStructure = {};
    rayTracingStructure.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress = {};
    bufferDeviceAddress.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;

    Array<const char*> deviceExtensions({ VK_KHR_SWAPCHAIN_EXTENSION_NAME });
    if (mSettings.supportRaytracing) {
        deviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

        // VK_KHR_ray_tracing requires VK_EXT_descriptor_indexing extension so we make sure it's enabled as well
        if (!mSettings.supportDescriptorIndexing) {
            mSettings.supportDescriptorIndexing = true;
        }

        bufferDeviceAddress.bufferDeviceAddress = VK_TRUE;
        rayTracingPipeline.pNext = &bufferDeviceAddress;
        rayTracingPipeline.rayTracingPipeline = VK_TRUE;
        rayTracingStructure.pNext = &rayTracingPipeline;
        rayTracingStructure.accelerationStructure = VK_TRUE;
        features2.pNext = &rayTracingStructure;
    }

    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexing = { };
    descriptorIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

    if (mSettings.supportDescriptorIndexing) {
        deviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

        if (features2.pNext) {
            descriptorIndexing.pNext = features2.pNext;
        }

        features2.pNext = &descriptorIndexing;
    }

    vkGetPhysicalDeviceFeatures2(mPhysicalDevice, &features2); // enable all the features our GPU has

    VkDeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &features2;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = nullptr;

    error = vkCreateDevice(mPhysicalDevice, &deviceCreateInfo, nullptr, &mDevice);
    if (VK_SUCCESS != error) {
        CHECK_VK_ERROR(error, "vkCreateDevice");
        return false;
    }

    // get our queues handles
    vkGetDeviceQueue(mDevice, mGraphicsQueueFamilyIndex, 0, &mGraphicsQueue);
    vkGetDeviceQueue(mDevice, mComputeQueueFamilyIndex, 0, &mComputeQueue);
    vkGetDeviceQueue(mDevice, mTransferQueueFamilyIndex, 0, &mTransferQueue);

    // if raytracing support requested - let's get raytracing properties to know shader header size and max recursion
    if (mSettings.supportRaytracing) {
        mRTProps = {};
        mRTProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

        VkPhysicalDeviceProperties2 devProps;
        devProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        devProps.pNext = &mRTProps;
        devProps.properties = {};

        vkGetPhysicalDeviceProperties2(mPhysicalDevice, &devProps);
    }

    return true;
}

bool VulkanApp::InitializeSurface() {
    VkResult error = glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mSurface);
    if (VK_SUCCESS != error) {
        CHECK_VK_ERROR(error, "glfwCreateWindowSurface");
        return false;
    }

    VkBool32 supportPresent = VK_FALSE;
    error = vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, mGraphicsQueueFamilyIndex, mSurface, &supportPresent);
    if (VK_SUCCESS != error || !supportPresent) {
        CHECK_VK_ERROR(error, "vkGetPhysicalDeviceSurfaceSupportKHR");
        return false;
    }

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, nullptr);
    Array<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, surfaceFormats.data());

    if (formatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
        mSurfaceFormat.format = mSettings.surfaceFormat;
        mSurfaceFormat.colorSpace = surfaceFormats[0].colorSpace;
    } else {
        bool found = false;
        for (const VkSurfaceFormatKHR& surfaceFormat : surfaceFormats) {
            if (surfaceFormat.format == mSettings.surfaceFormat) {
                mSurfaceFormat = surfaceFormat;
                found = true;
                break;
            }
        }
        if (!found) {
            mSurfaceFormat = surfaceFormats[0];
        }
    }

    return true;
}

bool VulkanApp::InitializeSwapchain() {
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VkResult error = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &surfaceCapabilities);
    if (VK_SUCCESS != error) {
        return false;
    }

    // make sure we stay in our surface's limits
    mSettings.resolutionX = Clamp(mSettings.resolutionX, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.currentExtent.width);
    mSettings.resolutionY = Clamp(mSettings.resolutionY, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.currentExtent.height);

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &presentModeCount, nullptr);
    Array<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &presentModeCount, presentModes.data());

    // trying to find best present mode for us
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (!mSettings.enableVSync) {
        // if we don't want vsync - let's find best one
        for (const VkPresentModeKHR mode : presentModes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                // this is the best one, so if we found it - just quit
                presentMode = mode;
                break;
            } else if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                // we'll use this one if no mailbox supported
                presentMode = mode;
            }
        }
    }

    VkSwapchainKHR prevSwapchain = mSwapchain;

    VkSwapchainCreateInfoKHR swapchainCreateInfo;
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.pNext = nullptr;
    swapchainCreateInfo.flags = 0;
    swapchainCreateInfo.surface = mSurface;
    swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount;
    swapchainCreateInfo.imageFormat = mSurfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = mSurfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = { mSettings.resolutionX, mSettings.resolutionY };
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.queueFamilyIndexCount = 0;
    swapchainCreateInfo.pQueueFamilyIndices = nullptr;
    swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = presentMode;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = prevSwapchain;

    error = vkCreateSwapchainKHR(mDevice, &swapchainCreateInfo, nullptr, &mSwapchain);
    if (VK_SUCCESS != error) {
        return false;
    }

    if (prevSwapchain) {
        for (VkImageView & imageView : mSwapchainImageViews) {
            vkDestroyImageView(mDevice, imageView, nullptr);
            imageView = VK_NULL_HANDLE;
        }
        vkDestroySwapchainKHR(mDevice, prevSwapchain, nullptr);
    }

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, nullptr);
    mSwapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, mSwapchainImages.data());

    mSwapchainImageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo imageViewCreateInfo;
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = nullptr;
        imageViewCreateInfo.format = mSurfaceFormat.format;
        imageViewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.image = mSwapchainImages[i];
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.components = { };

        error = vkCreateImageView(mDevice, &imageViewCreateInfo, nullptr, &mSwapchainImageViews[i]);
        if (VK_SUCCESS != error) {
            return false;
        }
    }

    return true;
}

bool VulkanApp::InitializeFencesAndCommandPool() {
    VkFenceCreateInfo fenceCreateInfo;
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    mWaitForFrameFences.resize(mSwapchainImages.size());
    for (VkFence& fence : mWaitForFrameFences) {
        vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &fence);
    }

    VkCommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = nullptr;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = mGraphicsQueueFamilyIndex;

    const VkResult error = vkCreateCommandPool(mDevice, &commandPoolCreateInfo, nullptr, &mCommandPool);
    return (VK_SUCCESS == error);
}

bool VulkanApp::InitializeOffscreenImage() {
    const VkExtent3D extent = { mSettings.resolutionX, mSettings.resolutionY, 1 };
    VkResult error = mOffscreenImage.Create(VK_IMAGE_TYPE_2D,
                                            mSurfaceFormat.format,
                                            extent,
                                            VK_IMAGE_TILING_OPTIMAL,
                                            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (VK_SUCCESS != error) {
        return false;
    }

    VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    error = mOffscreenImage.CreateImageView(VK_IMAGE_VIEW_TYPE_2D, mSurfaceFormat.format, range);
    return (VK_SUCCESS == error);
}

bool VulkanApp::InitializeCommandBuffers() {
    mCommandBuffers.resize(mSwapchainImages.size());

    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = nullptr;
    commandBufferAllocateInfo.commandPool = mCommandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>(mCommandBuffers.size());

    const VkResult error = vkAllocateCommandBuffers(mDevice, &commandBufferAllocateInfo, mCommandBuffers.data());
    return (VK_SUCCESS == error);
}

bool VulkanApp::InitializeSynchronization() {
    VkSemaphoreCreateInfo semaphoreCreatInfo;
    semaphoreCreatInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreatInfo.pNext = nullptr;
    semaphoreCreatInfo.flags = 0;

    VkResult error = vkCreateSemaphore(mDevice, &semaphoreCreatInfo, nullptr, &mSemaphoreImageAcquired);
    if (VK_SUCCESS != error) {
        return false;
    }

    error = vkCreateSemaphore(mDevice, &semaphoreCreatInfo, nullptr, &mSemaphoreRenderFinished);
    return (VK_SUCCESS == error);
}

void VulkanApp::FillCommandBuffers() {
    VkCommandBufferBeginInfo commandBufferBeginInfo;
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pNext = nullptr;
    commandBufferBeginInfo.flags = 0;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;

    VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    for (size_t i = 0; i < mCommandBuffers.size(); i++) {
        const VkCommandBuffer commandBuffer = mCommandBuffers[i];

        VkResult error = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
        CHECK_VK_ERROR(error, "vkBeginCommandBuffer");

        vulkanhelpers::ImageBarrier(commandBuffer,
                                    mOffscreenImage.GetImage(),
                                    subresourceRange,
                                    0,
                                    VK_ACCESS_SHADER_WRITE_BIT,
                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_GENERAL);

        this->FillCommandBuffer(commandBuffer, i); // user draw code

        vulkanhelpers::ImageBarrier(commandBuffer,
                                    mSwapchainImages[i],
                                    subresourceRange,
                                    0,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        vulkanhelpers::ImageBarrier(commandBuffer,
                                    mOffscreenImage.GetImage(),
                                    subresourceRange,
                                    VK_ACCESS_SHADER_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_READ_BIT,
                                    VK_IMAGE_LAYOUT_GENERAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        VkImageCopy copyRegion;
        copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.srcOffset = { 0, 0, 0 };
        copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.dstOffset = { 0, 0, 0 };
        copyRegion.extent = { mSettings.resolutionX, mSettings.resolutionY, 1 };
        vkCmdCopyImage(commandBuffer,
                       mOffscreenImage.GetImage(),
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       mSwapchainImages[i],
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1,
                       &copyRegion);

        vulkanhelpers::ImageBarrier(commandBuffer,
                                    mSwapchainImages[i], subresourceRange,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    0,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        error = vkEndCommandBuffer(commandBuffer);
        CHECK_VK_ERROR(error, "vkEndCommandBuffer");
    }
}


//
void VulkanApp::ProcessFrame(const float dt) {
    mFPSMeter.Update(dt);

    uint32_t imageIndex;
    VkResult error = vkAcquireNextImageKHR(mDevice, mSwapchain, UINT64_MAX, mSemaphoreImageAcquired, VK_NULL_HANDLE, &imageIndex);
    if (VK_SUCCESS != error) {
        return;
    }

    const VkFence fence = mWaitForFrameFences[imageIndex];
    error = vkWaitForFences(mDevice, 1, &fence, VK_TRUE, UINT64_MAX);
    if (VK_SUCCESS != error) {
        return;
    }
    vkResetFences(mDevice, 1, &fence);

    this->Update(imageIndex, dt);

    const VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &mSemaphoreImageAcquired;
    submitInfo.pWaitDstStageMask = &waitStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &mCommandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &mSemaphoreRenderFinished;

    error = vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, fence);
    if (VK_SUCCESS != error) {
        return;
    }

    VkPresentInfoKHR presentInfo;
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &mSemaphoreRenderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &mSwapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    error = vkQueuePresentKHR(mGraphicsQueue, &presentInfo);
    if (VK_SUCCESS != error) {
        return;
    }
}

void VulkanApp::FreeVulkan() {
    if (mSemaphoreRenderFinished) {
        vkDestroySemaphore(mDevice, mSemaphoreRenderFinished, nullptr);
        mSemaphoreRenderFinished = VK_NULL_HANDLE;
    }

    if (mSemaphoreImageAcquired) {
        vkDestroySemaphore(mDevice, mSemaphoreImageAcquired, nullptr);
        mSemaphoreImageAcquired = VK_NULL_HANDLE;
    }

    if (!mCommandBuffers.empty()) {
        vkFreeCommandBuffers(mDevice, mCommandPool, static_cast<uint32_t>(mCommandBuffers.size()), mCommandBuffers.data());
        mCommandBuffers.clear();
    }

    if (mCommandPool) {
        vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
        mCommandPool = VK_NULL_HANDLE;
    }

    for (VkFence& fence : mWaitForFrameFences) {
        vkDestroyFence(mDevice, fence, nullptr);
    }
    mWaitForFrameFences.clear();

    mOffscreenImage.Destroy();

    for (VkImageView& view : mSwapchainImageViews) {
        vkDestroyImageView(mDevice, view, nullptr);
    }
    mSwapchainImageViews.clear();
    mSwapchainImages.clear();

    if (mSwapchain) {
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }

    if (mSurface) {
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        mSurface = VK_NULL_HANDLE;
    }

    if (mDevice) {
        vkDestroyDevice(mDevice, nullptr);
        mDevice = VK_NULL_HANDLE;
    }

    if (mInstance) {
        vkDestroyInstance(mInstance, nullptr);
        mInstance = VK_NULL_HANDLE;
    }
}


// to be overriden by subclasses
void VulkanApp::InitSettings() {
}

void VulkanApp::InitApp() {
}

void VulkanApp::FreeResources() {
}

void VulkanApp::FillCommandBuffer(VkCommandBuffer, const size_t) {
}

void VulkanApp::OnMouseMove(const float, const float) {
}

void VulkanApp::OnMouseButton(const int, const int, const int) {
}

void VulkanApp::OnKey(const int, const int, const int, const int) {
}

void VulkanApp::Update(const size_t, const float) {
}
