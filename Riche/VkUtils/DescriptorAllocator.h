#pragma once

#include <vulkan/vulkan.h>

#include <mutex>
#include <vector>

// Manages pools of descriptor pools and allocates descriptor sets
class DescriptorAllocator {
 public:
  // Initialize with Vulkan device
  void Initialize(VkDevice device) {
    m_device = device;
    // Create an initial pool
    m_currentPool = CreatePool();
  }

  // Free all pools
  void Cleanup() {
    for (auto pool : m_pools) {
      vkDestroyDescriptorPool(m_device, pool, nullptr);
    }
    m_pools.clear();
  }

  // Allocate a descriptor set from the current pool, creating a new pool if needed
  VkDescriptorSet Allocate(VkDescriptorSetLayout layout) {
    std::lock_guard<std::mutex> lock(m_mutex);
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = m_currentPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet set;
    VkResult res = vkAllocateDescriptorSets(m_device, &allocInfo, &set);
    if (res == VK_ERROR_FRAGMENTED_POOL || res == VK_ERROR_OUT_OF_POOL_MEMORY) {
      // Pool is full, create a new one
      m_currentPool = CreatePool();
      allocInfo.descriptorPool = m_currentPool;
      vkAllocateDescriptorSets(m_device, &allocInfo, &set);
    }
    return set;
  }

 private:
  VkDevice m_device = VK_NULL_HANDLE;
  std::vector<VkDescriptorPool> m_pools;
  VkDescriptorPool m_currentPool = VK_NULL_HANDLE;
  std::mutex m_mutex;

  // Create a new descriptor pool with generous sizes
  VkDescriptorPool CreatePool() {
    VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 100},
                                        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
                                        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100},
                                        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100},
                                        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100},
                                        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100},
                                        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100},
                                        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100},
                                        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100},
                                        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100},
                                        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100}};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = (uint32_t)std::size(poolSizes);
    poolInfo.pPoolSizes = poolSizes;

    VkDescriptorPool pool;
    vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &pool);
    m_pools.push_back(pool);
    return pool;
  }
};
