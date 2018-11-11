#pragma once

#include "framework/vulkanapp.h"
#include "framework/camera.h"

struct RTAccelerationStructure {
    VkDeviceMemory              memory;
    VkAccelerationStructureNVX  accelerationStructure;
    uint64_t                    handle;
};

struct RTMesh {
    uint32_t                    numVertices;
    uint32_t                    numFaces;

    vulkanhelpers::Buffer       positions;
    vulkanhelpers::Buffer       attribs;
    vulkanhelpers::Buffer       indices;
    vulkanhelpers::Buffer       faces;
    vulkanhelpers::Buffer       matIDs;

    RTAccelerationStructure     blas;
};

struct RTMaterial {
    vulkanhelpers::Image        texture;
};

struct RTScene {
    Array<RTMesh>               meshes;
    Array<RTMaterial>           materials;
    RTAccelerationStructure     topLevelAS;

    // shader resources stuff
    Array<VkDescriptorBufferInfo>   matIDsBufferInfos;
    Array<VkDescriptorBufferInfo>   attribsBufferInfos;
    Array<VkDescriptorBufferInfo>   facesBufferInfos;
    Array<VkDescriptorImageInfo>    texturesInfos;
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

    virtual void OnMouseMove(const float x, const float y) override;
    virtual void OnMouseButton(const int button, const int action, const int mods) override;
    virtual void OnKey(const int key, const int scancode, const int action, const int mods) override;
    virtual void Update(const size_t imageIndex, const float dt) override;

private:
    bool CreateAS(const VkAccelerationStructureTypeNVX type,
                  const uint32_t geometryCount,
                  const VkGeometryNVX* geometries,
                  const uint32_t instanceCount,
                  RTAccelerationStructure& _as);
    void LoadSceneGeometry();
    void CreateScene();
    void CreateCamera();
    void UpdateCameraParams(struct UniformParams* params, const float dt);
    void CreateDescriptorSetsLayouts();
    void CreateRaytracingPipeline();
    void CreateShaderBindingTable();
    void UpdateDescriptorSets();

private:
    Array<VkDescriptorSetLayout>    mRTDescriptorSetsLayouts;
    VkPipelineLayout                mRTPipelineLayout;
    VkPipeline                      mRTPipeline;
    VkDescriptorPool                mRTDescriptorPool;
    Array<VkDescriptorSet>          mRTDescriptorSets;

    vulkanhelpers::Buffer           mShaderBindingTable;

    RTScene                         mScene;

    // camera a& user input
    Camera                          mCamera;
    vulkanhelpers::Buffer           mCameraBuffer;
    bool                            mWKeyDown;
    bool                            mAKeyDown;
    bool                            mSKeyDown;
    bool                            mDKeyDown;
    bool                            mShiftDown;
    bool                            mLMBDown;
    vec2                            mCursorPos;
};
