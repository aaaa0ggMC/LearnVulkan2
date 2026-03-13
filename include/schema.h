#ifndef LEARN_VULKAN_SCHEMA_H
#define LEARN_VULKAN_SCHEMA_H
#include <string_view>

inline std::string_view str_schema = R"({
    "logger" : {
        "back_pressure" : ["OPTIONAL TYPE BOOL",false],
        "consumer_count" : ["OPTIONAL TYPE INT MIN 0", 1],
        "fetch_message_count_max" : ["OPTIONAL TYPE INT MIN 0", 128],
        "back_pressure_multiply" : ["OPTIONAL TYPE INT MIN 0", 4],
        "header" : ["OPTIONAL TYPE STRING","LearnVulkan"]
    },
    "language" : ["OPTIONAL TYPE STRING","zh_cn"],
    "window" : {
        "width" : ["OPTIONAL TYPE INT MIN 0", 800 ],
        "height" : ["OPTIONAL TYPE INT MIN 0", 600 ],
        "title" : ["OPTIONAL TYPE STRING" , "LearnVulkan"]
    }
})";
#endif