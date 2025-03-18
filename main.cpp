#include <vulkan/vulkan.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include "vert.h"
#include "frag.h"

class VulkanRenderer {
private:
    GLFWwindow* m_window;
    vk::Instance m_instance;
    vk::PhysicalDevice m_physicalDevice;
    vk::Device m_device;
    vk::Queue m_queue;
    vk::SurfaceKHR m_surface;
    vk::SwapchainKHR m_swapchain;
    std::vector<vk::Image> m_swapchainImages;
    std::vector<vk::ImageView> m_swapchainImageViews;
    vk::Pipeline m_graphicsPipeline;
    void initWindow(){
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_window = glfwCreateWindow(800, 600, "Vulkan Window", nullptr, nullptr);
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
    void createGraphicsPipeline(){
    };

public:
    VulkanRenderer(){
        initWindow();
        createInstance();
        createPhysicalDevice();
        createLogicalDevice();
        createSurface();
        createSwapchain();
        createImageViews();
        createGraphicsPipeline();
    };
    ~VulkanRenderer(){
        m_device.destroyPipeline(m_graphicsPipeline);
        for(const auto& imageView: m_swapchainImageViews){
            m_device.destroyImageView(imageView);
        }
        m_device.destroySwapchainKHR(m_swapchain);
        m_instance.destroySurfaceKHR(m_surface);
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
