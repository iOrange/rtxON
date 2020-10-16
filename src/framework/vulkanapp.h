#include "vulkanhelpers.h"

#include "GLFW/glfw3.h"

#include "common.h"

struct AppSettings {
    std::string name;
    uint32_t    resolutionX;
    uint32_t    resolutionY;
    VkFormat    surfaceFormat;
    bool        enableValidation;
    bool        enableVSync;
    bool        supportRaytracing;
    bool        supportDescriptorIndexing;
};

struct FPSMeter {
    static const size_t kFPSHistorySize = 128;

    float   fpsHistory[kFPSHistorySize] = { 0.0f };
    size_t  historyPointer = 0;
    float   fpsAccumulator = 0.0f;
    float   fps = 0.0f;

    void    Update(const float dt);
    float   GetFPS() const;
    float   GetFrameTime() const;
};

class VulkanApp {
public:
    VulkanApp();
    virtual ~VulkanApp();

    void    Run();

protected:
    bool    Initialize();
    void    Loop();
    void    Shutdown();

    void    InitializeSettings();
    bool    InitializeVulkan();
    bool    InitializeDevicesAndQueues();
    bool    InitializeSurface();
    bool    InitializeSwapchain();
    bool    InitializeFencesAndCommandPool();
    bool    InitializeOffscreenImage();
    bool    InitializeCommandBuffers();
    bool    InitializeSynchronization();
    void    FillCommandBuffers();

    //
    void    ProcessFrame(const float dt);
    void    FreeVulkan();

    // to be overriden by subclasses
    virtual void InitSettings();
    virtual void InitApp();
    virtual void FreeResources();
    virtual void FillCommandBuffer(VkCommandBuffer commandBuffer, const size_t imageIndex);

    virtual void OnMouseMove(const float x, const float y);
    virtual void OnMouseButton(const int button, const int action, const int mods);
    virtual void OnKey(const int key, const int scancode, const int action, const int mods);
    virtual void Update(const size_t imageIndex, const float dt);

protected:
    AppSettings             mSettings;
    GLFWwindow*             mWindow;

    VkInstance              mInstance;
    VkPhysicalDevice        mPhysicalDevice;
    VkDevice                mDevice;
    VkSurfaceFormatKHR      mSurfaceFormat;
    VkSurfaceKHR            mSurface;
    VkSwapchainKHR          mSwapchain;
    Array<VkImage>          mSwapchainImages;
    Array<VkImageView>      mSwapchainImageViews;
    Array<VkFence>          mWaitForFrameFences;
    VkCommandPool           mCommandPool;
    vulkanhelpers::Image    mOffscreenImage;
    Array<VkCommandBuffer>  mCommandBuffers;
    VkSemaphore             mSemaphoreImageAcquired;
    VkSemaphore             mSemaphoreRenderFinished;

    uint32_t                mGraphicsQueueFamilyIndex;
    uint32_t                mComputeQueueFamilyIndex;
    uint32_t                mTransferQueueFamilyIndex;
    VkQueue                 mGraphicsQueue;
    VkQueue                 mComputeQueue;
    VkQueue                 mTransferQueue;

    // RTX stuff
    VkPhysicalDeviceRayTracingPropertiesKHR mRTProps;

    // FPS meter
    FPSMeter                mFPSMeter;
};
