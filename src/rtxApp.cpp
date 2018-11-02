#include "rtxApp.h"

static const String sShadersFolder = "_data/shaders/";


struct VkGeometryInstance {
    float transform[12];
    uint32_t instanceId : 24;
    uint32_t mask : 8;
    uint32_t instanceOffset : 24;
    uint32_t flags : 8;
    uint64_t accelerationStructureHandle;
};


RtxApp::RtxApp()
    : VulkanApp()
{
}
RtxApp::~RtxApp() {

}


void RtxApp::InitSettings() {
    mSettings.name = "rtxON";
    mSettings.enableValidation = true;
    mSettings.supportRaytracing = true;
}

void RtxApp::InitApp() {
    this->CreateScene();
    this->CreateRaytracingPipeline();
    this->CreateShaderBindingTable();
    this->CreateDescriptorSet();
}

void RtxApp::FreeResources() {
    for (RTAccelerationStructure& as : mScene.bottomLevelAS) {
        if (as.accelerationStructure) {
            vkDestroyAccelerationStructureNVX(mDevice, as.accelerationStructure, nullptr);
        }
        if (as.memory) {
            vkFreeMemory(mDevice, as.memory, nullptr);
        }
    }
    mScene.bottomLevelAS.clear();

    if (mScene.topLevelAS.accelerationStructure) {
        vkDestroyAccelerationStructureNVX(mDevice, mScene.topLevelAS.accelerationStructure, nullptr);
        mScene.topLevelAS.accelerationStructure = VK_NULL_HANDLE;
    }
    if (mScene.topLevelAS.memory) {
        vkFreeMemory(mDevice, mScene.topLevelAS.memory, nullptr);
        mScene.topLevelAS.memory = VK_NULL_HANDLE;
    }

    if (mRTDescriptorPool) {
        vkDestroyDescriptorPool(mDevice, mRTDescriptorPool, nullptr);
        mRTDescriptorPool = VK_NULL_HANDLE;
    }

    mShaderBindingTable.Destroy();

    if (mRTPipeline) {
        vkDestroyPipeline(mDevice, mRTPipeline, nullptr);
        mRTPipeline = VK_NULL_HANDLE;
    }

    if (mRTPipelineLayout) {
        vkDestroyPipelineLayout(mDevice, mRTPipelineLayout, nullptr);
        mRTPipelineLayout = VK_NULL_HANDLE;
    }

    if (mRTDescriptorSetLayout) {
        vkDestroyDescriptorSetLayout(mDevice, mRTDescriptorSetLayout, nullptr);
        mRTDescriptorSetLayout = VK_NULL_HANDLE;
    }
}

void RtxApp::FillCommandBuffer(VkCommandBuffer commandBuffer, const size_t imageIndex) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAYTRACING_NVX, mRTPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAYTRACING_NVX, mRTPipelineLayout, 0, 1, &mRTDescriptorSet, 0, 0);

    // our shader binding table layout:
    // |[ raygen ]|[closest hit]|[miss]|
    // | 0        | 1           | 2    |

    VkBuffer sbtBuffer = mShaderBindingTable.GetBuffer();
    vkCmdTraceRaysNVX(commandBuffer,
                      sbtBuffer, 0,
                      sbtBuffer, 2 * mRTProps.shaderHeaderSize, mRTProps.shaderHeaderSize,
                      sbtBuffer, 1 * mRTProps.shaderHeaderSize, mRTProps.shaderHeaderSize,
                      mSettings.resolutionX, mSettings.resolutionY);
}

bool RtxApp::CreateAS(const VkAccelerationStructureTypeNVX type,
                      const uint32_t geometryCount,
                      const VkGeometryNVX* geometries,
                      const uint32_t instanceCount,
                      RTAccelerationStructure& _as) {

    VkAccelerationStructureCreateInfoNVX accelerationStructureInfo;
    accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NVX;
    accelerationStructureInfo.pNext = nullptr;
    accelerationStructureInfo.type = type;
    accelerationStructureInfo.flags = 0;
    accelerationStructureInfo.compactedSize = 0;
    accelerationStructureInfo.instanceCount = instanceCount;
    accelerationStructureInfo.geometryCount = geometryCount;
    accelerationStructureInfo.pGeometries = geometries;

    VkResult error = vkCreateAccelerationStructureNVX(mDevice, &accelerationStructureInfo, nullptr, &_as.accelerationStructure);
    if (VK_SUCCESS != error) {
        CHECK_VK_ERROR(error, "vkCreateAccelerationStructureNV");
        return false;
    }

    VkAccelerationStructureMemoryRequirementsInfoNVX memoryRequirementsInfo;
    memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NVX;
    memoryRequirementsInfo.pNext = nullptr;
    memoryRequirementsInfo.accelerationStructure = _as.accelerationStructure;

    VkMemoryRequirements2 memoryRequirements;
    vkGetAccelerationStructureMemoryRequirementsNVX(mDevice, &memoryRequirementsInfo, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo;
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = nullptr;
    memoryAllocateInfo.allocationSize = memoryRequirements.memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = vulkanhelpers::GetMemoryType(memoryRequirements.memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    error = vkAllocateMemory(mDevice, &memoryAllocateInfo, nullptr, &_as.memory);
    if (VK_SUCCESS != error) {
        CHECK_VK_ERROR(error, "vkAllocateMemory for AS");
        return false;
    }

    VkBindAccelerationStructureMemoryInfoNVX bindInfo;
    bindInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NVX;
    bindInfo.pNext = nullptr;
    bindInfo.accelerationStructure = _as.accelerationStructure;
    bindInfo.memory = _as.memory;
    bindInfo.memoryOffset = 0;
    bindInfo.deviceIndexCount = 0;
    bindInfo.pDeviceIndices = nullptr;

    error = vkBindAccelerationStructureMemoryNVX(mDevice, 1, &bindInfo);
    if (VK_SUCCESS != error) {
        CHECK_VK_ERROR(error, "vkBindAccelerationStructureMemoryNVX");
        return false;
    }

    error = vkGetAccelerationStructureHandleNVX(mDevice, _as.accelerationStructure, sizeof(uint64_t), &_as.handle);
    if (VK_SUCCESS != error) {
        CHECK_VK_ERROR(error, "vkGetAccelerationStructureHandleNVX");
        return false;
    }

    return true;
}

void RtxApp::CreateScene() {
    vulkanhelpers::Buffer vb, ib;

    const float vertices[9] = {
        0.25f, 0.25f, 0.0f,
        0.75f, 0.25f, 0.0f,
        0.50f, 0.75f, 0.0f
    };

    const uint32_t indices[3] = { 0, 1, 2 };

    VkResult error = vb.Create(sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    CHECK_VK_ERROR(error, "vb.Create");

    if (!vb.UploadData(vertices, vb.GetSize())) {
        assert(false && "Failed to upload vertex buffer");
    }

    error = ib.Create(sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    CHECK_VK_ERROR(error, "ib.Create");

    if (!ib.UploadData(indices, ib.GetSize())) {
        assert(false && "Failed to upload index buffer");
    }

    VkGeometryNVX geometry;
    geometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NVX;
    geometry.pNext = nullptr;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NVX;
    geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NVX;
    geometry.geometry.triangles.pNext = nullptr;
    geometry.geometry.triangles.vertexData = vb.GetBuffer();
    geometry.geometry.triangles.vertexOffset = 0;
    geometry.geometry.triangles.vertexCount = 3;
    geometry.geometry.triangles.vertexStride = sizeof(vec3);
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.indexData = ib.GetBuffer();
    geometry.geometry.triangles.indexOffset = 0;
    geometry.geometry.triangles.indexCount = 3;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geometry.geometry.triangles.transformData = VK_NULL_HANDLE;
    geometry.geometry.triangles.transformOffset = 0;
    geometry.geometry.aabbs = { };
    geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NVX;
    geometry.flags = 0;


    // here we create our bottom-level acceleration structure for our happy triangle
    mScene.bottomLevelAS.resize(1);
    this->CreateAS(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NVX, 1, &geometry, 0, mScene.bottomLevelAS[0]);


    // create an instance for our triangle
    vulkanhelpers::Buffer instancesBuffer;

    const float transform[12] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
    };

    VkGeometryInstance instance;
    std::memcpy(instance.transform, transform, sizeof(transform));
    instance.instanceId = 0;
    instance.mask = 0xff;
    instance.instanceOffset = 0;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NVX;
    instance.accelerationStructureHandle = mScene.bottomLevelAS[0].handle;

    error = instancesBuffer.Create(sizeof(instance), VK_BUFFER_USAGE_RAYTRACING_BIT_NVX, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    CHECK_VK_ERROR(error, "instancesBuffer.Create");

    if (!instancesBuffer.UploadData(&instance, instancesBuffer.GetSize())) {
        assert(false && "Failed to upload instances buffer");
    }


    // and here we create out top-level acceleration structure that'll represent our scene
    this->CreateAS(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX, 0, nullptr, 1, mScene.topLevelAS);

    // now we have to build them

    VkAccelerationStructureMemoryRequirementsInfoNVX memoryRequirementsInfo;
    memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NVX;
    memoryRequirementsInfo.pNext = nullptr;
    memoryRequirementsInfo.accelerationStructure = mScene.bottomLevelAS[0].accelerationStructure;

    VkMemoryRequirements2 memReqBottomAS, memReqTopAS;
    vkGetAccelerationStructureScratchMemoryRequirementsNVX(mDevice, &memoryRequirementsInfo, &memReqBottomAS);

    memoryRequirementsInfo.accelerationStructure = mScene.topLevelAS.accelerationStructure;
    vkGetAccelerationStructureScratchMemoryRequirementsNVX(mDevice, &memoryRequirementsInfo, &memReqTopAS);

    const VkDeviceSize scratchBufferSize = Max(memReqBottomAS.memoryRequirements.size, memReqTopAS.memoryRequirements.size);

    vulkanhelpers::Buffer scratchBuffer;
    error = scratchBuffer.Create(scratchBufferSize, VK_BUFFER_USAGE_RAYTRACING_BIT_NVX, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CHECK_VK_ERROR(error, "scratchBuffer.Create");

    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = nullptr;
    commandBufferAllocateInfo.commandPool = mCommandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    error = vkAllocateCommandBuffers(mDevice, &commandBufferAllocateInfo, &commandBuffer);
    CHECK_VK_ERROR(error, "vkAllocateCommandBuffers");

    VkCommandBufferBeginInfo beginInfo;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkMemoryBarrier memoryBarrier;
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.pNext = nullptr;
    memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;

    // build bottom-level AS
    vkCmdBuildAccelerationStructureNVX(commandBuffer,
                                       VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NVX,
                                       0, VK_NULL_HANDLE, 0,
                                       1, &geometry,
                                       0, VK_FALSE,
                                       mScene.bottomLevelAS[0].accelerationStructure, VK_NULL_HANDLE,
                                       scratchBuffer.GetBuffer(), 0);

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, 0, 1, &memoryBarrier, 0, 0, 0, 0);

    // build top-level AS
    vkCmdBuildAccelerationStructureNVX(commandBuffer,
                                       VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX,
                                       1, instancesBuffer.GetBuffer(), 0,
                                       0, nullptr,
                                       0, VK_FALSE,
                                       mScene.topLevelAS.accelerationStructure, VK_NULL_HANDLE,
                                       scratchBuffer.GetBuffer(), 0);

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, 0, 1, &memoryBarrier, 0, 0, 0, 0);

    vkEndCommandBuffer(commandBuffer);

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

    vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(mGraphicsQueue);
    vkFreeCommandBuffers(mDevice, mCommandPool, 1, &commandBuffer);
}

void RtxApp::CreateRaytracingPipeline() {
    VkDescriptorSetLayoutBinding accelerationStructureLayoutBinding;
    accelerationStructureLayoutBinding.binding = 0;
    accelerationStructureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX;
    accelerationStructureLayoutBinding.descriptorCount = 1;
    accelerationStructureLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
    accelerationStructureLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding resultImageLayoutBinding;
    resultImageLayoutBinding.binding = 1;
    resultImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    resultImageLayoutBinding.descriptorCount = 1;
    resultImageLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
    resultImageLayoutBinding.pImmutableSamplers = nullptr;

    std::vector<VkDescriptorSetLayoutBinding> bindings({
        accelerationStructureLayoutBinding,
        resultImageLayoutBinding
    });

    VkDescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.flags = 0;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkResult error = vkCreateDescriptorSetLayout(mDevice, &layoutInfo, nullptr, &mRTDescriptorSetLayout);
    CHECK_VK_ERROR(error, "vkCreateDescriptorSetLayout");

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = nullptr;
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &mRTDescriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    error = vkCreatePipelineLayout(mDevice, &pipelineLayoutCreateInfo, nullptr, &mRTPipelineLayout);
    CHECK_VK_ERROR(error, "vkCreatePipelineLayout");


    vulkanhelpers::Shader rayGenShader, rayChitShader, rayMissShader;
    rayGenShader.LoadFromFile((sShadersFolder + "ray_gen.bin").c_str());
    rayChitShader.LoadFromFile((sShadersFolder + "ray_chit.bin").c_str());
    rayMissShader.LoadFromFile((sShadersFolder + "ray_miss.bin").c_str());

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages({
        rayGenShader.GetShaderStage(VK_SHADER_STAGE_RAYGEN_BIT_NVX),
        rayChitShader.GetShaderStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX),
        rayMissShader.GetShaderStage(VK_SHADER_STAGE_MISS_BIT_NVX)
    });

    // here are our groups map:
    // group 0 : raygen
    // group 1 : closest hit
    // group 2 : miss
    const uint32_t groupNumbers[] = { 0, 1, 2 };

    VkRaytracingPipelineCreateInfoNVX rayPipelineInfo;
    rayPipelineInfo.sType = VK_STRUCTURE_TYPE_RAYTRACING_PIPELINE_CREATE_INFO_NVX;
    rayPipelineInfo.pNext = nullptr;
    rayPipelineInfo.flags = 0;
    rayPipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    rayPipelineInfo.pStages = shaderStages.data();
    rayPipelineInfo.pGroupNumbers = groupNumbers;
    rayPipelineInfo.maxRecursionDepth = 1;
    rayPipelineInfo.layout = mRTPipelineLayout;
    rayPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    rayPipelineInfo.basePipelineIndex = 0;

    error = vkCreateRaytracingPipelinesNVX(mDevice, nullptr, 1, &rayPipelineInfo, nullptr, &mRTPipeline);
    CHECK_VK_ERROR(error, "vkCreateRaytracingPipelinesNVX");
}

void RtxApp::CreateShaderBindingTable() {
    const uint32_t numGroups = 3; // !! should be same amount of groups we used to create mRTPipeline
    const uint32_t shaderBindingTableSize = mRTProps.shaderHeaderSize * numGroups;

    VkResult error = mShaderBindingTable.Create(shaderBindingTableSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    CHECK_VK_ERROR(error, "mShaderBindingTable.Create");

    void* mem = mShaderBindingTable.Map(shaderBindingTableSize);
    error = vkGetRaytracingShaderHandlesNVX(mDevice, mRTPipeline, 0, numGroups, shaderBindingTableSize, mem);
    CHECK_VK_ERROR(error, L"vkGetRaytracingShaderHandleNV");
    mShaderBindingTable.Unmap();
}

void RtxApp::CreateDescriptorSet() {
    std::vector<VkDescriptorPoolSize> poolSizes({
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    });

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.pNext = nullptr;
    descriptorPoolCreateInfo.flags = 0;
    descriptorPoolCreateInfo.maxSets = 1;
    descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();

    VkResult error = vkCreateDescriptorPool(mDevice, &descriptorPoolCreateInfo, nullptr, &mRTDescriptorPool);
    CHECK_VK_ERROR(error, "vkCreateDescriptorPool");

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.pNext = nullptr;
    descriptorSetAllocateInfo.descriptorPool = mRTDescriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &mRTDescriptorSetLayout;

    error = vkAllocateDescriptorSets(mDevice, &descriptorSetAllocateInfo, &mRTDescriptorSet);
    CHECK_VK_ERROR(error, "vkAllocateDescriptorSets");


    VkDescriptorAccelerationStructureInfoNVX descriptorAccelerationStructureInfo;
    descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ACCELERATION_STRUCTURE_INFO_NVX;
    descriptorAccelerationStructureInfo.pNext = nullptr;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    descriptorAccelerationStructureInfo.pAccelerationStructures = &mScene.topLevelAS.accelerationStructure;

    VkWriteDescriptorSet accelerationStructureWrite;
    accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo; // Notice that pNext is assigned here!
    accelerationStructureWrite.dstSet = mRTDescriptorSet;
    accelerationStructureWrite.dstBinding = 0;
    accelerationStructureWrite.dstArrayElement = 0;
    accelerationStructureWrite.descriptorCount = 1;
    accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX;
    accelerationStructureWrite.pImageInfo = nullptr;
    accelerationStructureWrite.pBufferInfo = nullptr;
    accelerationStructureWrite.pTexelBufferView = nullptr;


    VkDescriptorImageInfo descriptorOutputImageInfo;
    descriptorOutputImageInfo.sampler = nullptr;
    descriptorOutputImageInfo.imageView = mOffscreenImage.GetImageView();
    descriptorOutputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet resultImageWrite;
    resultImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    resultImageWrite.pNext = nullptr;
    resultImageWrite.dstSet = mRTDescriptorSet;
    resultImageWrite.dstBinding = 1;
    resultImageWrite.dstArrayElement = 0;
    resultImageWrite.descriptorCount = 1;
    resultImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    resultImageWrite.pImageInfo = &descriptorOutputImageInfo;
    resultImageWrite.pBufferInfo = nullptr;
    resultImageWrite.pTexelBufferView = nullptr;


    Array<VkWriteDescriptorSet> descriptorWrites({
        accelerationStructureWrite,
        resultImageWrite
    });

    vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, VK_NULL_HANDLE);
}
