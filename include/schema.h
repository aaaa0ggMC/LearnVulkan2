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
    },
    "vulkan":{
        "app_name":["OPTIONAL TYPE STRING","LearnVulkan"],
        "app_version" : [
            "TUPLE",
            ["OPTIONAL TYPE INT MIN 0", 1 , "Major Version"],
            ["OPTIONAL TYPE INT MIN 0", 0 , "Minor Version"],
            ["OPTIONAL TYPE INT MIN 0", 0 , "Patch Version"]
        ],
        "engine_version" : [
            "TUPLE",
            ["OPTIONAL TYPE INT MIN 0", 1 , "Major Version"],
            ["OPTIONAL TYPE INT MIN 0", 0 , "Minor Version"],
            ["OPTIONAL TYPE INT MIN 0", 0 , "Patch Version"]
        ],
        "api_version" : [
            "TUPLE",
            ["OPTIONAL TYPE INT MIN 0 MAX 127", 0 , "Variant Version"],
            ["OPTIONAL TYPE INT MIN 0 MAX 511", 1 , "Major Version"],
            ["OPTIONAL TYPE INT MIN 0 MAX 1023", 3 , "Minor Version"],
            ["OPTIONAL TYPE INT MIN 0 MAX 63", 0 , "Patch Version"]
        ],
        "engine_name" : ["OPTIONAL TYPE STRING","No Engine"],
        "extensions" : ["REQUIRED TYPE ARRAY MIN 0","REQUIRED TYPE STRING"],
        "layers" : ["REQUIRED TYPE ARRAY MIN 0","REQUIRED TYPE STRING"],
        "debug_allow" : {
            "verbose" : ["REQUIRED TYPE BOOL" , true],
            "info" : ["REQUIRED TYPE BOOL" , true],
            "warn" : ["REQUIRED TYPE BOOL" , true],
            "error" : ["REQUIRED TYPE BOOL" , true],
            "others" : ["REQUIRED TYPE BOOL" , true]
        }
    }
})";
#endif