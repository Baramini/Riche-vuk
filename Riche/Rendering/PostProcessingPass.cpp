#include "PostProcessingPass.h"
#include "BasicLightingPass.h"
#include "CullingRenderPass.h"

void PostProcessingPass::Init(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D extent,
                              const std::vector<VkImageView>& swapchainImageViews, BasicLightingPass* lightingPass,
                              CullingRenderPass* shadowPass, uint32_t maxFramesInFlight) {
  m_device = device;
  m_physicalDevice = physicalDevice;
  m_extent = extent;
  m_maxFrames = maxFramesInFlight;
  m_swapchainImageViews = swapchainImageViews;

  m_pLightingPass = lightingPass;
  m_pShadowPass = shadowPass;

  CreateRenderPass();
  CreateFramebuffers();
  CreateDescriptorSets();
  CreatePipeline();
}

void PostProcessingPass::Cleanup(VkDevice device) {
  vkDestroyPipeline(device, m_pipeline, nullptr);
  vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
  vkDestroyRenderPass(device, m_renderPass, nullptr);

  for (auto& fb : m_framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
}

void PostProcessingPass::CreateRenderPass() {
  VkAttachmentDescription colorAttachment = {};
  colorAttachment.format = VK_FORMAT_B8G8R8A8_UNORM;  // 또는 외부에서 받아온 swapchain format 사용 가능
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
        m_swapchainImageViews[i]  // 외부에서 받은 값 사용
    };

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_renderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = m_extent.width;
    framebufferInfo.height = m_extent.height;
    framebufferInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_framebuffers[i]));
  }
}

void PostProcessingPass::CreateDescriptorSets() {
  m_descriptorSets.resize(m_maxFrames);

  for (uint32_t i = 0; i < m_maxFrames; ++i) {
    // Lighting 텍스처
    VkDescriptorImageInfo inputColour = {};
    inputColour.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inputColour.imageView = m_pLightingPass->GetOutputImageView(i);  // 조명 결과 이미지 뷰
    inputColour.sampler = VK_NULL_HANDLE;

    // Shadow 텍스처
    VkDescriptorImageInfo shadow = {};
    shadow.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    shadow.imageView = m_pShadowPass->GetFrameBufferImageView();  // 그림자 결과 이미지 뷰
    shadow.sampler = VK_NULL_HANDLE;

    // Bloom 텍스처
    VkDescriptorImageInfo bloom = {};
    bloom.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    bloom.imageView = m_pLightingPass->GetBloomImageView(i);  // 이 함수 새로 추가해야 함
    bloom.sampler = VK_NULL_HANDLE;

    // Descriptor Set 생성
    auto builder = VkUtils::DescriptorBuilder::Begin(&g_DescriptorLayoutCache, &g_DescriptorAllocator);

    builder.BindImage(0, &inputColour, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT);  // set 1
    builder.BindImage(1, &shadow, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT);       // set 2
    builder.BindImage(2, &bloom, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT);        // set 3

    g_DescriptorManager.AddDescriptorSet(&builder, "PostProcessInput" + std::to_string(i));
    m_descriptorSets[i] = g_DescriptorManager.GetVkDescriptorSet("PostProcessInput" + std::to_string(i));
  }
}

void PostProcessingPass::CreatePipeline() {
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

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkViewport viewport = {};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(m_extent.width);
  viewport.height = static_cast<float>(m_extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor = {};
  scissor.offset = {0, 0};
  scissor.extent = m_extent;

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisampling = {};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  // Descriptor Layouts (Sampler + Textures)
  std::vector<VkDescriptorSetLayout> setLayouts = {g_DescriptorManager.GetVkDescriptorSetLayout("SamplerList_ALL"),
                                                   g_DescriptorManager.GetVkDescriptorSetLayout("PostProcessInput0")};

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
  pipelineLayoutInfo.pSetLayouts = setLayouts.data();

  VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout));

  VkGraphicsPipelineCreateInfo pipelineInfo = {};
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

  vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
  vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
}

void PostProcessingPass::RecordCommands(VkCommandBuffer cmd, uint32_t frameIndex) {
  // -- Begin Render Pass --
  VkClearValue clearColor{};
  clearColor.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

  VkRenderPassBeginInfo renderPassInfo = {};
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

  // -- Draw Fullscreen Triangle --
  vkCmdDraw(cmd, 3, 1, 0, 0);  // full-screen triangle

  vkCmdEndRenderPass(cmd);
}
