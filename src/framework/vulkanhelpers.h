#pragma once
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.h"
#include "volk.h"

#include <cassert>

#define CHECK_VK_ERROR(_error, _message) do {   \
    if (VK_SUCCESS != error) {                  \
        assert(false && _message);              \
    }                                           \
} while (false);


namespace vulkanhelpers {

    namespace __details {
        static VkPhysicalDevice                 sPhysDevice;
        static VkDevice                         sDevice;
        static VkCommandPool                    sCommandPool;
        static VkQueue                          sTransferQueue;
        static VkPhysicalDeviceMemoryProperties sPhysicalDeviceMemoryProperties;
    } // namespace __details

    void     Initialize(VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue transferQueue);
    uint32_t GetMemoryType(VkMemoryRequirements& memoryRequiriments, VkMemoryPropertyFlags memoryProperties);
    void     ImageBarrier(VkCommandBuffer commandBuffer,
                          VkImage image,
                          VkImageSubresourceRange& subresourceRange,
                          VkAccessFlags srcAccessMask,
                          VkAccessFlags dstAccessMask,
                          VkImageLayout oldLayout,
                          VkImageLayout newLayout);


    class Buffer {
    public:
        Buffer();
        ~Buffer();

        VkResult        Create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperties);
        void            Destroy();

        void*           Map(VkDeviceSize size = UINT64_MAX, VkDeviceSize offset = 0) const;
        void            Unmap() const;

        bool            UploadData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0) const;

        // getters
        VkBuffer        GetBuffer() const;
        VkDeviceSize    GetSize() const;

    private:
        VkBuffer        mBuffer;
        VkDeviceMemory  mMemory;
        VkDeviceSize    mSize;
    };


    class Image {
    public:
        Image();
        ~Image();

        VkResult    Create(VkImageType imageType,
                           VkFormat format,
                           VkExtent3D extent,
                           VkImageTiling tiling,
                           VkImageUsageFlags usage,
                           VkMemoryPropertyFlags memoryProperties);

        void        Destroy();
        bool        Load(const char* fileName);
        VkResult    CreateImageView(VkImageViewType viewType, VkFormat format, VkImageSubresourceRange subresourceRange);
        VkResult    CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode);

        // getters
        VkFormat    GetFormat() const;
        VkImage     GetImage() const;
        VkImageView GetImageView() const;
        VkSampler   GetSampler() const;

    private:
        VkFormat        mFormat;
        VkImage         mImage;
        VkDeviceMemory  mMemory;
        VkImageView     mImageView;
        VkSampler       mSampler;
    };


    class Shader {
    public:
        Shader();
        ~Shader();

        bool    LoadFromFile(const char* fileName);
        void    Destroy();

        VkPipelineShaderStageCreateInfo GetShaderStage(VkShaderStageFlagBits stage);

    private:
        VkShaderModule  mModule;
    };

} // namespace vulkanhelpers
