#include "PostProcessingPass.h"
#include "BasicLightingPass.h"
#include "CullingRenderPass.h"

void PostProcessingPass::Init(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D extent,
                              const std::vector<VkImageView>& swapchainImageViews, BasicLightingPass* lightingPass,
                              CullingRenderPass* shadowPass, VkFormat swapchainFormat, uint32_t maxFramesInFlight) {
  m_device = device;
  m_physicalDevice = physicalDevice;
  m_extent = extent;
  m_maxFrames = maxFramesInFlight;
  m_swapchainImageViews = swapchainImageViews;
  m_pSwapchainFormat = swapchainFormat;

  m_pLightingPass = lightingPass;
  m_pShadowPass = shadowPass;

  CreateRenderPass();
  CreateFramebuffers();
  CreateDescriptorSets();
  CreatePipeline();
}

void PostProcessingPass::Cleanup() {
  /*
  // 파이프라인 및 레이아웃 제거
  if (m_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(m_device, m_pipeline, nullptr);

  if (m_pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

  // 렌더 패스 제거
  if (m_renderPass != VK_NULL_HANDLE) vkDestroyRenderPass(m_device, m_renderPass, nullptr);

  // 프레임버퍼 제거
  for (auto& fb : m_framebuffers) {
    if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(m_device, fb, nullptr);
  }
  */
}

void PostProcessingPass::CreateRenderPass() {
  VkAttachmentDescription colorAttachment = {};
  colorAttachment.format = m_pSwapchainFormat;  // 또는 외부에서 받아온 swapchain format 사용 가능
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef = {};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  std::array<VkSubpassDependency, 2> dependencies{};

  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[0].dependencyFlags = 0;

  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  dependencies[1].dependencyFlags = 0;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  renderPassInfo.pDependencies = dependencies.data();

  VK_CHECK(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass));
}

void PostProcessingPass::CreateFramebuffers() {
  m_framebuffers.resize(m_maxFrames);

  for (uint32_t i = 0; i < m_maxFrames; ++i) {
    VkImageView attachments[] = {
        m_swapchainImageViews[i]  // 외부에서 받은 Swapchain ImageView
    };

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_renderPass;  // PostProcessing 전용 렌더패스
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = m_extent.width;
    framebufferInfo.height = m_extent.height;
    framebufferInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_framebuffers[i]));

    std::cout << "[PostProcessingPass] Framebuffer " << i << " created successfully." << std::endl;
  }
}


void PostProcessingPass::CreateDescriptorSets() {
  m_descriptorSets.resize(m_maxFrames);

  for (uint32_t i = 0; i < m_maxFrames; ++i) {
    // Lighting 텍스처
    VkDescriptorImageInfo inputColour = {};
    inputColour.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inputColour.imageView = m_pLightingPass->GetOutputImageView(i);
    inputColour.sampler = VK_NULL_HANDLE;

    // Shadow 텍스처
    VkDescriptorImageInfo shadow = {};
    shadow.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    shadow.imageView = m_pShadowPass->GetFrameBufferImageView();
    shadow.sampler = VK_NULL_HANDLE;

    // Bloom 텍스처
    VkDescriptorImageInfo bloom = {};
    bloom.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    bloom.imageView = m_pLightingPass->GetBloomImageView(i);
    bloom.sampler = VK_NULL_HANDLE;

    // Descriptor Set 생성
    auto builder = VkUtils::DescriptorBuilder::Begin(&g_DescriptorLayoutCache, &g_DescriptorAllocator);

    builder.BindImage(0, &inputColour, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.BindImage(1, &shadow, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.BindImage(2, &bloom, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout layout = g_DescriptorManager.GetVkDescriptorSetLayout("PostProcessInput");

    builder.Build(descriptorSet, layout);  // 이미 만들어둔 공통 레이아웃 사용

    // DescriptorSet 등록만 수행
    std::string name = "PostProcessInput" + std::to_string(i);
    g_DescriptorManager.RegisterSet(name, descriptorSet);

    m_descriptorSets[i] = descriptorSet;
  }
}


void PostProcessingPass::CreatePipeline() {
  if (!g_DescriptorManager.HasDescriptorSetLayout("PostProcessInput")) {
    std::cerr << "[ERROR] PostProcessInput DescriptorSetLayout not found!" << std::endl;
  }

  // 1. Shader Modules
  auto vertShaderCode = VkUtils::ReadFile("Resources/Shaders/PostProcessingVS.spv");
  auto fragShaderCode = VkUtils::ReadFile("Resources/Shaders/PostProcessingPS.spv");

  VkShaderModule vertShaderModule = VkUtils::CreateShaderModule(m_device, vertShaderCode);
  VkShaderModule fragShaderModule = VkUtils::CreateShaderModule(m_device, fragShaderCode);

  VkPipelineShaderStageCreateInfo shaderStages[2] = {};
  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = vertShaderModule;
  shaderStages[0].pName = "main";

  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = fragShaderModule;
  shaderStages[1].pName = "main";

  // 2. Fixed Function Stages
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(m_extent.width);
  viewport.height = static_cast<float>(m_extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = m_extent;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  // 3. Descriptor Set Layouts
  std::vector<VkDescriptorSetLayout> setLayouts = {g_DescriptorManager.GetVkDescriptorSetLayout("SamplerList_ALL"),
                                                   g_DescriptorManager.GetVkDescriptorSetLayout("PostProcessInput")};

  // 4. Push Constant Range
  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(PostFXPushConstant);

  // 5. Pipeline Layout
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
  pipelineLayoutInfo.pSetLayouts = setLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

  VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout));

  // 6. Graphics Pipeline
  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.layout = m_pipelineLayout;
  pipelineInfo.renderPass = m_renderPass;
  pipelineInfo.subpass = 0;

  VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline));

  // 7. Clean Up Shader Modules
  vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
  vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
}

void PostProcessingPass::RecordCommands(VkCommandBuffer cmd, uint32_t frameIndex) {
  // -- Begin Render Pass --
  VkClearValue clearColor{};
  clearColor.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = m_renderPass;
  renderPassInfo.framebuffer = m_framebuffers[frameIndex];
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = m_extent;
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearColor;

  vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  // -- Bind Pipeline --
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

  // -- Bind Descriptor Sets --
  VkDescriptorSet sets[] = {g_DescriptorManager.GetVkDescriptorSet("SamplerList_ALL"), m_descriptorSets[frameIndex]};
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 2, sets, 0, nullptr);

  // -- Push Constants (Enable/Disable Bloom) --
  PostFXPushConstant pushConstant{};
  pushConstant.isEnableBloom = g_RenderSetting.isEnableBloom ? 1 : 0;
  vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PostFXPushConstant), &pushConstant);

  // -- Draw Fullscreen Triangle --
  vkCmdDraw(cmd, 3, 1, 0, 0);  // full-screen triangle

  vkCmdEndRenderPass(cmd);
}
