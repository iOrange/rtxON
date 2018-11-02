#pragma once

#include "framework/vulkanapp.h"

struct RTAccelerationStructure {
    VkDeviceMemory              memory;
    VkAccelerationStructureNVX  accelerationStructure;
    uint64_t                    handle;
};

struct RTScene {
    Array<RTAccelerationStructure>  bottomLevelAS;
    RTAccelerationStructure         topLevelAS;
};

class RtxApp : public VulkanApp {
public:
    RtxApp();
    ~RtxApp();

protected:
    virtual void InitSettings() override;
    virtual void InitApp() override;
    virtual void FreeResources() override;
    virtual void FillCommandBuffer(VkCommandBuffer commandBuffer, const size_t imageIndex) override;

private:
    bool CreateAS(const VkAccelerationStructureTypeNVX type,
                  const uint32_t geometryCount,
                  const VkGeometryNVX* geometries,
                  const uint32_t instanceCount,
                  RTAccelerationStructure& _as);
    void CreateScene();
    void CreateRaytracingPipeline();
    void CreateShaderBindingTable();
    void CreateDescriptorSet();

private:
    VkDescriptorSetLayout   mRTDescriptorSetLayout;
    VkPipelineLayout        mRTPipelineLayout;
    VkPipeline              mRTPipeline;
    VkDescriptorPool        mRTDescriptorPool;
    VkDescriptorSet         mRTDescriptorSet;

    vulkanhelpers::Buffer   mShaderBindingTable;

    RTScene                 mScene;
};
