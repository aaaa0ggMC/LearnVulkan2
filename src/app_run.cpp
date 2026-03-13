#include <app.h>

int App::run(){
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        glfwSwapBuffers(window);
    }
    return 0;
}