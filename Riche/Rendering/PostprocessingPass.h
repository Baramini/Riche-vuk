#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include "Image.h"
#include "VkUtils/ChooseFunc.h"
#include "VkUtils/DescriptorManager.h"
#include "VkUtils/DescriptorBuilder.h"
#include "VkUtils/ResourceManager.h"
#include "VkUtils/ShaderModule.h"

class BasicLightingPass;
class CullingRenderPass;

class PostProcessingPass {
 public:
  void Init(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D extent, const std::vector<VkImageView>& swapchainImageViews,
            BasicLightingPass* lightingPass, CullingRenderPass* shadowPass, VkFormat swapchainFormat, uint32_t maxFramesInFlight);
  void Cleanup();

  void Update(uint32_t frameIndex);
  void RecordCommands(VkCommandBuffer cmd, uint32_t frameIndex);

  VkRenderPass GetRenderPass() const { return m_renderPass; }
  VkFramebuffer GetFramebuffer(uint32_t i) const { return m_framebuffers[i]; }
  VkSemaphore GetSemaphore(uint32_t i) const { return m_semaphores[i]; }

 private:
  void CreateRenderPass();
  void CreateFramebuffers();
  void CreatePipeline();
  void CreateDescriptorSets();

 private:
  VkDevice m_device = VK_NULL_HANDLE;
  VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
  VkExtent2D m_extent{};

  VkRenderPass m_renderPass = VK_NULL_HANDLE;
  VkPipeline m_pipeline = VK_NULL_HANDLE;
  VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
  VkFormat m_pSwapchainFormat = VK_FORMAT_UNDEFINED;

  BasicLightingPass* m_pLightingPass = nullptr;
  CullingRenderPass* m_pShadowPass = nullptr;

  std::vector<VkFramebuffer> m_framebuffers;
  std::vector<VkDescriptorSet> m_descriptorSets;
  std::vector<VkSemaphore> m_semaphores;
  std::vector<VkImageView> m_swapchainImageViews;

  uint32_t m_maxFrames = 0;
};

struct PostFXPushConstant {
  int isEnableBloom;
};