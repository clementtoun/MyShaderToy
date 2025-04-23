#include "VulkanCore.h"

#include <array>
#include <fstream>
#include <sstream>

void VulkanCore::SetWindow(GlfwWindow* window)
{
    m_Window = window;
}

void VulkanCore::CreateDevice(const std::string& ApplicationName, uint32_t ApplicationVersion, const std::string& EngineName, uint32_t EngineVersion)
{
    if (!m_Window)
    {
        std::cout << "Window is not valid, cannot create VulkanInstance !\n";
        return;
    }

    vkb::InstanceBuilder instance_builder;
    instance_builder.use_default_debug_messenger().
        set_app_name(ApplicationName.c_str()).
        set_app_version(ApplicationVersion).
        set_engine_name(EngineName.c_str()).
        set_engine_version(EngineVersion).
        require_api_version(VKB_VK_API_VERSION_1_3);

    uint32_t propertyCount;
    vkEnumerateInstanceLayerProperties(&propertyCount, NULL);

    std::vector<VkLayerProperties> layersProperties(propertyCount);
    vkEnumerateInstanceLayerProperties(&propertyCount, layersProperties.data());

    for (const auto& property : layersProperties)
    {
        for (const auto& wantedLayerName : wantedLayers)
        {
            if (strcmp(wantedLayerName.data(), property.layerName) == 0)
            {
                instance_builder.enable_layer(wantedLayerName.c_str());
            }
        }
    }

    if (enableValidationLayers)
        instance_builder.request_validation_layers().use_default_debug_messenger();

    auto instance_ret = instance_builder.build();
    if (!instance_ret) {
        std::cout << instance_ret.error().message() << "\n";
    }

    m_Instance = instance_ret.value();

    m_Inst_disp = m_Instance.make_table();

    VK_CHECK(m_Window->createSurface(m_Instance.instance, m_Surface));

    //vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features.dynamicRendering = true;
    features.synchronization2 = true;

    vkb::PhysicalDeviceSelector phys_device_selector(m_Instance);
    auto phys_device_ret = phys_device_selector.set_surface(m_Surface)
        .set_minimum_version(1, 3)
        .set_required_features_13(features)
        .add_required_extensions(deviceExtensions)
        .require_dedicated_transfer_queue()
        .select();
    if (!phys_device_ret) {
        std::cout << phys_device_ret.error().message() << "\n";
    }
    vkb::PhysicalDevice physical_device = phys_device_ret.value();

    bool extensionRes = physical_device.enable_extensions_if_present(deviceExtensions);

    vkb::DeviceBuilder device_builder{ physical_device };
    auto device_ret = device_builder.build();
    if (!device_ret) {
        std::cout << device_ret.error().message() << "\n";
    }
    m_Device = device_ret.value();

    m_Disp = m_Device.make_table();
}

void VulkanCore::CreateSwapChain()
{
    int w, h;
    do {
        glfwGetFramebufferSize(m_Window->getWindow(), &w, &h);
        glfwWaitEvents(); // Attend un resize
    } while (w == 0 || h == 0);

    const VkSurfaceFormatKHR surfaceFormat = {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    };

    vkb::SwapchainBuilder swapchain_builder{ m_Device };
    auto swap_ret = swapchain_builder.set_old_swapchain(m_Swapchain)
        .set_desired_format(surfaceFormat)
        .set_desired_present_mode(PRESENT_MODE)
        .build();
    if (!swap_ret) {
        std::cout << swap_ret.error().message() << " " << swap_ret.vk_result() << "\n";
    }
    vkb::destroy_swapchain(m_Swapchain);
    m_Swapchain = swap_ret.value();

    m_SwapchainImages = m_Swapchain.get_images().value();
    m_SwapchainImageViews = m_Swapchain.get_image_views().value();
}

void VulkanCore::RecreateSwapChain()
{
    m_Disp.deviceWaitIdle();

    m_Swapchain.destroy_image_views(m_SwapchainImageViews);

    CreateSwapChain();
}

void VulkanCore::GetQueues()
{
    auto gq = m_Device.get_queue(vkb::QueueType::graphics);
    if (!gq.has_value()) {
        std::cout << "Failed to get graphics queue: " << gq.error().message() << "\n";
    }
    m_GraphicsQueue = gq.value();

    auto pq = m_Device.get_queue(vkb::QueueType::present);
    if (!pq.has_value()) {
        std::cout << "failed to get present queue: " << pq.error().message() << "\n";
    }
    m_PresentQueue = pq.value();

    auto tq = m_Device.get_dedicated_queue(vkb::QueueType::transfer);
    if (!tq.has_value()) {
        std::cout << "Failed to get dedicated transfer queue: " << pq.error().message() << "\n";

    }
    m_TranferQueue = tq.value();
}

void VulkanCore::CreateCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_Device.get_queue_index(vkb::QueueType::graphics).value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(m_Disp.createCommandPool(&poolInfo, nullptr, &m_GraphicPool));

    poolInfo.queueFamilyIndex = m_Device.get_dedicated_queue_index(vkb::QueueType::transfer).value();;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    VK_CHECK(m_Disp.createCommandPool(&poolInfo, nullptr, &m_TransferPool));
}

void VulkanCore::CreateVmaAllocator()
{
    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorCreateInfo.physicalDevice = m_Device.physical_device;
    allocatorCreateInfo.device = m_Device;
    allocatorCreateInfo.instance = m_Instance;
    allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    VK_CHECK(vmaCreateAllocator(&allocatorCreateInfo, &m_Allocator)); 
}

void VulkanCore::CompileShader(const std::string& spv_path)
{
    std::string shaderName = spv_path.substr(spv_path.find_last_of("\\"));

    std::string command = ".\\glslc.exe " + spv_path + " -o " + ".\\Shader\\Compiled_SPV\\" + shaderName + ".spv";

    std::system(command.c_str());
}

VkShaderModule VulkanCore::createModule(const std::string& spv_path)
{
    std::ifstream file(spv_path, std::ios::binary);

    std::string fileString = "";

    if (file)
    {
        std::stringstream strStream;
        strStream << file.rdbuf();

        fileString = strStream.str();

        file.close();


        VkShaderModuleCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = fileString.size();
        create_info.pCode = reinterpret_cast<const uint32_t*>(fileString.data());

        VkShaderModule shaderModule;
        VK_CHECK(m_Disp.createShaderModule(&create_info, nullptr, &shaderModule));

        return shaderModule;
    }

    return VK_NULL_HANDLE;
}

void VulkanCore::CreateGraphicPipeline()
{
    CompileShader(".\\Shader\\Vertex\\Shader0.vert");
    CompileShader(".\\Shader\\Frag\\Shader0.frag");

    VkShaderModule vertexShaderModule = createModule(".\\Shader\\Compiled_SPV\\Shader0.vert.spv");
    VkShaderModule fragmentShaderModule = createModule(".\\Shader\\Compiled_SPV\\Shader0.frag.spv");

    if (vertexShaderModule == VK_NULL_HANDLE || fragmentShaderModule == VK_NULL_HANDLE)
    {
        std::cout << "Failed to create shaderModule !\n";
        return;
    }

    VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo;
    vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageCreateInfo.pNext = NULL;
    vertexShaderStageCreateInfo.flags = 0;
    vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStageCreateInfo.module = vertexShaderModule;
    vertexShaderStageCreateInfo.pName = "main";
    vertexShaderStageCreateInfo.pSpecializationInfo = NULL;

    VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo;
    fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStageCreateInfo.pNext = NULL;
    fragmentShaderStageCreateInfo.flags = 0;
    fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderStageCreateInfo.module = fragmentShaderModule;
    fragmentShaderStageCreateInfo.pName = "main";
    fragmentShaderStageCreateInfo.pSpecializationInfo = NULL;

    std::array<VkPipelineShaderStageCreateInfo, 2> pipelineShaderStageCreateInfos = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

    VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo;
    pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipelineVertexInputStateCreateInfo.pNext = NULL;
    pipelineVertexInputStateCreateInfo.flags = 0;
    pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
    pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = NULL;
    pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
    pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = NULL;

    VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo;
    pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipelineInputAssemblyStateCreateInfo.pNext = NULL;
    pipelineInputAssemblyStateCreateInfo.flags = 0;
    pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    VkPipelineTessellationStateCreateInfo pipelineTessellationStateCreateInfo;
    pipelineTessellationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    pipelineTessellationStateCreateInfo.pNext = NULL;
    pipelineTessellationStateCreateInfo.flags = 0;
    pipelineTessellationStateCreateInfo.patchControlPoints = 3;

    auto extent = m_Swapchain.extent;

    VkViewport viewport;
    viewport.x = 0.0;
    viewport.y = static_cast<float>(extent.height);
    viewport.width = static_cast<float>(extent.width);
    viewport.height = -static_cast<float>(extent.height);
    viewport.minDepth = 0.0;
    viewport.maxDepth = 1.0;

    VkRect2D scissor;
    scissor.offset = { 0, 0 };
    scissor.extent = extent;

    VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo;
    pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    pipelineViewportStateCreateInfo.pNext = NULL;
    pipelineViewportStateCreateInfo.flags = 0;
    pipelineViewportStateCreateInfo.viewportCount = 1;
    pipelineViewportStateCreateInfo.pViewports = &viewport;
    pipelineViewportStateCreateInfo.scissorCount = 1;
    pipelineViewportStateCreateInfo.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo;
    pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pipelineRasterizationStateCreateInfo.pNext = NULL;
    pipelineRasterizationStateCreateInfo.flags = 0;
    pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    pipelineRasterizationStateCreateInfo.depthBiasConstantFactor = 0.0;
    pipelineRasterizationStateCreateInfo.depthBiasClamp = 0.0;
    pipelineRasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0;
    pipelineRasterizationStateCreateInfo.lineWidth = 1.0;

    VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo;
    pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipelineMultisampleStateCreateInfo.pNext = NULL;
    pipelineMultisampleStateCreateInfo.flags = 0;
    pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipelineMultisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    pipelineMultisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
    pipelineMultisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;
    pipelineMultisampleStateCreateInfo.minSampleShading = 1.0f; 
    pipelineMultisampleStateCreateInfo.pSampleMask = nullptr; 

    VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo;
    pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    pipelineDepthStencilStateCreateInfo.pNext = NULL;
    pipelineDepthStencilStateCreateInfo.flags = 0;
    pipelineDepthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
    pipelineDepthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
    pipelineDepthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    pipelineDepthStencilStateCreateInfo.minDepthBounds = 0.0;
    pipelineDepthStencilStateCreateInfo.maxDepthBounds = 1.0;
    pipelineDepthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
    pipelineDepthStencilStateCreateInfo.front = {};
    pipelineDepthStencilStateCreateInfo.back = {};

    VkPipelineColorBlendAttachmentState pPipelineColorBlendAttachmentState;
    pPipelineColorBlendAttachmentState.blendEnable = VK_FALSE;
    pPipelineColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    pPipelineColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    pPipelineColorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    pPipelineColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    pPipelineColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    pPipelineColorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
    pPipelineColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo;
    pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    pipelineColorBlendStateCreateInfo.pNext = NULL;
    pipelineColorBlendStateCreateInfo.flags = 0;
    pipelineColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    pipelineColorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    pipelineColorBlendStateCreateInfo.attachmentCount = 1;
    pipelineColorBlendStateCreateInfo.pAttachments = &pPipelineColorBlendAttachmentState;
    pipelineColorBlendStateCreateInfo.blendConstants[0] = 0.0f;
    pipelineColorBlendStateCreateInfo.blendConstants[1] = 0.0f;
    pipelineColorBlendStateCreateInfo.blendConstants[2] = 0.0f;
    pipelineColorBlendStateCreateInfo.blendConstants[3] = 0.0f;

    std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo;
    pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    pipelineDynamicStateCreateInfo.pNext = NULL;
    pipelineDynamicStateCreateInfo.flags = 0;
    pipelineDynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

    if (m_GraphicPipelineLayout == VK_NULL_HANDLE)
    {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.pNext = NULL;
        pipelineLayoutCreateInfo.flags = 0;
        pipelineLayoutCreateInfo.setLayoutCount = 0;
        pipelineLayoutCreateInfo.pSetLayouts = NULL;
        pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
        pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

        VK_CHECK(m_Disp.createPipelineLayout(&pipelineLayoutCreateInfo, NULL, &m_GraphicPipelineLayout));
    }

    const auto format = RT_IMAGE_FORMAT;

    VkPipelineRenderingCreateInfoKHR pipeline_create{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR };
    pipeline_create.pNext = VK_NULL_HANDLE;
    pipeline_create.colorAttachmentCount = 1;
    pipeline_create.pColorAttachmentFormats = &format;
    pipeline_create.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    pipeline_create.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo;
    graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphicsPipelineCreateInfo.pNext = &pipeline_create;
    graphicsPipelineCreateInfo.flags = 0;
    graphicsPipelineCreateInfo.stageCount = static_cast<uint32_t>(pipelineShaderStageCreateInfos.size());
    graphicsPipelineCreateInfo.pStages = pipelineShaderStageCreateInfos.data();
    graphicsPipelineCreateInfo.pVertexInputState = &pipelineVertexInputStateCreateInfo;
    graphicsPipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
    graphicsPipelineCreateInfo.pTessellationState = &pipelineTessellationStateCreateInfo;
    graphicsPipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
    graphicsPipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
    graphicsPipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
    graphicsPipelineCreateInfo.pDepthStencilState = &pipelineDepthStencilStateCreateInfo;
    graphicsPipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
    graphicsPipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
    graphicsPipelineCreateInfo.layout = m_GraphicPipelineLayout;
    graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
    graphicsPipelineCreateInfo.subpass = 0;
    graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    graphicsPipelineCreateInfo.basePipelineIndex = -1;

    VK_CHECK(m_Disp.createGraphicsPipelines(VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, NULL, &m_GraphicPipeline));

    m_Disp.destroyShaderModule(vertexShaderModule, nullptr);
    m_Disp.destroyShaderModule(fragmentShaderModule, nullptr);
}

void VulkanCore::CreateCommandBuffer()
{
    m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = NULL;
    commandBufferAllocateInfo.commandPool = m_GraphicPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>(m_CommandBuffers.size());

    VK_CHECK(m_Disp.allocateCommandBuffers(&commandBufferAllocateInfo, m_CommandBuffers.data()));
}

void VulkanCore::CreateSyncObject()
{
    m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(m_Disp.createSemaphore(&semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]));
        VK_CHECK(m_Disp.createSemaphore(&semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]));
        VK_CHECK(m_Disp.createFence(&fenceInfo, nullptr, &m_InFlightFences[i]));
    }
}

void VulkanCore::CreateRenderTarget(uint32_t width, uint32_t height)
{
    m_RTImages.resize(MAX_FRAMES_IN_FLIGHT);
    m_ImGuiDescriptors.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.extent.width = width;
        imageCreateInfo.extent.height = height;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.format = RT_IMAGE_FORMAT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.flags = 0;
        imageCreateInfo.queueFamilyIndexCount = 1;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        uint32_t graphicQueueIndex = m_Device.get_queue_index(vkb::QueueType::graphics).value();
        imageCreateInfo.pQueueFamilyIndices = &graphicQueueIndex;

        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        allocCreateInfo.priority = 1.0f;

        if (vmaCreateImage(m_Allocator, &imageCreateInfo, &allocCreateInfo, &m_RTImages[i].Image, &m_RTImages[i].ImageAllocation, nullptr) != VK_SUCCESS) {
            std::cout << "Image creation failed !" << std::endl;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_RTImages[i].Image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = RT_IMAGE_FORMAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(m_Disp.createImageView(&viewInfo, nullptr, &m_RTImages[i].ImageView));


        m_ImGuiDescriptors[i] = ImGui_ImplVulkan_AddTexture(m_ImGuiSampler, m_RTImages[i].ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    m_LastTime = std::chrono::high_resolution_clock::now();
}

void VulkanCore::RecreateRenderTarget(uint32_t width, uint32_t height)
{
    m_Disp.deviceWaitIdle();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        m_RTImages[i].Clenup(m_Allocator, m_Disp);
        ImGui_ImplVulkan_RemoveTexture(m_ImGuiDescriptors[i]);
    }

    m_ImGuiDescriptors.clear();
    m_RTImages.clear();

    CreateRenderTarget(width, height);
}

void VulkanCore::InitImGui()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    m_io = &ImGui::GetIO();
    m_io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    m_io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    m_io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking

    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 30 },
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 30;
    pool_info.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;
    m_Disp.createDescriptorPool(&pool_info, NULL, & m_ImGuiDescriptorPool);

    ImGui::StyleColorsDark();

    VkPipelineRenderingCreateInfoKHR pipeline_rendering_create_info{};
    pipeline_rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipeline_rendering_create_info.colorAttachmentCount = 1;
    pipeline_rendering_create_info.pColorAttachmentFormats = &m_Swapchain.image_format;
    pipeline_rendering_create_info.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    pipeline_rendering_create_info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    ImGui_ImplGlfw_InitForVulkan(m_Window->getWindow(), true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_Instance;
    init_info.PhysicalDevice = m_Device.physical_device;
    init_info.Device = m_Device;
    init_info.QueueFamily = m_Device.get_queue_index(vkb::QueueType::graphics).value();
    init_info.Queue = m_GraphicsQueue;
    init_info.PipelineCache = NULL;
    init_info.DescriptorPool = m_ImGuiDescriptorPool;
    init_info.RenderPass = VK_NULL_HANDLE;
    init_info.Subpass = 0;
    init_info.MinImageCount = m_Swapchain.requested_min_image_count;
    init_info.ImageCount = m_Swapchain.image_count;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = NULL;
    init_info.CheckVkResultFn = NULL;
    init_info.UseDynamicRendering = true;
    init_info.PipelineRenderingCreateInfo = pipeline_rendering_create_info;
    ImGui_ImplVulkan_Init(&init_info);

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // outside image bounds just use border color
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.minLod = -1000;
    sampler_info.maxLod = 1000;
    sampler_info.maxAnisotropy = 1.0f;

    VK_CHECK(m_Disp.createSampler(&sampler_info, nullptr, &m_ImGuiSampler));
}

void VulkanCore::RecordCommandBuffer(uint32_t imageIndex, ImDrawData* imGui_draw_data)
{
    VkCommandBufferBeginInfo beginInfo;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = NULL;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = NULL; //ignored if VK_COMMAND_BUFFER_LEVEL_PRIMARY

    VK_CHECK(m_Disp.beginCommandBuffer(m_CommandBuffers[m_CurrentFrame], &beginInfo));

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_RTImages[m_CurrentFrame].Image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(
        m_CommandBuffers[m_CurrentFrame],
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    VkRenderingAttachmentInfoKHR color_attachment_info;
    color_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    color_attachment_info.pNext = nullptr;
    color_attachment_info.imageView = m_RTImages[m_CurrentFrame].ImageView;
    color_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
    color_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment_info.clearValue = { { { 0.0f, 0.0f, 0.0f, 1.0f } } };
    color_attachment_info.resolveMode = VK_RESOLVE_MODE_NONE;
    color_attachment_info.resolveImageView = VK_NULL_HANDLE;
    color_attachment_info.resolveImageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkExtent2D ImageExtent = { m_RTWidth, m_RTHeight };

    VkRenderingInfoKHR render_info;
    render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    render_info.pNext = nullptr;
    render_info.flags = 0;
    render_info.renderArea.offset = { 0, 0 };
    render_info.renderArea.extent = ImageExtent;
    render_info.layerCount = 1;
    render_info.viewMask = 0;
    render_info.colorAttachmentCount = 1;
    render_info.pColorAttachments = &color_attachment_info;
    render_info.pDepthAttachment = nullptr;
    render_info.pStencilAttachment = nullptr;

    m_Disp.cmdBeginRendering(m_CommandBuffers[m_CurrentFrame], &render_info);

    VkViewport viewport;
    viewport.x = 0.0;
    viewport.y = 0.0;
    viewport.width = static_cast<float>(ImageExtent.width);
    viewport.height = static_cast<float>(ImageExtent.height);
    viewport.minDepth = 0.0;
    viewport.maxDepth = 1.f;

    m_Disp.cmdSetViewport(m_CommandBuffers[m_CurrentFrame], 0, 1, &viewport);

    VkRect2D scissor;
    scissor.offset = { 0, 0 };
    scissor.extent = ImageExtent;

    m_Disp.cmdSetScissor(m_CommandBuffers[m_CurrentFrame], 0, 1, &scissor);

    m_Disp.cmdBindPipeline(m_CommandBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicPipeline);

    PushConstants pc{};
    GetPushConstant(pc);

    vkCmdPushConstants(
        m_CommandBuffers[m_CurrentFrame],
        m_GraphicPipelineLayout,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(PushConstants),
        &pc
    );

    m_Disp.cmdDraw(m_CommandBuffers[m_CurrentFrame], 3, 1, 0, 0);

    m_Disp.cmdEndRendering(m_CommandBuffers[m_CurrentFrame]);

    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_RTImages[m_CurrentFrame].Image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        m_CommandBuffers[m_CurrentFrame],
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    //ImGui Render

    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_SwapchainImages[imageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(
        m_CommandBuffers[m_CurrentFrame],
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    color_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    color_attachment_info.pNext = nullptr;
    color_attachment_info.imageView = m_SwapchainImageViews[imageIndex];
    color_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
    color_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment_info.clearValue = { { { 0.0f, 0.0f, 0.0f, 1.0f } } };
    color_attachment_info.resolveMode = VK_RESOLVE_MODE_NONE;
    color_attachment_info.resolveImageView = VK_NULL_HANDLE;
    color_attachment_info.resolveImageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkExtent2D swapchainExtent = m_Swapchain.extent;

    render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    render_info.pNext = nullptr;
    render_info.flags = 0;
    render_info.renderArea.offset = { 0, 0 };
    render_info.renderArea.extent = swapchainExtent;
    render_info.layerCount = 1;
    render_info.viewMask = 0;
    render_info.colorAttachmentCount = 1;
    render_info.pColorAttachments = &color_attachment_info;
    render_info.pDepthAttachment = nullptr;
    render_info.pStencilAttachment = nullptr;

    m_Disp.cmdBeginRendering(m_CommandBuffers[m_CurrentFrame], &render_info);

    ImGui_ImplVulkan_RenderDrawData(imGui_draw_data, m_CommandBuffers[m_CurrentFrame]);

    m_Disp.cmdEndRendering(m_CommandBuffers[m_CurrentFrame]);

    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_SwapchainImages[imageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = 0;

    vkCmdPipelineBarrier(
        m_CommandBuffers[m_CurrentFrame],
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    VK_CHECK(m_Disp.endCommandBuffer(m_CommandBuffers[m_CurrentFrame]));
}

void VulkanCore::ReloadShader()
{
    m_Disp.deviceWaitIdle();

    m_Disp.destroyPipeline(m_GraphicPipeline, nullptr);

    CreateGraphicPipeline();
}

void VulkanCore::Draw()
{
    m_Disp.waitForFences(1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;

    VkResult result = m_Disp.acquireNextImageKHR(m_Swapchain, UINT64_MAX, 
        m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS) {
        std::cout << "Failed to acquire next image !" << '\n';
        return;
    }

    VK_CHECK(m_Disp.resetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0));

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (m_Playing)
    {
        auto currentTime = std::chrono::high_resolution_clock::now();
        m_DeltaTime = std::chrono::duration<float>(currentTime - m_LastTime).count();
        m_LastTime = currentTime;
        m_SimulationTime += m_DeltaTime;

        const float alpha = 0.005f;
        m_fps = m_fps < 0.f ? (1.f / m_DeltaTime) : (1.f / m_DeltaTime) * alpha + (1.f - alpha) * m_fps;
    }

    {
        ImGui::DockSpaceOverViewport();
    }

    /*
    {
        m_ImGuiGlslEditor.Draw();
    }
    */

    {
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoNavInputs;

        ImGui::Begin("Scene", nullptr, window_flags);
        {
            ImVec2 ImageRegionAvail = ImGui::GetContentRegionAvail();

            std::string text = std::format("{:.2f}", static_cast<float>(m_SimulationTime))
                + "\t" + std::format("{:.1f}", m_fps) + " fps"
                + "\t" + std::to_string(m_RTWidth) + "x" + std::to_string(m_RTHeight);

            const char* buttonRestartText = "Restart";
            const char* buttonPlayText = m_Playing ? "Stop" : "Play";
            const char* butttonRecompileText = "Recompile";

            ImGuiStyle& style = ImGui::GetStyle();

            ImVec2 FramePadding = ImVec2(style.FramePadding.x * 2.0f, style.FramePadding.y * 2.0f);
            float buttonRestartY = ImGui::CalcTextSize(buttonRestartText).y + FramePadding.y * 2.0f + style.ItemSpacing.y;
            float buttonStartSizeY = ImGui::CalcTextSize(buttonPlayText).y + FramePadding.y * 2.0f + style.ItemSpacing.y;
            float textSizeY = ImGui::CalcTextSize(text.c_str()).y + style.ItemSpacing.y;
            ImVec2 buttonRecompileSize = ImGui::CalcTextSize(butttonRecompileText);
            buttonRecompileSize.x += FramePadding.x;
            buttonRecompileSize.y += FramePadding.y + style.ItemSpacing.y;

            ImageRegionAvail.y -= std::max(std::max(std::max(textSizeY, buttonStartSizeY), buttonRestartY), buttonRecompileSize.y);

            ImGui::BeginChild("Render", ImVec2(0, ImageRegionAvail.y), false, window_flags);

            if (m_RTWidth != ImageRegionAvail.x || m_RTHeight != ImageRegionAvail.y)
            {
                m_RTWidth = static_cast<uint32_t>(std::max(ImageRegionAvail.x, 1.f));
                m_RTHeight = static_cast<uint32_t>(std::max(ImageRegionAvail.y, 1.f));

                RecreateRenderTarget(m_RTWidth, m_RTHeight);
            }

            ImVec2 offset = ImGui::GetCursorScreenPos();
            ImGui::ImageButton(
                (ImTextureID)m_ImGuiDescriptors[m_CurrentFrame],
                ImageRegionAvail,
                ImVec2(0, 0),
                ImVec2(1, 1),
                0
            );

            ImVec2 MousePos = ImGui::GetMousePos();

            if (offset.x < MousePos.x && MousePos.x < offset.x + ImageRegionAvail.x && 
                offset.y < MousePos.y && MousePos.y < offset.y + ImageRegionAvail.y && 
                ImGui::IsAnyMouseDown())
            {
                m_CurrentMousePose = glm::vec2(MousePos.x - offset.x, ImageRegionAvail.y - (MousePos.y - offset.y));

                if (!m_MouseDown)
                {
                    m_LastClickMousePose = m_CurrentMousePose;
                    m_MouseDown = true;
                }
            }
            else
            {
                m_MouseDown = false;
            }

            ImGui::EndChild();

            ImGui::BeginChild("RenderBar", ImVec2(0, 0), false, window_flags);

            if (ImGui::Button(buttonRestartText))
            {
                m_SimulationTime = 0.;
            }
            ImGui::SameLine(0.0f, 10.0f);
            if (ImGui::Button(buttonPlayText))
            {
                m_Playing = !m_Playing;
                if (m_Playing)
                    m_LastTime = std::chrono::high_resolution_clock::now();
            }
            ImGui::SameLine(0.0f, 30.0f);
            ImGui::Text(text.c_str());
            ImGui::SameLine(ImageRegionAvail.x - buttonRecompileSize.x);
            if (ImGui::Button(butttonRecompileText))
            {
                ReloadShader();
                m_SimulationTime = 0.;
            }
       
            ImGui::EndChild();
            
        }
        ImGui::End();
    }

    ImGui::Render();
    ImDrawData* main_draw_data = ImGui::GetDrawData();

    RecordCommandBuffer(imageIndex, main_draw_data);

    VkSemaphore waitSemaphores[] = { m_ImageAvailableSemaphores[m_CurrentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[m_CurrentFrame] };

    VkSubmitInfo submitsInfo;
    submitsInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitsInfo.pNext = NULL;
    submitsInfo.waitSemaphoreCount = 1;
    submitsInfo.pWaitSemaphores = waitSemaphores;
    submitsInfo.pWaitDstStageMask = waitStages;
    submitsInfo.commandBufferCount = 1;
    submitsInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentFrame];
    submitsInfo.signalSemaphoreCount = 1;
    submitsInfo.pSignalSemaphores = signalSemaphores;

    m_Disp.resetFences(1, &m_InFlightFences[m_CurrentFrame]);

    VK_CHECK(m_Disp.queueSubmit(m_GraphicsQueue, 1, &submitsInfo, m_InFlightFences[m_CurrentFrame]));

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { m_Swapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr; // Optionnel

    result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        RecreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS) {
        std::cout << "Failed to present image!" << '\n';
        return;
    }

    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

VulkanCore::~VulkanCore()
{
    m_Disp.deviceWaitIdle();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_Disp.destroySemaphore(m_ImageAvailableSemaphores[i], nullptr);
        m_Disp.destroySemaphore(m_RenderFinishedSemaphores[i], nullptr);
        m_Disp.destroyFence(m_InFlightFences[i], nullptr);
    }

    m_Disp.destroyCommandPool(m_GraphicPool, nullptr);
    m_Disp.destroyCommandPool(m_TransferPool, nullptr);

    m_Disp.destroyPipeline(m_GraphicPipeline, nullptr);
    m_Disp.destroyPipelineLayout(m_GraphicPipelineLayout, nullptr);

    m_Swapchain.destroy_image_views(m_SwapchainImageViews);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        m_RTImages[i].Clenup(m_Allocator, m_Disp);
    }

    m_Disp.destroySampler(m_ImGuiSampler, nullptr);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    vmaDestroyAllocator(m_Allocator);

    m_Disp.destroyDescriptorPool(m_ImGuiDescriptorPool, nullptr);

    vkb::destroy_swapchain(m_Swapchain);
    vkb::destroy_device(m_Device);
    vkb::destroy_surface(m_Instance, m_Surface);
    vkb::destroy_instance(m_Instance);
}

void VulkanCore::GetPushConstant(PushConstants& pushConstant)
{
    pushConstant.iResolution = glm::vec3(m_RTWidth, m_RTHeight, 1.f);
    pushConstant.iFrame = m_FrameCount;
    pushConstant.iFrameRate = m_fps;
    pushConstant.iTime = static_cast<float>(m_SimulationTime);
    pushConstant.iTimeDelta = m_DeltaTime;
    pushConstant.iMouse = glm::vec4(m_CurrentMousePose, (m_MouseDown ? 1.f : -1.f) * m_LastClickMousePose.x, -m_LastClickMousePose.y);
    pushConstant.iDate = glm::vec4(0.f);
}
