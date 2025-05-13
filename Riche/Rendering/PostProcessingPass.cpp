#include "PostProcessingPass.h"
#include "BasicLightingPass.h"
#include "VkUtils/DescriptorAllocator.h"
#include "VkUtils/DescriptorBuilder.h"
#include "VkUtils/DescriptorLayoutCache.h"
#include "VkUtils/ResourceManager.h"

void PostProcessingPass::Init(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D extent,
                              const std::vector<VkImageView>& swapchainImageViews, BasicLightingPass* lightingPass,
                              CullingRenderPass* /*shadowPass*/,  // unused
                              VkFormat swapchainFormat, uint32_t maxFramesInFlight) {
  m_device = device;
  m_physicalDevice = physicalDevice;
  m_extent = extent;
  m_swapchainImageViews = swapchainImageViews;
  m_pLightingPass = lightingPass;
  m_swapchainFormat = swapchainFormat;
  m_maxFrames = maxFramesInFlight;

  CreateDescriptorSetLayout();  // registers "PostProcessInput"
  CreateRenderPass();
  CreatePipeline();
  CreateFramebuffers();
  CreateDescriptorSets();
}

void PostProcessingPass::Cleanup() {
  for (auto fb : m_framebuffers) {
    vkDestroyFramebuffer(m_device, fb, nullptr);
  }
  vkDestroyPipeline(m_device, m_pipeline, nullptr);
  vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
  vkDestroyRenderPass(m_device, m_renderPass, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

  // Unregister sets/layout from DescriptorManager
  for (uint32_t i = 0; i < m_swapchainImageViews.size(); ++i) {
    g_DescriptorManager.UnregisterSet("PostProcessInput" + std::to_string(i));
  }
  g_DescriptorManager.UnregisterLayout("PostProcessInput");
}

void PostProcessingPass::RecordCommands(VkCommandBuffer cmd, uint32_t frameIndex) {
  //VkClearValue clearColor{{0.0f, 0.0f, 0.0f, 1.0f}};
  //VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  //rpbi.renderPass = m_renderPass;
  //rpbi.framebuffer = m_framebuffers[frameIndex];
  //rpbi.renderArea.extent = m_extent;
  //rpbi.clearValueCount = 1;
  //rpbi.pClearValues = &clearColor;
  //vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
  auto descSet = g_DescriptorManager.GetVkDescriptorSet("PostProcessInput" + std::to_string(frameIndex));
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &descSet, 0, nullptr);

  PostFXPushConstant pc;
  pc.isEnableBloom = g_RenderSetting.isEnableBloom;
  pc.padding = 0.0f;  // 반드시 채워줄 것
  pc.texelSize = glm::vec2(1.0f / m_extent.width, 1.0f / m_extent.height);
  vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

  vkCmdDraw(cmd, 3, 1, 0, 0);

  //vkCmdEndRenderPass(cmd);
}

void PostProcessingPass::CreateDescriptorSetLayout() {
  VkDescriptorSetLayoutBinding bindings[2] = {};
  bindings[0] = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT};
  bindings[1] = {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT};

  VkDescriptorSetLayoutCreateInfo dslci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  dslci.bindingCount = 2;
  dslci.pBindings = bindings;
  if (vkCreateDescriptorSetLayout(m_device, &dslci, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create PostProcessInput layout");
  }

  g_DescriptorManager.RegisterLayout("PostProcessInput", m_descriptorSetLayout);
}

void PostProcessingPass::CreateRenderPass() {
  /*VkAttachmentDescription att{};
  att.samples = VK_SAMPLE_COUNT_1_BIT;
  att.format = m_swapchainFormat;
  att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference aref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription sp{};
  sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sp.colorAttachmentCount = 1;
  sp.pColorAttachments = &aref;

  VkSubpassDependency dep{VK_SUBPASS_EXTERNAL,
                          0,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          0,
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};

  VkRenderPassCreateInfo rpc{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  rpc.attachmentCount = 1;
  rpc.pAttachments = &att;
  rpc.subpassCount = 1;
  rpc.pSubpasses = &sp;
  rpc.dependencyCount = 1;
  rpc.pDependencies = &dep;
  if (vkCreateRenderPass(m_device, &rpc, nullptr, &m_renderPass) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create PostProcess render pass");
  }*/

  // Array of Subpasses
  std::array<VkSubpassDescription, 1> subpasses{};

  //
  // ATTACHMENTS
  //
  // SwapChain Colour attacment of render pass
  VkAttachmentDescription swapChainColourAttachment = {};
  swapChainColourAttachment.format = m_swapchainFormat;  // format to use for attachment
  swapChainColourAttachment.samples =
      VK_SAMPLE_COUNT_1_BIT;  // number of samples to write for multisampling, relative to multisampling
  swapChainColourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;             // Describes what to do with attachment before rendering
  swapChainColourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;           // Describes what to do with attachment after rendering
  swapChainColourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // Describes what to do with stencil before rendering
  swapChainColourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // Describes what to do with stencil after rendering

  // FrameBuffer data will be stored as an image, but images can be given different data layouts
  // to give optimal use for certain operations, initial -> subpass -> final
  swapChainColourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;      // Image data layout before render pass starts
  swapChainColourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // Image data layout after render pass (to change to)

  VkAttachmentDescription depthStencilAttachment = {};
  depthStencilAttachment.format = VkUtils::ChooseSupportedFormat(
      m_physicalDevice, {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT},
      VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
  depthStencilAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthStencilAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthStencilAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthStencilAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthStencilAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  // REFERENCES
  // FrameBuffer를 만들 때 사용하였던, attachment 배열을 참조한다고 보면 된다.
  // Attachment reference uses an attachment index that refers to index in the attachment list passed to renderPassCreateInfo
  VkAttachmentReference swapChainColourAttachmentRef = {};
  swapChainColourAttachmentRef.attachment = 0;  // 얼마나 많은 attachment가 있는지 정의X, 몇번째 attachment를 참조하냐의 느낌.
  swapChainColourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthStencilAttachmentRef = {};
  depthStencilAttachmentRef.attachment = 1;
  depthStencilAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  // NOTE: We don't need Input Reference, because we don't use more than two subpasses.
  // Information about a particular subpass the render pass is using
  subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;  // Pipeline type subpass is to be bound to
  subpasses[0].colorAttachmentCount = 1;
  subpasses[0].pColorAttachments = &swapChainColourAttachmentRef;
  subpasses[0].pDepthStencilAttachment = &depthStencilAttachmentRef;

  //
  // SUBPASS DEPENDENCY
  //
  // Need to determine when layout transitions occur using subpass dependencies
  std::array<VkSubpassDependency, 2> subpassDependencies;

  // Conversion from VK_IMAGE_LAYER-UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
  // Transition must happen after ..
  subpassDependencies[0].srcSubpass =
      VK_SUBPASS_EXTERNAL;  // 외부에서 들어오므로, Subpass index(VK_SUBPASS_EXTERNAL = Special value meaning outside of renderpass)
  subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;  // Pipeline stage
  subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;            // Stage access mask (memory access)
  // But most happen before ..
  subpassDependencies[0].dstSubpass = 0;
  subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  subpassDependencies[0].dependencyFlags = 0;

  // Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
  // Transition must happen after ..
  subpassDependencies[1].srcSubpass = 0;
  subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;  // Pipeline stage
  subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  // But most happen before ..
  subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  subpassDependencies[1].dependencyFlags = 0;

  //
  // RENDER PASS CREATE INFO
  //
  std::array<VkAttachmentDescription, 2> renderPassAttachments = {swapChainColourAttachment, depthStencilAttachment};

  // Create info for Render Pass
  VkRenderPassCreateInfo renderPassCreateInfo = {};
  renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
  renderPassCreateInfo.pAttachments = renderPassAttachments.data();
  renderPassCreateInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
  renderPassCreateInfo.pSubpasses = subpasses.data();
  renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
  renderPassCreateInfo.pDependencies = subpassDependencies.data();

  VK_CHECK(vkCreateRenderPass(m_device, &renderPassCreateInfo, nullptr, &m_renderPass));
}

void PostProcessingPass::CreatePipeline() {
  // 1) Load SPIR‑V shaders
  auto vertCode = VkUtils::ReadFile("Resources/Shaders/PostProcessingVS.spv");
  auto fragCode = VkUtils::ReadFile("Resources/Shaders/PostProcessingPS.spv");
  VkShaderModule vertShader = VkUtils::CreateShaderModule(m_device, vertCode);
  VkShaderModule fragShader = VkUtils::CreateShaderModule(m_device, fragCode);

  // 2) Pipeline shader stages
  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0] = {
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertShader, "main", nullptr};
  stages[1] = {
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragShader, "main", nullptr};

  // 3) No vertex buffers: full‑screen triangle in VS
  VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vi.vertexBindingDescriptionCount = 0;
  vi.pVertexBindingDescriptions = nullptr;
  vi.vertexAttributeDescriptionCount = 0;
  vi.pVertexAttributeDescriptions = nullptr;

  // 4) Input assembly: triangle list
  VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  ia.primitiveRestartEnable = VK_FALSE;

  // 5) Viewport & scissor
  VkViewport viewport{0.0f, 0.0f, float(m_extent.width), float(m_extent.height), 0.0f, 1.0f};
  VkRect2D scissor{{0, 0}, m_extent};
  VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  vp.viewportCount = 1;
  vp.pViewports = &viewport;
  vp.scissorCount = 1;
  vp.pScissors = &scissor;

  // 6) Rasterizer
  VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rs.depthClampEnable = VK_FALSE;
  rs.rasterizerDiscardEnable = VK_FALSE;
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.lineWidth = 1.0f;
  rs.cullMode = VK_CULL_MODE_NONE;
  rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rs.depthBiasEnable = VK_FALSE;

  // 7) Multisampling
  VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  ms.sampleShadingEnable = VK_FALSE;

  // 8) Color blending
  VkPipelineColorBlendAttachmentState attach{0};
  attach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  attach.blendEnable = VK_FALSE;
  VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  cb.logicOpEnable = VK_FALSE;
  cb.attachmentCount = 1;
  cb.pAttachments = &attach;

  // 9) Pipeline layout with push constant
  VkPushConstantRange pcr{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PostFXPushConstant)};
  VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pli.setLayoutCount = 1;
  pli.pSetLayouts = &m_descriptorSetLayout;
  pli.pushConstantRangeCount = 1;
  pli.pPushConstantRanges = &pcr;
  if (vkCreatePipelineLayout(m_device, &pli, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create PostProcess pipeline layout");
  }

  // 10) Graphics pipeline
  VkGraphicsPipelineCreateInfo gpi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  gpi.stageCount = 2;
  gpi.pStages = stages;
  gpi.pVertexInputState = &vi;
  gpi.pInputAssemblyState = &ia;
  gpi.pViewportState = &vp;
  gpi.pRasterizationState = &rs;
  gpi.pMultisampleState = &ms;
  gpi.pColorBlendState = &cb;
  gpi.layout = m_pipelineLayout;
  gpi.renderPass = m_renderPass;
  gpi.subpass = 0;
  if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &gpi, nullptr, &m_pipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create PostProcess pipeline");
  }

  // 11) Cleanup shader modules
  vkDestroyShaderModule(m_device, vertShader, nullptr);
  vkDestroyShaderModule(m_device, fragShader, nullptr);
}

void PostProcessingPass::CreateFramebuffers() {
  m_framebuffers.resize(m_swapchainImageViews.size());
  for (size_t i = 0; i < m_framebuffers.size(); ++i) {
    VkFramebufferCreateInfo fci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fci.renderPass = m_renderPass;
    fci.attachmentCount = 1;
    fci.pAttachments = &m_swapchainImageViews[i];
    fci.width = m_extent.width;
    fci.height = m_extent.height;
    fci.layers = 1;
    if (vkCreateFramebuffer(m_device, &fci, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create PostProcess framebuffer");
    }
  }
}

void PostProcessingPass::CreateDescriptorSets() {
  for (uint32_t i = 0; i < m_swapchainImageViews.size(); ++i) {
    VkDescriptorImageInfo colorInfo{m_pLightingPass->GetColourSampler(), m_pLightingPass->GetColourImageView(i),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo shadowInfo{m_pLightingPass->GetShadowSampler(), m_pLightingPass->GetShadowImageView(i),
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    auto builder = VkUtils::DescriptorBuilder::Begin(&g_DescriptorLayoutCache, &g_DescriptorAllocator);
    builder.BindImage(0, &colorInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .BindImage(1, &shadowInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

    g_DescriptorManager.AddDescriptorSet(&builder, "PostProcessInput" + std::to_string(i));
  }
}