#include <app.h>

int App::run(){
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        draw();
    }
    return 0;
}

void App::draw(){
    vkWaitForFences(device, 1, &fen_in_flight, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &fen_in_flight);

    uint32_t image_index = 0;
    vkAcquireNextImageKHR(device,swapchain,UINT64_MAX,sem_img_available,VK_NULL_HANDLE,&image_index);

    vkResetCommandBuffer(cmd_buffer, 0);
    vk_record_command_buffer(cmd_buffer, image_index);
}