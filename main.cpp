#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1002000
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include "vert.h"
#include "frag.h"

#define HEIGHT 600
#define WIDTH 800
#define MAX_FRAMES_IN_FLIGHT 2

class VulkanRenderer {
private:
    uint8_t m_currentFrame = 0;
    GLFWwindow* m_window;
    vk::Instance m_instance;
    vk::PhysicalDevice m_physicalDevice;
    vk::Device m_device;
    VmaAllocator m_allocator;
    vk::Queue m_queue;
    vk::SurfaceKHR m_surface;
    vk::SwapchainKHR m_swapchain;
    std::vector<vk::Image> m_swapchainImages;
    std::vector<vk::ImageView> m_swapchainImageViews;
    vk::RenderPass m_renderPass;
    vk::PipelineLayout m_pipelineLayout;
    vk::Pipeline m_graphicsPipeline;
    std::vector<vk::Framebuffer> m_framebuffers;
    vk::CommandPool m_commandPool;
    vk::CommandBuffer m_commandBuffer[MAX_FRAMES_IN_FLIGHT];
    vk::Semaphore m_imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    vk::Semaphore m_renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
    vk::Fence m_inFlightFences[MAX_FRAMES_IN_FLIGHT];
    vk::Buffer m_vertexBuffers[MAX_FRAMES_IN_FLIGHT];
    VmaAllocation m_vertexBufferAllocations[MAX_FRAMES_IN_FLIGHT];
    vk::Buffer m_instanceBuffers[MAX_FRAMES_IN_FLIGHT];
    VmaAllocation m_instanceBufferAllocations[MAX_FRAMES_IN_FLIGHT];
    vk::Buffer m_stagingBuffers[MAX_FRAMES_IN_FLIGHT];
    VmaAllocation m_stagingBufferAllocations[MAX_FRAMES_IN_FLIGHT];
    void * m_mappedStagingBuffers[MAX_FRAMES_IN_FLIGHT];
    void initWindow(){
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Window", nullptr, nullptr);
    };
    void createInstance(){
        vk::ApplicationInfo appInfo("VulkanApp", 1, "No Engine", 1, VK_API_VERSION_1_2);
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        std::vector<const char*> layers={"VK_LAYER_KHRONOS_validation"};
        vk::InstanceCreateInfo createInfo(vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR, &appInfo, layers.size(), layers.data(), extensions.size(), extensions.data());
        m_instance = vk::createInstance(createInfo);
    };
    void createPhysicalDevice(){
        std::vector<vk::PhysicalDevice> devices = m_instance.enumeratePhysicalDevices();
        for(const auto& deviceType: {vk::PhysicalDeviceType::eDiscreteGpu, vk::PhysicalDeviceType::eIntegratedGpu, vk::PhysicalDeviceType::eVirtualGpu, vk::PhysicalDeviceType::eCpu}){
            for(const auto& device: devices){
                if(device.getProperties().deviceType == deviceType){
                    m_physicalDevice = device;
                    break;
                }
            }
        }
    };
    void createLogicalDevice(){
        float queuePriority = 1.0f;
        vk::DeviceQueueCreateInfo queueCreateInfo({}, 0, 1, &queuePriority);
        std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset"};
        vk::DeviceCreateInfo createInfo{};
        createInfo.queueCreateInfoCount = 1;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.enabledExtensionCount = deviceExtensions.size();
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        m_device = m_physicalDevice.createDevice(createInfo);
        m_queue = m_device.getQueue(0, 0);
    };
    void createAllocator(){
        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.physicalDevice = m_physicalDevice;
        allocatorInfo.device = m_device;
        allocatorInfo.instance = m_instance;
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
        vmaCreateAllocator(&allocatorInfo, &m_allocator);
    };
    void createSurface(){
        VkSurfaceKHR surface;
        if(glfwCreateWindowSurface(m_instance, m_window, nullptr, &surface) != VK_SUCCESS){
            throw std::runtime_error("Failed to create window surface");
        }
        m_surface = surface;
    };
    void createSwapchain(){
        vk::SurfaceCapabilitiesKHR surfaceCapabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface);
        vk::SurfaceFormatKHR surfaceFormat = vk::Format::eB8G8R8A8Srgb;
        vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
        vk::Extent2D extent = surfaceCapabilities.currentExtent;
        vk::SwapchainCreateInfoKHR createInfo({}, m_surface, surfaceCapabilities.minImageCount, surfaceFormat.format, surfaceFormat.colorSpace, extent, 1, vk::ImageUsageFlagBits::eColorAttachment, vk::SharingMode::eExclusive, 0, nullptr, surfaceCapabilities.currentTransform, vk::CompositeAlphaFlagBitsKHR::eOpaque, presentMode, VK_TRUE);
        m_swapchain = m_device.createSwapchainKHR(createInfo);
    };
    void createImageViews(){
        m_swapchainImages = m_device.getSwapchainImagesKHR(m_swapchain);
        for(const auto& image: m_swapchainImages){
            vk::ImageViewCreateInfo createInfo({}, image, vk::ImageViewType::e2D, vk::Format::eB8G8R8A8Srgb, vk::ComponentMapping{}, vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
            m_swapchainImageViews.push_back(m_device.createImageView(createInfo));
        }
    };
    void createRenderPass(){
        vk::AttachmentDescription attachmentDescription{};
        attachmentDescription.format = vk::Format::eB8G8R8A8Srgb;
        attachmentDescription.samples = vk::SampleCountFlagBits::e1;
        attachmentDescription.loadOp = vk::AttachmentLoadOp::eClear;
        attachmentDescription.storeOp = vk::AttachmentStoreOp::eStore;
        attachmentDescription.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        attachmentDescription.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        attachmentDescription.initialLayout = vk::ImageLayout::eUndefined;
        attachmentDescription.finalLayout = vk::ImageLayout::ePresentSrcKHR;
        vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);
        vk::SubpassDescription subpassDescription({}, vk::PipelineBindPoint::eGraphics, 0, nullptr, 1, &colorAttachmentRef, nullptr, nullptr, 0, nullptr);
        vk::SubpassDependency dependency(VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, vk::AccessFlagBits::eColorAttachmentWrite, vk::DependencyFlagBits::eByRegion);
        vk::RenderPassCreateInfo createInfo({}, 1, &attachmentDescription, 1, &subpassDescription, 1, &dependency);
        m_renderPass = m_device.createRenderPass(createInfo);
    }
    void createGraphicsPipeline(){
        vk::ShaderModuleCreateInfo vertShaderCreateInfo{};
        vertShaderCreateInfo.codeSize = sizeof(vert_spv);
        vertShaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vert_spv);
        vk::ShaderModule vertShaderModule = m_device.createShaderModule(vertShaderCreateInfo);
        vk::ShaderModuleCreateInfo fragShaderCreateInfo{};
        fragShaderCreateInfo.codeSize = sizeof(frag_spv);
        fragShaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(frag_spv);
        vk::ShaderModule fragShaderModule = m_device.createShaderModule(fragShaderCreateInfo);
        vk::PipelineShaderStageCreateInfo vertShaderStageInfo({}, vk::ShaderStageFlagBits::eVertex, vertShaderModule, "main");
        vk::PipelineShaderStageCreateInfo fragShaderStageInfo({}, vk::ShaderStageFlagBits::eFragment, fragShaderModule, "main");
        vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
        vk::VertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(float)*2;
        bindingDescription.inputRate = vk::VertexInputRate::eVertex;
        vk::VertexInputAttributeDescription attributeDescription{};
        attributeDescription.binding = 0;
        attributeDescription.location = 0;
        attributeDescription.format = vk::Format::eR32G32Sfloat;
        attributeDescription.offset = 0;
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = 1;
        vertexInputInfo.pVertexAttributeDescriptions = &attributeDescription;
        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo({}, vk::PrimitiveTopology::eTriangleList, VK_FALSE);
        vk::Viewport viewport(0.0f, 0.0f, WIDTH, HEIGHT, 0.0f, 1.0f);
        vk::Rect2D scissor({0, 0}, {WIDTH, HEIGHT});
        vk::PipelineViewportStateCreateInfo viewportStateInfo({}, 1, &viewport, 1, &scissor);
        vk::PipelineRasterizationStateCreateInfo rasterizerInfo({}, VK_FALSE, VK_FALSE, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f);
        vk::PipelineMultisampleStateCreateInfo multisamplingInfo;
        vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        colorBlendAttachment.blendEnable = VK_FALSE;
        vk::PipelineColorBlendStateCreateInfo colorBlendingInfo({}, VK_FALSE, vk::LogicOp::eCopy, 1, &colorBlendAttachment);
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
        m_pipelineLayout = m_device.createPipelineLayout(pipelineLayoutInfo);
        vk::GraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
        pipelineInfo.pViewportState = &viewportStateInfo;
        pipelineInfo.pRasterizationState = &rasterizerInfo;
        pipelineInfo.pMultisampleState = &multisamplingInfo;
        pipelineInfo.pColorBlendState = &colorBlendingInfo;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = m_renderPass;
        pipelineInfo.subpass = 0;
        m_graphicsPipeline = m_device.createGraphicsPipeline({}, pipelineInfo).value;
        m_device.destroyShaderModule(vertShaderModule);
        m_device.destroyShaderModule(fragShaderModule);
    };
    void createFramebuffers(){
        for(const auto& imageView: m_swapchainImageViews){
            vk::FramebufferCreateInfo createInfo({}, m_renderPass, 1, &imageView, WIDTH, HEIGHT, 1);
            m_framebuffers.push_back(m_device.createFramebuffer(createInfo));
        }
    };
    void createCommandPool(){
        vk::CommandPoolCreateInfo createInfo{};
        createInfo.queueFamilyIndex = 0;
        createInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        m_commandPool = m_device.createCommandPool(createInfo);
    };
    void createCommandBuffers(){
        vk::CommandBufferAllocateInfo allocateInfo(m_commandPool, vk::CommandBufferLevel::ePrimary, MAX_FRAMES_IN_FLIGHT);
        std::vector<vk::CommandBuffer> buffers =  m_device.allocateCommandBuffers(allocateInfo);
        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
            m_commandBuffer[i] = buffers[i];
        }
    };
    void createSyncObjects(){
        vk::SemaphoreCreateInfo semaphoreInfo{};
        vk::FenceCreateInfo fenceInfo{vk::FenceCreateFlagBits::eSignaled};
        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
            m_imageAvailableSemaphores[i] = m_device.createSemaphore(semaphoreInfo);
            m_renderFinishedSemaphores[i] = m_device.createSemaphore(semaphoreInfo);
            m_inFlightFences[i] = m_device.createFence(fenceInfo);
        }
    };
    void createVertexBuffers(){
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.size = sizeof(float)*52;
        bufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
            vmaCreateBuffer(m_allocator, reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo), &allocInfo, reinterpret_cast<VkBuffer*>(&m_vertexBuffers[i]), &m_vertexBufferAllocations[i], nullptr);
        }
    };
    void createInstanceBuffers(){
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.size = sizeof(float)*2;
        bufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
            vmaCreateBuffer(m_allocator, reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo), &allocInfo, reinterpret_cast<VkBuffer*>(&m_instanceBuffers[i]), &m_instanceBufferAllocations[i], nullptr);
        }
    };
    void createStagingBuffers(){
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.size = sizeof(float)*54;
        bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
            vmaCreateBuffer(m_allocator, reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo), &allocInfo, reinterpret_cast<VkBuffer*>(&m_stagingBuffers[i]), &m_stagingBufferAllocations[i], nullptr);
            vmaMapMemory(m_allocator, m_stagingBufferAllocations[i], &m_mappedStagingBuffers[i]);
        }
    };

public:
    VulkanRenderer(){
        initWindow();
        createInstance();
        createPhysicalDevice();
        createLogicalDevice();
        createAllocator();
        createSurface();
        createSwapchain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
        createVertexBuffers();
        createInstanceBuffers();
        createStagingBuffers();
    };
    ~VulkanRenderer(){
        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
            vmaUnmapMemory(m_allocator, m_stagingBufferAllocations[i]);
            vmaDestroyBuffer(m_allocator, m_stagingBuffers[i], m_stagingBufferAllocations[i]);
            vmaDestroyBuffer(m_allocator, m_instanceBuffers[i], m_instanceBufferAllocations[i]);
            vmaDestroyBuffer(m_allocator, m_vertexBuffers[i], m_vertexBufferAllocations[i]);
            m_device.destroySemaphore(m_imageAvailableSemaphores[i]);
            m_device.destroySemaphore(m_renderFinishedSemaphores[i]);
            m_device.destroyFence(m_inFlightFences[i]);
            m_device.freeCommandBuffers(m_commandPool, m_commandBuffer[i]);
        }
        m_device.destroyCommandPool(m_commandPool);
        for(const auto& framebuffer: m_framebuffers){
            m_device.destroyFramebuffer(framebuffer);
        }
        m_device.destroyPipeline(m_graphicsPipeline);
        m_device.destroyPipelineLayout(m_pipelineLayout);
        m_device.destroyRenderPass(m_renderPass);
        for(const auto& imageView: m_swapchainImageViews){
            m_device.destroyImageView(imageView);
        }
        m_device.destroySwapchainKHR(m_swapchain);
        m_instance.destroySurfaceKHR(m_surface);
        vmaDestroyAllocator(m_allocator);
        m_device.destroy();
        m_instance.destroy();
        glfwDestroyWindow(m_window);
        glfwTerminate();
    };
    void run(){
        while(!glfwWindowShouldClose(m_window)){
            glfwPollEvents();
        }
    };
};

int main() {
    VulkanRenderer app;
    app.run();
    return 0;
}
