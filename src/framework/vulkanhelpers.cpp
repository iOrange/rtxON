#include "vulkanhelpers.h"
#include <string>
#include <vector>
#include <fstream>
#include <cstring> // for memcpy


#define STB_IMAGE_IMPLEMENTATION
// excluding old and unusefull formats
#define STBI_NO_PSD
#define STBI_NO_GIF
#define STBI_NO_PIC
#define STBI_NO_PNM

#include "stb_image.h"


namespace vulkanhelpers {

void Initialize(VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue transferQueue) {
    __details::sPhysDevice = physicalDevice;
    __details::sDevice = device;
    __details::sCommandPool = commandPool;
    __details::sTransferQueue = transferQueue;

    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &__details::sPhysicalDeviceMemoryProperties);
}

uint32_t GetMemoryType(VkMemoryRequirements& memoryRequiriments, VkMemoryPropertyFlags memoryProperties) {
    uint32_t result = 0;
    for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < VK_MAX_MEMORY_TYPES; ++memoryTypeIndex) {
        if (memoryRequiriments.memoryTypeBits & (1 << memoryTypeIndex)) {
            if ((__details::sPhysicalDeviceMemoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & memoryProperties) == memoryProperties) {
                result = memoryTypeIndex;
                break;
            }
        }
    }
    return result;
}

void ImageBarrier(VkCommandBuffer commandBuffer,
                  VkImage image,
                  VkImageSubresourceRange& subresourceRange,
                  VkAccessFlags srcAccessMask,
                  VkAccessFlags dstAccessMask,
                  VkImageLayout oldLayout,
                  VkImageLayout newLayout) {

    VkImageMemoryBarrier imageMemoryBarrier;
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.pNext = nullptr;
    imageMemoryBarrier.srcAccessMask = srcAccessMask;
    imageMemoryBarrier.dstAccessMask = dstAccessMask;
    imageMemoryBarrier.oldLayout = oldLayout;
    imageMemoryBarrier.newLayout = newLayout;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange = subresourceRange;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 0, nullptr, 0, nullptr, 1,
                         &imageMemoryBarrier);
}



Buffer::Buffer()
    : mBuffer(VK_NULL_HANDLE)
    , mMemory(VK_NULL_HANDLE)
    , mSize(0)
{
}
Buffer::~Buffer() {
    this->Destroy();
}

VkResult Buffer::Create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperties) {
    VkResult result = VK_SUCCESS;

    VkBufferCreateInfo bufferCreateInfo;
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.pNext = nullptr;
    bufferCreateInfo.flags = 0;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = usage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.queueFamilyIndexCount = 0;
    bufferCreateInfo.pQueueFamilyIndices = nullptr;

    mSize = size;

    result = vkCreateBuffer(__details::sDevice, &bufferCreateInfo, nullptr, &mBuffer);
    if (VK_SUCCESS == result) {
        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(__details::sDevice, mBuffer, &memoryRequirements);

        VkMemoryAllocateInfo memoryAllocateInfo;
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.pNext = nullptr;
        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = GetMemoryType(memoryRequirements, memoryProperties);

        result = vkAllocateMemory(__details::sDevice, &memoryAllocateInfo, nullptr, &mMemory);
        if (VK_SUCCESS != result) {
            vkDestroyBuffer(__details::sDevice, mBuffer, nullptr);
            mBuffer = VK_NULL_HANDLE;
            mMemory = VK_NULL_HANDLE;
        } else {
            result = vkBindBufferMemory(__details::sDevice, mBuffer, mMemory, 0);
            if (VK_SUCCESS != result) {
                vkDestroyBuffer(__details::sDevice, mBuffer, nullptr);
                vkFreeMemory(__details::sDevice, mMemory, nullptr);
                mBuffer = VK_NULL_HANDLE;
                mMemory = VK_NULL_HANDLE;
            }
        }
    }

    return result;
}

void Buffer::Destroy() {
    if (mBuffer) {
        vkDestroyBuffer(__details::sDevice, mBuffer, nullptr);
        mBuffer = VK_NULL_HANDLE;
    }
    if (mMemory) {
        vkFreeMemory(__details::sDevice, mMemory, nullptr);
        mMemory = VK_NULL_HANDLE;
    }
}

void* Buffer::Map(VkDeviceSize size, VkDeviceSize offset) const {
    void* mem = nullptr;

    if (size > mSize) {
        size = mSize;
    }

    VkResult result = vkMapMemory(__details::sDevice, mMemory, offset, size, 0, &mem);
   if (VK_SUCCESS != result) {
        mem = nullptr;
    }

    return mem;
}
void Buffer::Unmap() const {
    vkUnmapMemory(__details::sDevice, mMemory);
}

bool Buffer::UploadData(const void* data, VkDeviceSize size, VkDeviceSize offset) const {
    bool result = false;

    void* mem = this->Map(size, offset);
    if (mem) {
        std::memcpy(mem, data, size);
        this->Unmap();
    }
    return true;
}

// getters
VkBuffer Buffer::GetBuffer() const {
    return mBuffer;
}

VkDeviceSize Buffer::GetSize() const {
    return mSize;
}



Image::Image()
    : mFormat(VK_FORMAT_B8G8R8A8_UNORM)
    , mImage(VK_NULL_HANDLE)
    , mMemory(VK_NULL_HANDLE)
    , mImageView(VK_NULL_HANDLE)
    , mSampler(VK_NULL_HANDLE)
{

}
Image::~Image() {
    this->Destroy();
}

VkResult Image::Create(VkImageType imageType,
                       VkFormat format,
                       VkExtent3D extent,
                       VkImageTiling tiling,
                       VkImageUsageFlags usage,
                       VkMemoryPropertyFlags memoryProperties) {
    VkResult result = VK_SUCCESS;

    mFormat = format;

    VkImageCreateInfo imageCreateInfo;
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = nullptr;
    imageCreateInfo.flags = 0;
    imageCreateInfo.imageType = imageType;
    imageCreateInfo.format = format;
    imageCreateInfo.extent = extent;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = tiling;
    imageCreateInfo.usage = usage;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = 0;
    imageCreateInfo.pQueueFamilyIndices = nullptr;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    result = vkCreateImage(__details::sDevice, &imageCreateInfo, nullptr, &mImage);
    if (VK_SUCCESS == result) {
        VkMemoryRequirements memoryRequirements;
        vkGetImageMemoryRequirements(__details::sDevice, mImage, &memoryRequirements);

        VkMemoryAllocateInfo memoryAllocateInfo;
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.pNext = nullptr;
        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = GetMemoryType(memoryRequirements, memoryProperties);

        result = vkAllocateMemory(__details::sDevice, &memoryAllocateInfo, nullptr, &mMemory);
        if (VK_SUCCESS != result) {
            vkDestroyImage(__details::sDevice, mImage, nullptr);
            mImage = VK_NULL_HANDLE;
            mMemory = VK_NULL_HANDLE;
        } else {
            result = vkBindImageMemory(__details::sDevice, mImage, mMemory, 0);
            if (VK_SUCCESS != result) {
                vkDestroyImage(__details::sDevice, mImage, nullptr);
                vkFreeMemory(__details::sDevice, mMemory, nullptr);
                mImage = VK_NULL_HANDLE;
                mMemory = VK_NULL_HANDLE;
            }
        }
    }

    return result;
}

void Image::Destroy() {
    if (mSampler) {
        vkDestroySampler(__details::sDevice, mSampler, nullptr);
        mSampler = VK_NULL_HANDLE;
    }
    if (mImageView) {
        vkDestroyImageView(__details::sDevice, mImageView, nullptr);
        mImageView = VK_NULL_HANDLE;
    }
    if (mMemory) {
        vkFreeMemory(__details::sDevice, mMemory, nullptr);
        mMemory = VK_NULL_HANDLE;
    }
    if (mImage) {
        vkDestroyImage(__details::sDevice, mImage, nullptr);
        mImage = VK_NULL_HANDLE;
    }
}

bool Image::Load(const char* fileName) {
    int width, height, channels;
    bool textureHDR = false;
    stbi_uc* imageData = nullptr;

    std::string fileNameString(fileName);
    const std::string extension = fileNameString.substr(fileNameString.length() - 3);

    if (extension == "hdr") {
        textureHDR = true;
        imageData = reinterpret_cast<stbi_uc*>(stbi_loadf(fileName, &width, &height, &channels, STBI_rgb_alpha));
    } else {
        imageData = stbi_load(fileName, &width, &height, &channels, STBI_rgb_alpha);
    }

    if (imageData) {
        const int bpp = textureHDR ? sizeof(float[4]) : sizeof(uint8_t[4]);
        VkDeviceSize imageSize = static_cast<VkDeviceSize>(width * height * bpp);

        Buffer stagingBuffer;
        VkResult error = stagingBuffer.Create(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (VK_SUCCESS == error && stagingBuffer.UploadData(imageData, imageSize)) {
            stbi_image_free(imageData);

            VkExtent3D imageExtent {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
                1
            };

            const VkFormat fmt = textureHDR ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_SRGB;

            error = this->Create(VK_IMAGE_TYPE_2D, fmt, imageExtent, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            if (VK_SUCCESS != error) {
                return false;
            }

            VkCommandBufferAllocateInfo allocInfo;
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.pNext = nullptr;
            allocInfo.commandPool = __details::sCommandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;

            VkCommandBuffer commandBuffer;
            error = vkAllocateCommandBuffers(__details::sDevice, &allocInfo, &commandBuffer);
            if (VK_SUCCESS != error) {
                return false;
            }

            VkCommandBufferBeginInfo beginInfo;
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.pNext = nullptr;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            beginInfo.pInheritanceInfo = nullptr;

            error = vkBeginCommandBuffer(commandBuffer, &beginInfo);
            if (VK_SUCCESS != error) {
                vkFreeCommandBuffers(__details::sDevice, __details::sCommandPool, 1, &commandBuffer);
                return false;
            }

            VkImageMemoryBarrier barrier;
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;;
            barrier.pNext = nullptr;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = mImage;
            barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            VkBufferImageCopy region;
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            region.imageOffset = { 0, 0, 0 };
            region.imageExtent = imageExtent;

            vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.GetBuffer(), mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            error = vkEndCommandBuffer(commandBuffer);
            if (VK_SUCCESS != error) {
                vkFreeCommandBuffers(__details::sDevice, __details::sCommandPool, 1, &commandBuffer);
                return false;
            }

            VkSubmitInfo submitInfo;
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.pNext = nullptr;
            submitInfo.waitSemaphoreCount = 0;
            submitInfo.pWaitSemaphores = nullptr;
            submitInfo.pWaitDstStageMask = nullptr;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            submitInfo.signalSemaphoreCount = 0;
            submitInfo.pSignalSemaphores = nullptr;

            error = vkQueueSubmit(__details::sTransferQueue, 1, &submitInfo, VK_NULL_HANDLE);
            if (VK_SUCCESS != error) {
                vkFreeCommandBuffers(__details::sDevice, __details::sCommandPool, 1, &commandBuffer);
                return false;
            }

            error = vkQueueWaitIdle(__details::sTransferQueue);
            if (VK_SUCCESS != error) {
                vkFreeCommandBuffers(__details::sDevice, __details::sCommandPool, 1, &commandBuffer);
                return false;
            }

            vkFreeCommandBuffers(__details::sDevice, __details::sCommandPool, 1, &commandBuffer);
        } else {
            stbi_image_free(imageData);
        }
    }

    return true;
}

VkResult Image::CreateImageView(VkImageViewType viewType, VkFormat format, VkImageSubresourceRange subresourceRange) {
    VkImageViewCreateInfo imageViewCreateInfo;
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.pNext = nullptr;
    imageViewCreateInfo.viewType = viewType;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.subresourceRange = subresourceRange;
    imageViewCreateInfo.image = mImage;
    imageViewCreateInfo.flags = 0;
    imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

    return vkCreateImageView(__details::sDevice, &imageViewCreateInfo, nullptr, &mImageView);
}

VkResult Image::CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode) {
    VkSamplerCreateInfo samplerCreateInfo;
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.pNext = nullptr;
    samplerCreateInfo.flags = 0;
    samplerCreateInfo.magFilter = magFilter;
    samplerCreateInfo.minFilter = minFilter;
    samplerCreateInfo.mipmapMode = mipmapMode;
    samplerCreateInfo.addressModeU = addressMode;
    samplerCreateInfo.addressModeV = addressMode;
    samplerCreateInfo.addressModeW = addressMode;
    samplerCreateInfo.mipLodBias = 0;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    samplerCreateInfo.maxAnisotropy = 1;
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCreateInfo.minLod = 0;
    samplerCreateInfo.maxLod = 0;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

    return vkCreateSampler(__details::sDevice, &samplerCreateInfo, nullptr, &mSampler);
}

// getters
VkFormat Image::GetFormat() const {
    return mFormat;
}

VkImage Image::GetImage() const {
    return mImage;
}

VkImageView Image::GetImageView() const {
    return mImageView;
}

VkSampler Image::GetSampler() const {
    return mSampler;
}



Shader::Shader()
    : mModule(VK_NULL_HANDLE)
{
}
Shader::~Shader() {
    this->Destroy();
}

bool Shader::LoadFromFile(const char* fileName) {
    bool result = false;

    std::ifstream file(fileName, std::ios::in | std::ios::binary);
    if (file) {
        file.seekg(0, std::ios::end);
        const size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> bytecode(fileSize);
        bytecode.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        VkShaderModuleCreateInfo shaderModuleCreateInfo;
        shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCreateInfo.pNext = nullptr;
        shaderModuleCreateInfo.codeSize = bytecode.size();
        shaderModuleCreateInfo.pCode = reinterpret_cast<uint32_t*>(bytecode.data());
        shaderModuleCreateInfo.flags = 0;

        const VkResult error = vkCreateShaderModule(__details::sDevice, &shaderModuleCreateInfo, nullptr, &mModule);
        result = (VK_SUCCESS == error);
    }

    return result;
}

void Shader::Destroy() {
    if (mModule) {
        vkDestroyShaderModule(__details::sDevice, mModule, nullptr);
        mModule = VK_NULL_HANDLE;
    }
}

VkPipelineShaderStageCreateInfo Shader::GetShaderStage(VkShaderStageFlagBits stage) {
    return VkPipelineShaderStageCreateInfo {
        /*sType*/ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        /*pNext*/ nullptr,
        /*flags*/ 0,
        /*stage*/ stage,
        /*module*/ mModule,
        /*pName*/ "main",
        /*pSpecializationInfo*/ nullptr
    };
}

} // namespace vulkanhelpers
