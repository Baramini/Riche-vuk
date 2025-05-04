#pragma once

#include <vulkan/vulkan.h>

#include <vector>

class BasicLightingPass;
class CullingRenderPass;

class PostProcessingPass {
 public:
  void Init(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D extent, const std::vector<VkImageView>& swapchainImageViews,
            BasicLightingPass* lightingPass, CullingRenderPass* shadowPass, VkFormat swapchainFormat, uint32_t maxFramesInFlight);
  void Cleanup();
  void RecordCommands(VkCommandBuffer cmd, uint32_t frameIndex);

  VkRenderPass GetRenderPass() const { return m_renderPass; }
  VkFramebuffer GetFramebuffer(uint32_t i) const { return m_framebuffers[i]; }
  VkSemaphore GetSemaphore(uint32_t i) const { return m_semaphores[i]; }

 private:
  void CreateDescriptorSetLayout();
  void CreateRenderPass();
  void CreatePipeline();
  void CreateFramebuffers();
  void CreateDescriptorSets();

 private:
  VkDevice m_device = VK_NULL_HANDLE;
  VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
  VkExtent2D m_extent{};

  VkRenderPass m_renderPass = VK_NULL_HANDLE;
  VkPipeline m_pipeline = VK_NULL_HANDLE;
  VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
  VkFormat m_swapchainFormat = VK_FORMAT_UNDEFINED;

  BasicLightingPass* m_pLightingPass = nullptr;

  std::vector<VkFramebuffer> m_framebuffers;
  std::vector<VkSemaphore> m_semaphores;
  std::vector<VkImageView> m_swapchainImageViews;

  uint32_t m_maxFrames = 0;
};

struct PostFXPushConstant {
  int isEnableBloom;
  float padding;
  glm::vec2 texelSize;
};