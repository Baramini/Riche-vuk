#include "DescriptorManager.h"
#include "DescriptorBuilder.h"

namespace VkUtils {
void DescriptorManager::Initialize() {}

void DescriptorManager::Cleanup(VkDevice device) {}

DescriptorHandle DescriptorManager::AddDescriptorSet(DescriptorBuilder* builder, std::string const& name,
                                                     bool isBindless /* = false */) {
  if (auto iter = loadedDescriptorSet.find(name) == loadedDescriptorSet.end()) {
    ++setHandle;
    ++setLayoutHandle;

    VkDescriptorSet set;
    VkDescriptorSetLayout setLayout;

    builder->Build(set, setLayout, isBindless);
    loadedDescriptorSet.insert({name, setHandle});
    loadedSetLayout.insert({name, setLayoutHandle});
    descriptorSetMap[setHandle] = set;
    setLayoutMap[setLayoutHandle] = setLayout;
  }
  return setHandle;
}

void DescriptorManager::UpdateDescriptorSet(DescriptorBuilder* builder, VkDescriptorSet set) { builder->Build(set); }

VkDescriptorSet& DescriptorManager::GetVkDescriptorSet(DescriptorHandle handle) { return descriptorSetMap.find(handle)->second; }

VkDescriptorSet& DescriptorManager::GetVkDescriptorSet(std::string const& name) {
  return descriptorSetMap.find(loadedDescriptorSet.find(name)->second)->second;
}

VkDescriptorSetLayout& DescriptorManager::GetVkDescriptorSetLayout(DescriptorHandle handle) {
  return setLayoutMap.find(handle)->second;
}

VkDescriptorSetLayout& DescriptorManager::GetVkDescriptorSetLayout(std::string const& name) {
  return setLayoutMap.find(loadedSetLayout.find(name)->second)->second;
}
void DescriptorManager::AddDescriptorSetLayout(std::string const& name, VkDescriptorSet set, VkDescriptorSetLayout layout) {
  ++setHandle;
  ++setLayoutHandle;

  loadedDescriptorSet.insert({name, setHandle});
  loadedSetLayout.insert({name, setLayoutHandle});
  descriptorSetMap[setHandle] = set;
  setLayoutMap[setLayoutHandle] = layout;
}
void DescriptorManager::RegisterSetLayout(const std::string& name, VkDescriptorSetLayout layout) {
  ++setLayoutHandle;
  loadedSetLayout[name] = setLayoutHandle;
  setLayoutMap[setLayoutHandle] = layout;
}
void DescriptorManager::RegisterLayout(const std::string& name, VkDescriptorSetLayout layout) {
  if (loadedSetLayout.find(name) == loadedSetLayout.end()) {
    ++setLayoutHandle;
    loadedSetLayout[name] = setLayoutHandle;
    setLayoutMap[setLayoutHandle] = layout;
  }
}
void DescriptorManager::RegisterSet(const std::string& name, VkDescriptorSet set) {
  if (loadedDescriptorSet.find(name) == loadedDescriptorSet.end()) {
    ++setHandle;
    loadedDescriptorSet[name] = setHandle;
    descriptorSetMap[setHandle] = set;
  } else {
    descriptorSetMap[loadedDescriptorSet[name]] = set;
  }
}
void DescriptorManager::UnregisterLayout(const std::string& name) {
  auto layoutIter = loadedSetLayout.find(name);
  if (layoutIter != loadedSetLayout.end()) {
    DescriptorHandle layoutHandle = layoutIter->second;
    setLayoutMap.erase(layoutHandle);   // handle ℃ VkDescriptorSetLayout 力芭
    loadedSetLayout.erase(layoutIter);  // name ℃ handle 力芭
  }
}
void DescriptorManager::UnregisterSet(const std::string& name) {
  auto setIter = loadedDescriptorSet.find(name);
  if (setIter != loadedDescriptorSet.end()) {
    DescriptorHandle setHandle = setIter->second;
    descriptorSetMap.erase(setHandle);   // handle ℃ VkDescriptorSet 力芭
    loadedDescriptorSet.erase(setIter);  // name ℃ handle 力芭
  }
}
}  // namespace VkUtils