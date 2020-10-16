#include "rtxApp.h"

#include "shared_with_shaders.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"


static const String sShadersFolder = "_data/shaders/";
static const String sScenesFolder = "_data/scenes/";

static const float sMoveSpeed = 2.0f;
static const float sAccelMult = 5.0f;
static const float sRotateSpeed = 0.25f;

static const vec3 sSunPos = vec3(0.4f, 0.45f, 0.55f);
static const float sAmbientLight = 0.1f;



RtxApp::RtxApp()
    : VulkanApp()
    , mRTPipelineLayout(VK_NULL_HANDLE)
    , mRTPipeline(VK_NULL_HANDLE)
    , mRTDescriptorPool(VK_NULL_HANDLE)
    , mWKeyDown(false)
    , mAKeyDown(false)
    , mSKeyDown(false)
    , mDKeyDown(false)
    , mShiftDown(false)
    , mLMBDown(false)
{
}
RtxApp::~RtxApp() {

}


void RtxApp::InitSettings() {
    mSettings.name = "rtxON";
    mSettings.enableValidation = true;
    mSettings.enableVSync = false;
    mSettings.supportRaytracing = true;
    mSettings.supportDescriptorIndexing = true;
}

void RtxApp::InitApp() {
    this->LoadSceneGeometry();
    this->CreateScene();
    this->CreateCamera();
    this->CreateDescriptorSetsLayouts();
    this->CreateRaytracingPipelineAndSBT();
    this->UpdateDescriptorSets();
}

void RtxApp::FreeResources() {
    for (RTMesh& mesh : mScene.meshes) {
        vkDestroyAccelerationStructureKHR(mDevice, mesh.blas.accelerationStructure, nullptr);
        vkFreeMemory(mDevice, mesh.blas.memory, nullptr);
    }
    mScene.meshes.clear();
    mScene.materials.clear();

    if (mScene.topLevelAS.accelerationStructure) {
        vkDestroyAccelerationStructureKHR(mDevice, mScene.topLevelAS.accelerationStructure, nullptr);
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

    mSBT.Destroy();

    if (mRTPipeline) {
        vkDestroyPipeline(mDevice, mRTPipeline, nullptr);
        mRTPipeline = VK_NULL_HANDLE;
    }

    if (mRTPipelineLayout) {
        vkDestroyPipelineLayout(mDevice, mRTPipelineLayout, nullptr);
        mRTPipelineLayout = VK_NULL_HANDLE;
    }

    for (VkDescriptorSetLayout& dsl : mRTDescriptorSetsLayouts) {
        vkDestroyDescriptorSetLayout(mDevice, dsl, nullptr);
    }
    mRTDescriptorSetsLayouts.clear();
}

void RtxApp::FillCommandBuffer(VkCommandBuffer commandBuffer, const size_t imageIndex) {
    vkCmdBindPipeline(commandBuffer,
                      VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                      mRTPipeline);

    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            mRTPipelineLayout, 0,
                            static_cast<uint32_t>(mRTDescriptorSets.size()), mRTDescriptorSets.data(),
                            0, 0);

    VkStridedBufferRegionKHR raygenSBT = {
        mSBT.GetSBTBuffer(),
        mSBT.GetRaygenOffset(),
        mSBT.GetGroupsStride(),
        mSBT.GetRaygenSize()
    };

    VkStridedBufferRegionKHR hitSBT = {
        mSBT.GetSBTBuffer(),
        mSBT.GetHitGroupsOffset(),
        mSBT.GetGroupsStride(),
        mSBT.GetHitGroupsSize()
    };

    VkStridedBufferRegionKHR missSBT = {
        mSBT.GetSBTBuffer(),
        mSBT.GetMissGroupsOffset(),
        mSBT.GetGroupsStride(),
        mSBT.GetMissGroupsSize()
    };

    VkStridedBufferRegionKHR callableSBT = {};

    vkCmdTraceRaysKHR(commandBuffer, &raygenSBT, &missSBT, &hitSBT, &callableSBT, mSettings.resolutionX, mSettings.resolutionY, 1u);
}

void RtxApp::OnMouseMove(const float x, const float y) {
    vec2 newPos(x, y);
    vec2 delta = mCursorPos - newPos;

    if (mLMBDown) {
        mCamera.Rotate(delta.x * sRotateSpeed, delta.y * sRotateSpeed);
    }

    mCursorPos = newPos;
}

void RtxApp::OnMouseButton(const int button, const int action, const int mods) {
    if (0 == button && GLFW_PRESS == action) {
        mLMBDown = true;
    } else if (0 == button && GLFW_RELEASE == action) {
        mLMBDown = false;
    }
}

void RtxApp::OnKey(const int key, const int scancode, const int action, const int mods) {
    if (GLFW_PRESS == action) {
        switch (key) {
            case GLFW_KEY_W: mWKeyDown = true; break;
            case GLFW_KEY_A: mAKeyDown = true; break;
            case GLFW_KEY_S: mSKeyDown = true; break;
            case GLFW_KEY_D: mDKeyDown = true; break;

            case GLFW_KEY_LEFT_SHIFT:
            case GLFW_KEY_RIGHT_SHIFT:
                mShiftDown = true;
            break;
        }
    } else if (GLFW_RELEASE == action) {
        switch (key) {
            case GLFW_KEY_W: mWKeyDown = false; break;
            case GLFW_KEY_A: mAKeyDown = false; break;
            case GLFW_KEY_S: mSKeyDown = false; break;
            case GLFW_KEY_D: mDKeyDown = false; break;

            case GLFW_KEY_LEFT_SHIFT:
            case GLFW_KEY_RIGHT_SHIFT:
                mShiftDown = false;
            break;
        }
    }
}

void RtxApp::Update(const size_t, const float dt) {
    // Update FPS text
    String frameStats = ToString(mFPSMeter.GetFPS(), 1) + " FPS (" + ToString(mFPSMeter.GetFrameTime(), 1) + " ms)";
    String fullTitle = mSettings.name + "  " + frameStats;
    glfwSetWindowTitle(mWindow, fullTitle.c_str());
    /////////////////


    UniformParams* params = reinterpret_cast<UniformParams*>(mCameraBuffer.Map());

    params->sunPosAndAmbient = vec4(sSunPos, sAmbientLight);

    this->UpdateCameraParams(params, dt);

    mCameraBuffer.Unmap();
}



bool RtxApp::CreateAS(const VkAccelerationStructureTypeKHR type,
                      const uint32_t geometryCount,
                      const VkAccelerationStructureCreateGeometryTypeInfoKHR* geometries,
                      const uint32_t instanceCount,
                      RTAccelerationStructure& _as) {

    VkAccelerationStructureCreateInfoKHR& accelerationStructureInfo = _as.accelerationStructureInfo;
    accelerationStructureInfo = {};
    accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    accelerationStructureInfo.type = type;
    accelerationStructureInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    accelerationStructureInfo.maxGeometryCount = geometryCount;
    accelerationStructureInfo.pGeometryInfos = geometries;

    VkResult error = vkCreateAccelerationStructureKHR(mDevice, &accelerationStructureInfo, nullptr, &_as.accelerationStructure);
    if (VK_SUCCESS != error) {
        CHECK_VK_ERROR(error, "vkCreateAccelerationStructureKHR");
        return false;
    }

    VkAccelerationStructureMemoryRequirementsInfoKHR memoryRequirementsInfo = {};
    memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR;
    memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_KHR;
    memoryRequirementsInfo.buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;
    memoryRequirementsInfo.accelerationStructure = _as.accelerationStructure;

    VkMemoryRequirements2 memoryRequirements = {};
    memoryRequirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    vkGetAccelerationStructureMemoryRequirementsKHR(mDevice, &memoryRequirementsInfo, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = vulkanhelpers::GetMemoryType(memoryRequirements.memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    error = vkAllocateMemory(mDevice, &memoryAllocateInfo, nullptr, &_as.memory);
    if (VK_SUCCESS != error) {
        CHECK_VK_ERROR(error, "vkAllocateMemory for AS");
        return false;
    }

    VkBindAccelerationStructureMemoryInfoKHR bindInfo = {};
    bindInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_KHR;
    bindInfo.accelerationStructure = _as.accelerationStructure;
    bindInfo.memory = _as.memory;

    error = vkBindAccelerationStructureMemoryKHR(mDevice, 1, &bindInfo);
    if (VK_SUCCESS != error) {
        CHECK_VK_ERROR(error, "vkBindAccelerationStructureMemoryKHR");
        return false;
    }

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        nullptr,
        _as.accelerationStructure
    };

    _as.handle = vkGetAccelerationStructureDeviceAddressKHR(mDevice, &addressInfo);

    return true;
}

void RtxApp::LoadSceneGeometry() {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    String warn, error;

    String fileName = sScenesFolder + "fake_whitted/fake_whitted.obj";
    String baseDir = fileName;
    const size_t slash = baseDir.find_last_of('/');
    if (slash != String::npos) {
        baseDir.erase(slash);
    }

    const bool result = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &error, fileName.c_str(), baseDir.c_str(), true);
    if (result) {
        mScene.meshes.resize(shapes.size());
        mScene.materials.resize(materials.size());

        for (size_t meshIdx = 0; meshIdx < shapes.size(); ++meshIdx) {
            RTMesh& mesh = mScene.meshes[meshIdx];
            const tinyobj::shape_t& shape = shapes[meshIdx];

            const size_t numFaces = shape.mesh.num_face_vertices.size();
            const size_t numVertices = numFaces * 3;

            mesh.numVertices = static_cast<uint32_t>(numVertices);
            mesh.numFaces = static_cast<uint32_t>(numFaces);

            const size_t positionsBufferSize = numVertices * sizeof(vec3);
            const size_t indicesBufferSize = numFaces * 3 * sizeof(uint32_t);
            const size_t facesBufferSize = numFaces * 4 * sizeof(uint32_t);
            const size_t attribsBufferSize = numVertices * sizeof(VertexAttribute);
            const size_t matIDsBufferSize = numFaces * sizeof(uint32_t);

            VkResult error = mesh.positions.Create(positionsBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            CHECK_VK_ERROR(error, "mesh.positions.Create");

            error = mesh.indices.Create(indicesBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            CHECK_VK_ERROR(error, "mesh.indices.Create");

            error = mesh.faces.Create(facesBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            CHECK_VK_ERROR(error, "mesh.faces.Create");

            error = mesh.attribs.Create(attribsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            CHECK_VK_ERROR(error, "mesh.attribs.Create");

            error = mesh.matIDs.Create(matIDsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            CHECK_VK_ERROR(error, "mesh.matIDs.Create");

            vec3* positions = reinterpret_cast<vec3*>(mesh.positions.Map());
            VertexAttribute* attribs = reinterpret_cast<VertexAttribute*>(mesh.attribs.Map());
            uint32_t* indices = reinterpret_cast<uint32_t*>(mesh.indices.Map());
            uint32_t* faces = reinterpret_cast<uint32_t*>(mesh.faces.Map());
            uint32_t* matIDs = reinterpret_cast<uint32_t*>(mesh.matIDs.Map());

            size_t vIdx = 0;
            for (size_t f = 0; f < numFaces; ++f) {
                assert(shape.mesh.num_face_vertices[f] == 3);
                for (size_t j = 0; j < 3; ++j, ++vIdx) {
                    const tinyobj::index_t& i = shape.mesh.indices[vIdx];

                    vec3& pos = positions[vIdx];
                    vec4& normal = attribs[vIdx].normal;
                    vec4& uv = attribs[vIdx].uv;

                    pos.x = attrib.vertices[3 * i.vertex_index + 0];
                    pos.y = attrib.vertices[3 * i.vertex_index + 1];
                    pos.z = attrib.vertices[3 * i.vertex_index + 2];
                    normal.x = attrib.normals[3 * i.normal_index + 0];
                    normal.y = attrib.normals[3 * i.normal_index + 1];
                    normal.z = attrib.normals[3 * i.normal_index + 2];
                    uv.x = attrib.texcoords[2 * i.texcoord_index + 0];
                    uv.y = attrib.texcoords[2 * i.texcoord_index + 1];
                }

                const uint32_t a = static_cast<uint32_t>(3 * f + 0);
                const uint32_t b = static_cast<uint32_t>(3 * f + 1);
                const uint32_t c = static_cast<uint32_t>(3 * f + 2);
                indices[a] = a;
                indices[b] = b;
                indices[c] = c;
                faces[4 * f + 0] = a;
                faces[4 * f + 1] = b;
                faces[4 * f + 2] = c;

                matIDs[f] = static_cast<uint32_t>(shape.mesh.material_ids[f]);
            }

            mesh.matIDs.Unmap();
            mesh.indices.Unmap();
            mesh.faces.Unmap();
            mesh.attribs.Unmap();
            mesh.positions.Unmap();
        }

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        for (size_t i = 0; i < materials.size(); ++i) {
            const tinyobj::material_t& srcMat = materials[i];
            RTMaterial& dstMat = mScene.materials[i];

            String fullTexturePath = baseDir + "/" + srcMat.diffuse_texname;
            if (dstMat.texture.Load(fullTexturePath.c_str())) {
                dstMat.texture.CreateImageView(VK_IMAGE_VIEW_TYPE_2D, dstMat.texture.GetFormat(), subresourceRange);
                dstMat.texture.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
            }
        }
    }

    // prepare shader resources infos
    const size_t numMeshes = mScene.meshes.size();
    const size_t numMaterials = mScene.materials.size();

    mScene.matIDsBufferInfos.resize(numMeshes);
    mScene.attribsBufferInfos.resize(numMeshes);
    mScene.facesBufferInfos.resize(numMeshes);
    for (size_t i = 0; i < numMeshes; ++i) {
        const RTMesh& mesh = mScene.meshes[i];
        VkDescriptorBufferInfo& matIDsInfo = mScene.matIDsBufferInfos[i];
        VkDescriptorBufferInfo& attribsInfo = mScene.attribsBufferInfos[i];
        VkDescriptorBufferInfo& facesInfo = mScene.facesBufferInfos[i];

        matIDsInfo.buffer = mesh.matIDs.GetBuffer();
        matIDsInfo.offset = 0;
        matIDsInfo.range = mesh.matIDs.GetSize();

        attribsInfo.buffer = mesh.attribs.GetBuffer();
        attribsInfo.offset = 0;
        attribsInfo.range = mesh.attribs.GetSize();

        facesInfo.buffer = mesh.faces.GetBuffer();
        facesInfo.offset = 0;
        facesInfo.range = mesh.faces.GetSize();
    }

    mScene.texturesInfos.resize(numMaterials);
    for (size_t i = 0; i < numMaterials; ++i) {
        const RTMaterial& mat = mScene.materials[i];
        VkDescriptorImageInfo& textureInfo = mScene.texturesInfos[i];

        textureInfo.sampler = mat.texture.GetSampler();
        textureInfo.imageView = mat.texture.GetImageView();
        textureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
}

void RtxApp::CreateScene() {
    const VkTransformMatrixKHR transform = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
    };

    const size_t numMeshes = mScene.meshes.size();

    Array<VkAccelerationStructureCreateGeometryTypeInfoKHR> geometryInfos(numMeshes, VkAccelerationStructureCreateGeometryTypeInfoKHR{});
    Array<VkAccelerationStructureGeometryKHR> geometries(numMeshes, VkAccelerationStructureGeometryKHR{});
    Array<VkAccelerationStructureInstanceKHR> instances(numMeshes, VkAccelerationStructureInstanceKHR{});

    for (size_t i = 0; i < numMeshes; ++i) {
        RTMesh& mesh = mScene.meshes[i];

        VkAccelerationStructureCreateGeometryTypeInfoKHR& geometryInfo = geometryInfos[i];
        VkAccelerationStructureGeometryKHR& geometry = geometries[i];

        geometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR;
        geometryInfo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometryInfo.maxPrimitiveCount = mesh.numFaces;
        geometryInfo.indexType = VK_INDEX_TYPE_UINT32;
        geometryInfo.maxVertexCount = mesh.numVertices;
        geometryInfo.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        geometryInfo.allowsTransforms = VK_FALSE;

        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geometry.geometryType = geometryInfo.geometryType;
        geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geometry.geometry.triangles.vertexData = vulkanhelpers::GetBufferDeviceAddressConst(mesh.positions);
        geometry.geometry.triangles.vertexStride = sizeof(vec3);
        geometry.geometry.triangles.vertexFormat = geometryInfo.vertexFormat;
        geometry.geometry.triangles.indexData = vulkanhelpers::GetBufferDeviceAddressConst(mesh.indices);
        geometry.geometry.triangles.indexType = geometryInfo.indexType;

        // here we create our bottom-level acceleration structure for our mesh
        this->CreateAS(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, 1, &geometryInfo, 0, mesh.blas);


        VkAccelerationStructureInstanceKHR& instance = instances[i];
        instance.transform = transform;
        instance.instanceCustomIndex = static_cast<uint32_t>(i);
        instance.mask = 0xff;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = mesh.blas.handle;
    }

    // create instances for our meshes
    vulkanhelpers::Buffer instancesBuffer;
    VkResult error = instancesBuffer.Create(instances.size() * sizeof(VkAccelerationStructureInstanceKHR), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    CHECK_VK_ERROR(error, "instancesBuffer.Create");

    if (!instancesBuffer.UploadData(instances.data(), instancesBuffer.GetSize())) {
        assert(false && "Failed to upload instances buffer");
    }


    // and here we create out top-level acceleration structure that'll represent our scene
    VkAccelerationStructureCreateGeometryTypeInfoKHR tlasGeoInfo = {};
    tlasGeoInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR;
    tlasGeoInfo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeoInfo.maxPrimitiveCount = static_cast<uint32_t>(instances.size());
    tlasGeoInfo.allowsTransforms = VK_TRUE;

    this->CreateAS(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, 1, &tlasGeoInfo, 1, mScene.topLevelAS);

    // now we have to build them

    VkAccelerationStructureMemoryRequirementsInfoKHR memoryRequirementsInfo = {};
    memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR;
    memoryRequirementsInfo.buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;

    VkDeviceSize maximumBlasSize = 0;
    for (const RTMesh& mesh : mScene.meshes) {
        memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_KHR;
        memoryRequirementsInfo.accelerationStructure = mesh.blas.accelerationStructure;

        VkMemoryRequirements2 memReqBLAS = {};
        memReqBLAS.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
        vkGetAccelerationStructureMemoryRequirementsKHR(mDevice, &memoryRequirementsInfo, &memReqBLAS);

        maximumBlasSize = Max(maximumBlasSize, memReqBLAS.memoryRequirements.size);
    }

    VkMemoryRequirements2 memReqTLAS = {};
    memReqTLAS.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_KHR;
    memoryRequirementsInfo.accelerationStructure = mScene.topLevelAS.accelerationStructure;
    vkGetAccelerationStructureMemoryRequirementsKHR(mDevice, &memoryRequirementsInfo, &memReqTLAS);

    const VkDeviceSize scratchBufferSize = Max(maximumBlasSize, memReqTLAS.memoryRequirements.size);

    vulkanhelpers::Buffer scratchBuffer;
    error = scratchBuffer.Create(scratchBufferSize, VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CHECK_VK_ERROR(error, "scratchBuffer.Create");

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = mCommandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    error = vkAllocateCommandBuffers(mDevice, &commandBufferAllocateInfo, &commandBuffer);
    CHECK_VK_ERROR(error, "vkAllocateCommandBuffers");

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    // build bottom-level ASs
    for (size_t i = 0; i < numMeshes; ++i) {
        VkAccelerationStructureGeometryKHR* geometryPtr = &geometries[i];

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type = mScene.meshes[i].blas.accelerationStructureInfo.type;
        buildInfo.flags = mScene.meshes[i].blas.accelerationStructureInfo.flags;
        buildInfo.update = VK_FALSE;
        buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
        buildInfo.dstAccelerationStructure = mScene.meshes[i].blas.accelerationStructure;
        buildInfo.geometryArrayOfPointers = VK_FALSE;
        buildInfo.geometryCount = mScene.meshes[i].blas.accelerationStructureInfo.maxGeometryCount;
        buildInfo.ppGeometries = &geometryPtr;
        buildInfo.scratchData = vulkanhelpers::GetBufferDeviceAddress(scratchBuffer);

        VkAccelerationStructureBuildOffsetInfoKHR offsetInfo;
        offsetInfo.primitiveCount = geometryInfos[i].maxPrimitiveCount;
        offsetInfo.primitiveOffset = 0;
        offsetInfo.firstVertex = 0;
        offsetInfo.transformOffset = 0;

        VkAccelerationStructureBuildOffsetInfoKHR* offsets[1] = { &offsetInfo };

        vkCmdBuildAccelerationStructureKHR(commandBuffer, 1, &buildInfo, offsets);

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                             0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
    }

    // build top-level AS
    VkAccelerationStructureGeometryKHR topLevelGeometry = {};
    topLevelGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    topLevelGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    topLevelGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    topLevelGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    topLevelGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
    topLevelGeometry.geometry.instances.data.deviceAddress = vulkanhelpers::GetBufferDeviceAddress(instancesBuffer).deviceAddress;

    VkAccelerationStructureGeometryKHR* geometryPtr = &topLevelGeometry;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = mScene.topLevelAS.accelerationStructureInfo.type;
    buildInfo.flags = mScene.topLevelAS.accelerationStructureInfo.flags;
    buildInfo.update = VK_FALSE;
    buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure = mScene.topLevelAS.accelerationStructure;
    buildInfo.geometryArrayOfPointers = VK_FALSE;
    buildInfo.geometryCount = 1;
    buildInfo.ppGeometries = &geometryPtr;
    buildInfo.scratchData = vulkanhelpers::GetBufferDeviceAddress(scratchBuffer);

    VkAccelerationStructureBuildOffsetInfoKHR offsetInfo;
    offsetInfo.primitiveCount = tlasGeoInfo.maxPrimitiveCount;
    offsetInfo.primitiveOffset = 0;
    offsetInfo.firstVertex = 0;
    offsetInfo.transformOffset = 0;

    VkAccelerationStructureBuildOffsetInfoKHR* offsets[1] = { &offsetInfo };

    vkCmdBuildAccelerationStructureKHR(commandBuffer, 1, &buildInfo, offsets);

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    error = vkQueueWaitIdle(mGraphicsQueue);
    CHECK_VK_ERROR(error, "vkQueueWaitIdle");
    vkFreeCommandBuffers(mDevice, mCommandPool, 1, &commandBuffer);
}

void RtxApp::CreateCamera() {
    VkResult error = mCameraBuffer.Create(sizeof(UniformParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    CHECK_VK_ERROR(error, "mCameraBuffer.Create");

    mCamera.SetViewport({ 0, 0, static_cast<int>(mSettings.resolutionX), static_cast<int>(mSettings.resolutionY) });
    mCamera.SetViewPlanes(0.1f, 100.0f);
    mCamera.SetFovY(45.0f);
    mCamera.LookAt(vec3(0.25f, 3.20f, 6.15f), vec3(0.25f, 2.75f, 5.25f));
}

void RtxApp::UpdateCameraParams(UniformParams* params, const float dt) {
    vec2 moveDelta(0.0f, 0.0f);
    if (mWKeyDown) {
        moveDelta.y += 1.0f;
    }
    if (mSKeyDown) {
        moveDelta.y -= 1.0f;
    }
    if (mAKeyDown) {
        moveDelta.x -= 1.0f;
    }
    if (mDKeyDown) {
        moveDelta.x += 1.0f;
    }

    moveDelta *= sMoveSpeed * dt * (mShiftDown ? sAccelMult : 1.0f);
    mCamera.Move(moveDelta.x, moveDelta.y);

    params->camPos = vec4(mCamera.GetPosition(), 0.0f);
    params->camDir = vec4(mCamera.GetDirection(), 0.0f);
    params->camUp = vec4(mCamera.GetUp(), 0.0f);
    params->camSide = vec4(mCamera.GetSide(), 0.0f);
    params->camNearFarFov = vec4(mCamera.GetNearPlane(), mCamera.GetFarPlane(), Deg2Rad(mCamera.GetFovY()), 0.0f);
}

void RtxApp::CreateDescriptorSetsLayouts() {
    const uint32_t numMeshes = static_cast<uint32_t>(mScene.meshes.size());
    const uint32_t numMaterials = static_cast<uint32_t>(mScene.materials.size());

    mRTDescriptorSetsLayouts.resize(SWS_NUM_SETS);

    // First set:
    //  binding 0  ->  AS
    //  binding 1  ->  output image
    //  binding 2  ->  Camera data

    VkDescriptorSetLayoutBinding accelerationStructureLayoutBinding;
    accelerationStructureLayoutBinding.binding = SWS_SCENE_AS_BINDING;
    accelerationStructureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    accelerationStructureLayoutBinding.descriptorCount = 1;
    accelerationStructureLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    accelerationStructureLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding resultImageLayoutBinding;
    resultImageLayoutBinding.binding = SWS_RESULT_IMAGE_BINDING;
    resultImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    resultImageLayoutBinding.descriptorCount = 1;
    resultImageLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    resultImageLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding camdataBufferBinding;
    camdataBufferBinding.binding = SWS_CAMDATA_BINDING;
    camdataBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    camdataBufferBinding.descriptorCount = 1;
    camdataBufferBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    camdataBufferBinding.pImmutableSamplers = nullptr;

    std::vector<VkDescriptorSetLayoutBinding> bindings({
        accelerationStructureLayoutBinding,
        resultImageLayoutBinding,
        camdataBufferBinding
    });

    VkDescriptorSetLayoutCreateInfo set0LayoutInfo;
    set0LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set0LayoutInfo.pNext = nullptr;
    set0LayoutInfo.flags = 0;
    set0LayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    set0LayoutInfo.pBindings = bindings.data();

    VkResult error = vkCreateDescriptorSetLayout(mDevice, &set0LayoutInfo, nullptr, &mRTDescriptorSetsLayouts[SWS_SCENE_AS_SET]);
    CHECK_VK_ERROR(error, "vkCreateDescriptorSetLayout");

    // Second set:
    //  binding 0 .. N  ->  per-face material IDs for our meshes  (N = num meshes)

    const VkDescriptorBindingFlags flag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags;
    bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlags.pNext = nullptr;
    bindingFlags.pBindingFlags = &flag;
    bindingFlags.bindingCount = 1;

    VkDescriptorSetLayoutBinding ssboBinding;
    ssboBinding.binding = 0;
    ssboBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ssboBinding.descriptorCount = numMeshes;
    ssboBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    ssboBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo set1LayoutInfo;
    set1LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set1LayoutInfo.pNext = &bindingFlags;
    set1LayoutInfo.flags = 0;
    set1LayoutInfo.bindingCount = 1;
    set1LayoutInfo.pBindings = &ssboBinding;

    error = vkCreateDescriptorSetLayout(mDevice, &set1LayoutInfo, nullptr, &mRTDescriptorSetsLayouts[SWS_MATIDS_SET]);
    CHECK_VK_ERROR(error, L"vkCreateDescriptorSetLayout");

    // Third set:
    //  binding 0 .. N  ->  vertex attributes for our meshes  (N = num meshes)
    //   (re-using second's set info)

    error = vkCreateDescriptorSetLayout(mDevice, &set1LayoutInfo, nullptr, &mRTDescriptorSetsLayouts[SWS_ATTRIBS_SET]);
    CHECK_VK_ERROR(error, L"vkCreateDescriptorSetLayout");

    // Fourth set:
    //  binding 0 .. N  ->  faces info (indices) for our meshes  (N = num meshes)
    //   (re-using second's set info)

    error = vkCreateDescriptorSetLayout(mDevice, &set1LayoutInfo, nullptr, &mRTDescriptorSetsLayouts[SWS_FACES_SET]);
    CHECK_VK_ERROR(error, L"vkCreateDescriptorSetLayout");

    // Fifth set:
    //  binding 0 .. N  ->  textures (N = num materials)
    
    VkDescriptorSetLayoutBinding textureBinding;
    textureBinding.binding = 0;
    textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureBinding.descriptorCount = numMaterials;
    textureBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    textureBinding.pImmutableSamplers = nullptr;

    set1LayoutInfo.pBindings = &textureBinding;

    error = vkCreateDescriptorSetLayout(mDevice, &set1LayoutInfo, nullptr, &mRTDescriptorSetsLayouts[SWS_TEXTURES_SET]);
    CHECK_VK_ERROR(error, L"vkCreateDescriptorSetLayout");
}

void RtxApp::CreateRaytracingPipelineAndSBT() {
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = SWS_NUM_SETS;
    pipelineLayoutCreateInfo.pSetLayouts = mRTDescriptorSetsLayouts.data();

    VkResult error = vkCreatePipelineLayout(mDevice, &pipelineLayoutCreateInfo, nullptr, &mRTPipelineLayout);
    CHECK_VK_ERROR(error, "vkCreatePipelineLayout");


    vulkanhelpers::Shader rayGenShader, rayChitShader, rayMissShader, shadowChit, shadowMiss;
    rayGenShader.LoadFromFile((sShadersFolder + "ray_gen.bin").c_str());
    rayChitShader.LoadFromFile((sShadersFolder + "ray_chit.bin").c_str());
    rayMissShader.LoadFromFile((sShadersFolder + "ray_miss.bin").c_str());
    shadowChit.LoadFromFile((sShadersFolder + "shadow_ray_chit.bin").c_str());
    shadowMiss.LoadFromFile((sShadersFolder + "shadow_ray_miss.bin").c_str());

    mSBT.Initialize(2, 2, mRTProps.shaderGroupHandleSize, mRTProps.shaderGroupBaseAlignment);

    mSBT.SetRaygenStage(rayGenShader.GetShaderStage(VK_SHADER_STAGE_RAYGEN_BIT_KHR));

    mSBT.AddStageToHitGroup({ rayChitShader.GetShaderStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) }, SWS_PRIMARY_HIT_SHADERS_IDX);
    mSBT.AddStageToHitGroup({ shadowChit.GetShaderStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) }, SWS_SHADOW_HIT_SHADERS_IDX);

    mSBT.AddStageToMissGroup(rayMissShader.GetShaderStage(VK_SHADER_STAGE_MISS_BIT_KHR), SWS_PRIMARY_MISS_SHADERS_IDX);
    mSBT.AddStageToMissGroup(shadowMiss.GetShaderStage(VK_SHADER_STAGE_MISS_BIT_KHR), SWS_SHADOW_MISS_SHADERS_IDX);


    VkRayTracingPipelineCreateInfoKHR rayPipelineInfo = {};
    rayPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    rayPipelineInfo.stageCount = mSBT.GetNumStages();
    rayPipelineInfo.pStages = mSBT.GetStages();
    rayPipelineInfo.groupCount = mSBT.GetNumGroups();
    rayPipelineInfo.pGroups = mSBT.GetGroups();
    rayPipelineInfo.maxRecursionDepth = 1;
    rayPipelineInfo.layout = mRTPipelineLayout;
    rayPipelineInfo.libraries.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;

    error = vkCreateRayTracingPipelinesKHR(mDevice, VK_NULL_HANDLE, 1, &rayPipelineInfo, VK_NULL_HANDLE, &mRTPipeline);
    CHECK_VK_ERROR(error, "vkCreateRayTracingPipelinesKHR");

    mSBT.CreateSBT(mDevice, mRTPipeline);
}

void RtxApp::UpdateDescriptorSets() {
    const uint32_t numMeshes = static_cast<uint32_t>(mScene.meshes.size());
    const uint32_t numMaterials = static_cast<uint32_t>(mScene.materials.size());

    std::vector<VkDescriptorPoolSize> poolSizes({
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },       // top-level AS
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },                    // output image
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },                   // Camera data
        //
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numMeshes * 3 },       // per-face material IDs for each mesh
                                                                    // vertex attribs for each mesh
                                                                    // faces buffer for each mesh
        //
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numMaterials } // textures for each material
    });

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.pNext = nullptr;
    descriptorPoolCreateInfo.flags = 0;
    descriptorPoolCreateInfo.maxSets = SWS_NUM_SETS;
    descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();

    VkResult error = vkCreateDescriptorPool(mDevice, &descriptorPoolCreateInfo, nullptr, &mRTDescriptorPool);
    CHECK_VK_ERROR(error, "vkCreateDescriptorPool");

    mRTDescriptorSets.resize(SWS_NUM_SETS);

    Array<uint32_t> variableDescriptorCounts({
        1,
        numMeshes,      // per-face material IDs for each mesh
        numMeshes,      // vertex attribs for each mesh
        numMeshes,      // faces buffer for each mesh
        numMaterials,   // textures for each material
    });

    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountInfo;
    variableDescriptorCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    variableDescriptorCountInfo.pNext = nullptr;
    variableDescriptorCountInfo.descriptorSetCount = SWS_NUM_SETS;
    variableDescriptorCountInfo.pDescriptorCounts = variableDescriptorCounts.data(); // actual number of descriptors

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.pNext = &variableDescriptorCountInfo;
    descriptorSetAllocateInfo.descriptorPool = mRTDescriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = SWS_NUM_SETS;
    descriptorSetAllocateInfo.pSetLayouts = mRTDescriptorSetsLayouts.data();

    error = vkAllocateDescriptorSets(mDevice, &descriptorSetAllocateInfo, mRTDescriptorSets.data());
    CHECK_VK_ERROR(error, "vkAllocateDescriptorSets");

    ///////////////////////////////////////////////////////////

    VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo;
    descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    descriptorAccelerationStructureInfo.pNext = nullptr;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    descriptorAccelerationStructureInfo.pAccelerationStructures = &mScene.topLevelAS.accelerationStructure;

    VkWriteDescriptorSet accelerationStructureWrite;
    accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo; // Notice that pNext is assigned here!
    accelerationStructureWrite.dstSet = mRTDescriptorSets[SWS_SCENE_AS_SET];
    accelerationStructureWrite.dstBinding = SWS_SCENE_AS_BINDING;
    accelerationStructureWrite.dstArrayElement = 0;
    accelerationStructureWrite.descriptorCount = 1;
    accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    accelerationStructureWrite.pImageInfo = nullptr;
    accelerationStructureWrite.pBufferInfo = nullptr;
    accelerationStructureWrite.pTexelBufferView = nullptr;

    ///////////////////////////////////////////////////////////

    VkDescriptorImageInfo descriptorOutputImageInfo;
    descriptorOutputImageInfo.sampler = VK_NULL_HANDLE;
    descriptorOutputImageInfo.imageView = mOffscreenImage.GetImageView();
    descriptorOutputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet resultImageWrite;
    resultImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    resultImageWrite.pNext = nullptr;
    resultImageWrite.dstSet = mRTDescriptorSets[SWS_RESULT_IMAGE_SET];
    resultImageWrite.dstBinding = SWS_RESULT_IMAGE_BINDING;
    resultImageWrite.dstArrayElement = 0;
    resultImageWrite.descriptorCount = 1;
    resultImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    resultImageWrite.pImageInfo = &descriptorOutputImageInfo;
    resultImageWrite.pBufferInfo = nullptr;
    resultImageWrite.pTexelBufferView = nullptr;

    ///////////////////////////////////////////////////////////

    VkDescriptorBufferInfo camdataBufferInfo;
    camdataBufferInfo.buffer = mCameraBuffer.GetBuffer();
    camdataBufferInfo.offset = 0;
    camdataBufferInfo.range = mCameraBuffer.GetSize();

    VkWriteDescriptorSet camdataBufferWrite;
    camdataBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    camdataBufferWrite.pNext = nullptr;
    camdataBufferWrite.dstSet = mRTDescriptorSets[SWS_CAMDATA_SET];
    camdataBufferWrite.dstBinding = SWS_CAMDATA_BINDING;
    camdataBufferWrite.dstArrayElement = 0;
    camdataBufferWrite.descriptorCount = 1;
    camdataBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    camdataBufferWrite.pImageInfo = nullptr;
    camdataBufferWrite.pBufferInfo = &camdataBufferInfo;
    camdataBufferWrite.pTexelBufferView = nullptr;

    ///////////////////////////////////////////////////////////

    VkWriteDescriptorSet matIDsBufferWrite;
    matIDsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    matIDsBufferWrite.pNext = nullptr;
    matIDsBufferWrite.dstSet = mRTDescriptorSets[SWS_MATIDS_SET];
    matIDsBufferWrite.dstBinding = 0;
    matIDsBufferWrite.dstArrayElement = 0;
    matIDsBufferWrite.descriptorCount = numMeshes;
    matIDsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    matIDsBufferWrite.pImageInfo = nullptr;
    matIDsBufferWrite.pBufferInfo = mScene.matIDsBufferInfos.data();
    matIDsBufferWrite.pTexelBufferView = nullptr;

    ///////////////////////////////////////////////////////////

    VkWriteDescriptorSet attribsBufferWrite;
    attribsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    attribsBufferWrite.pNext = nullptr;
    attribsBufferWrite.dstSet = mRTDescriptorSets[SWS_ATTRIBS_SET];
    attribsBufferWrite.dstBinding = 0;
    attribsBufferWrite.dstArrayElement = 0;
    attribsBufferWrite.descriptorCount = numMeshes;
    attribsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    attribsBufferWrite.pImageInfo = nullptr;
    attribsBufferWrite.pBufferInfo = mScene.attribsBufferInfos.data();
    attribsBufferWrite.pTexelBufferView = nullptr;

    ///////////////////////////////////////////////////////////

    VkWriteDescriptorSet facesBufferWrite;
    facesBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    facesBufferWrite.pNext = nullptr;
    facesBufferWrite.dstSet = mRTDescriptorSets[SWS_FACES_SET];
    facesBufferWrite.dstBinding = 0;
    facesBufferWrite.dstArrayElement = 0;
    facesBufferWrite.descriptorCount = numMeshes;
    facesBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    facesBufferWrite.pImageInfo = nullptr;
    facesBufferWrite.pBufferInfo = mScene.facesBufferInfos.data();
    facesBufferWrite.pTexelBufferView = nullptr;

    ///////////////////////////////////////////////////////////

    VkWriteDescriptorSet texturesBufferWrite;
    texturesBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    texturesBufferWrite.pNext = nullptr;
    texturesBufferWrite.dstSet = mRTDescriptorSets[SWS_TEXTURES_SET];
    texturesBufferWrite.dstBinding = 0;
    texturesBufferWrite.dstArrayElement = 0;
    texturesBufferWrite.descriptorCount = numMaterials;
    texturesBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    texturesBufferWrite.pImageInfo = mScene.texturesInfos.data();
    texturesBufferWrite.pBufferInfo = nullptr;
    texturesBufferWrite.pTexelBufferView = nullptr;

    ///////////////////////////////////////////////////////////

    Array<VkWriteDescriptorSet> descriptorWrites({
        accelerationStructureWrite,
        resultImageWrite,
        camdataBufferWrite,
        //
        matIDsBufferWrite,
        //
        attribsBufferWrite,
        //
        facesBufferWrite,
        //
        texturesBufferWrite
    });

    vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, VK_NULL_HANDLE);
}


// SBT Helper class

static uint32_t AlignUp(const uint32_t value, const uint32_t align) {
    return (value + align - 1) & ~(align - 1);
}

SBTHelper::SBTHelper()
    : mShaderHandleSize(0u)
    , mShaderGroupAlignment(0u)
    , mNumHitGroups(0u)
    , mNumMissGroups(0u) {
}

void SBTHelper::Initialize(const uint32_t numHitGroups, const uint32_t numMissGroups, const uint32_t shaderHandleSize, const uint32_t shaderGroupAlignment) {
    mShaderHandleSize = shaderHandleSize;
    mShaderGroupAlignment = shaderGroupAlignment;
    mNumHitGroups = numHitGroups;
    mNumMissGroups = numMissGroups;

    mNumHitShaders.resize(numHitGroups, 0u);
    mNumMissShaders.resize(numMissGroups, 0u);

    mStages.clear();
    mGroups.clear();
}

void SBTHelper::Destroy() {
    mNumHitShaders.clear();
    mNumMissShaders.clear();
    mStages.clear();
    mGroups.clear();

    mSBTBuffer.Destroy();
}

void SBTHelper::SetRaygenStage(const VkPipelineShaderStageCreateInfo& stage) {
    // this shader stage should go first!
    assert(mStages.empty());
    mStages.push_back(stage);

    VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};
    groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groupInfo.generalShader = 0;
    groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
    groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
    groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
    mGroups.push_back(groupInfo); // group 0 is always for raygen
}

void SBTHelper::AddStageToHitGroup(const Array<VkPipelineShaderStageCreateInfo>& stages, const uint32_t groupIndex) {
    // raygen stage should go first!
    assert(!mStages.empty());

    assert(groupIndex < mNumHitShaders.size());
    assert(!stages.empty() && stages.size() <= 3);// only 3 hit shaders per group (intersection, any-hit and closest-hit)
    assert(mNumHitShaders[groupIndex] == 0);

    uint32_t offset = 1; // there's always raygen shader

    for (uint32_t i = 0; i <= groupIndex; ++i) {
        offset += mNumHitShaders[i];
    }

    auto itStage = mStages.begin() + offset;
    mStages.insert(itStage, stages.begin(), stages.end());

    VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};
    groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groupInfo.generalShader = VK_SHADER_UNUSED_KHR;
    groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
    groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
    groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

    for (size_t i = 0; i < stages.size(); i++) {
        const VkPipelineShaderStageCreateInfo& stageInfo = stages[i];
        const uint32_t shaderIdx = static_cast<uint32_t>(offset + i);

        if (stageInfo.stage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) {
            groupInfo.closestHitShader = shaderIdx;
        } else if (stageInfo.stage == VK_SHADER_STAGE_ANY_HIT_BIT_KHR) {
            groupInfo.anyHitShader = shaderIdx;
        }
    };

    mGroups.insert((mGroups.begin() + 1 + groupIndex), groupInfo);

    mNumHitShaders[groupIndex] += static_cast<uint32_t>(stages.size());
}

void SBTHelper::AddStageToMissGroup(const VkPipelineShaderStageCreateInfo& stage, const uint32_t groupIndex) {
    // raygen stage should go first!
    assert(!mStages.empty());

    assert(groupIndex < mNumMissShaders.size());
    assert(mNumMissShaders[groupIndex] == 0); // only 1 miss shader per group

    uint32_t offset = 1; // there's always raygen shader

    // now skip all hit shaders
    for (const uint32_t numHitShader : mNumHitShaders) {
        offset += numHitShader;
    }

    for (uint32_t i = 0; i <= groupIndex; ++i) {
        offset += mNumMissShaders[i];
    }

    mStages.insert(mStages.begin() + offset, stage);

    // group create info 
    VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};
    groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groupInfo.generalShader = offset;
    groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
    groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
    groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

    // group 0 is always for raygen, then go hit shaders
    mGroups.insert((mGroups.begin() + (groupIndex + 1 + mNumHitGroups)), groupInfo);

    mNumMissShaders[groupIndex]++;
}

uint32_t SBTHelper::GetGroupsStride() const {
    return mShaderGroupAlignment;
}

uint32_t SBTHelper::GetNumGroups() const {
    return 1 + mNumHitGroups + mNumMissGroups;
}

uint32_t SBTHelper::GetRaygenOffset() const {
    return 0;
}

uint32_t SBTHelper::GetRaygenSize() const {
    return mShaderGroupAlignment;
}

uint32_t SBTHelper::GetHitGroupsOffset() const {
    return this->GetRaygenOffset() + this->GetRaygenSize();
}

uint32_t SBTHelper::GetHitGroupsSize() const {
    return mNumHitGroups * mShaderGroupAlignment;
}

uint32_t SBTHelper::GetMissGroupsOffset() const {
    return this->GetHitGroupsOffset() + this->GetHitGroupsSize();
}

uint32_t SBTHelper::GetMissGroupsSize() const {
    return mNumMissGroups * mShaderGroupAlignment;
}

uint32_t SBTHelper::GetNumStages() const {
    return static_cast<uint32_t>(mStages.size());
}

const VkPipelineShaderStageCreateInfo* SBTHelper::GetStages() const {
    return mStages.data();
}

const VkRayTracingShaderGroupCreateInfoKHR* SBTHelper::GetGroups() const {
    return mGroups.data();
}

uint32_t SBTHelper::GetSBTSize() const {
    return this->GetNumGroups() * mShaderGroupAlignment;
}

bool SBTHelper::CreateSBT(VkDevice device, VkPipeline rtPipeline) {
    const size_t sbtSize = this->GetSBTSize();

    VkResult error = mSBTBuffer.Create(sbtSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    CHECK_VK_ERROR(error, "mSBT.Create");

    if (VK_SUCCESS != error) {
        return false;
    }

    // get shader group handles
    Array<uint8_t> groupHandles(this->GetNumGroups() * mShaderHandleSize);
    error = vkGetRayTracingShaderGroupHandlesKHR(device, rtPipeline, 0, this->GetNumGroups(), groupHandles.size(), groupHandles.data());
    CHECK_VK_ERROR(error, L"vkGetRayTracingShaderGroupHandlesKHR");

    // now we fill our SBT
    uint8_t* mem = static_cast<uint8_t*>(mSBTBuffer.Map());
    for (size_t i = 0; i < this->GetNumGroups(); ++i) {
        memcpy(mem, groupHandles.data() + i * mShaderHandleSize, mShaderHandleSize);
        mem += mShaderGroupAlignment;
    }
    mSBTBuffer.Unmap();

    return (VK_SUCCESS == error);
}

VkBuffer SBTHelper::GetSBTBuffer() const {
    return mSBTBuffer.GetBuffer();
}
