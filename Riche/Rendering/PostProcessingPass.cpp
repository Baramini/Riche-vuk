#include "PostProcessingPass.h"

void PostProcessingPass::Init(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D extent, const std::vector<VkImageView>& swapchainImageViews,
    uint32_t maxFramesInFlight) {
  m_device = device;
  m_physicalDevice = physicalDevice;
  m_extent = extent;
  m_maxFrames = maxFramesInFlight;

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
    inputColour.imageView = g_LightingPass->GetOutputImageView(i);  // 조명 결과 이미지 뷰
    inputColour.sampler = VK_NULL_HANDLE;

    // Shadow 텍스처
    VkDescriptorImageInfo shadow = {};
    shadow.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    shadow.imageView = g_ShadowPass->GetShadowImageView(i);  // 그림자 결과 이미지 뷰
    shadow.sampler = VK_NULL_HANDLE;

    // Bloom 텍스처
    VkDescriptorImageInfo bloom = {};
    bloom.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    bloom.imageView = g_BloomPass->GetBlurredBloomImageView(i);  // 블러된 Bloom 이미지 뷰
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