#pragma once

#include <vulkan/vulkan.h>

#include <mutex>
#include <unordered_map>

// Caches VkDescriptorSetLayout objects to avoid redundant creations
class DescriptorLayoutCache {
 public:
  // Initialize with Vulkan device
  void Initialize(VkDevice device) { m_device = device; }

  // Destroy all cached layouts
  void Cleanup() {
    for (auto& pair : m_layoutCache) {
      vkDestroyDescriptorSetLayout(m_device, pair.second, nullptr);
    }
    m_layoutCache.clear();
  }

  // Creates or returns a cached descriptor set layout matching 'info'
  VkDescriptorSetLayout CreateDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo* info) {
    // Hash descriptor bindings for lookup
    size_t hash = HashLayoutInfo(info);
    auto it = m_layoutCache.find(hash);
    if (it != m_layoutCache.end()) {
      return it->second;
    }
    VkDescriptorSetLayout layout;
    vkCreateDescriptorSetLayout(m_device, info, nullptr, &layout);
    m_layoutCache[hash] = layout;
    return layout;
  }

 private:
  VkDevice m_device = VK_NULL_HANDLE;
  std::unordered_map<size_t, VkDescriptorSetLayout> m_layoutCache;
  std::mutex m_mutex;

  // Simple hash combining binding info
  size_t HashLayoutInfo(const VkDescriptorSetLayoutCreateInfo* info) {
    size_t h = info->bindingCount;
    for (uint32_t i = 0; i < info->bindingCount; ++i) {
      const VkDescriptorSetLayoutBinding& b = info->pBindings[i];
      h ^= std::hash<uint32_t>()(b.binding + b.descriptorType);
      h ^= std::hash<uint32_t>()(b.descriptorCount << 1);
      h ^= std::hash<uint32_t>()(b.stageFlags << 2);
    }
    return h;
  }
};