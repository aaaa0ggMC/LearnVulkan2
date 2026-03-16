#include <app.h>

using enum alib5::Severity;
using namespace alib5;


int App::run(){
    static uint64_t current_frame = 0;

    Clock clk;
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        draw(current_frame);
    }
    vkDeviceWaitIdle(device);

    lg(Info,*translator,"check.fps") << log_tfmt("{:.2f}") << current_frame / clk.get_all() * 1000 << endlog;
    return 0;
}

void App::draw(size_t & current_frame){
    uint64_t sync_index = current_frame % sync_object_count;
    auto & fence = fen_in_flights[sync_index];
    auto & semaphore_img = sem_img_available[sync_index];
    auto & cmd_buffer = cmd_buffers[sync_index];

    vkWaitForFences(device, 1, &fence ,VK_TRUE, UINT64_MAX);
    
    uint32_t image_index = 0;
    vkAcquireNextImageKHR(device,swapchain,UINT64_MAX,semaphore_img,VK_NULL_HANDLE,&image_index);
    auto & seamphore_render = sem_render_fin[image_index];

    vkResetFences(device, 1, &fence);

    vkResetCommandBuffer(cmd_buffer, 0);
    vk_record_command_buffer(cmd_buffer, image_index);

    VkSubmitInfo submit_info {};
    submit_info.sType =  VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore wait_semaphores[] = { semaphore_img };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buffer;
    
    VkSemaphore signal_semaphores[] = { seamphore_render };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    if(VkResult result = vkQueueSubmit(graphics_queue, 1, &submit_info, fence)){
        lg(Error,*translator,"bad.sumbit_queue",(int)result) << endlog;
        throw "BG";
    }
    
    VkPresentInfoKHR present_info {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swapchains[] = { swapchain }; 
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &image_index;
    present_info.pResults = nullptr;

    if(VkResult result = vkQueuePresentKHR(graphics_queue,&present_info)){
        lg(Error,*translator,"bad.queue_present",(int)result) << endlog;
        throw "Bad GUYYYY";
    }    

    ++current_frame;
}