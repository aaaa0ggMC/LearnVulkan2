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
            ["OPTIONAL TYPE INT MIN 0 MAX 7", 0 , "Variant Version"],
            ["OPTIONAL TYPE INT MIN 0 MAX 127", 1 , "Major Version"],
            ["OPTIONAL TYPE INT MIN 0 MAX 1023", 3 , "Minor Version"],
            ["OPTIONAL TYPE INT MIN 0 MAX 4095", 0 , "Patch Version"]
        ],
        "engine_name" : ["OPTIONAL TYPE STRING","No Engine"],
        "instance_extensions" : ["OPTIONAL TYPE ARRAY MIN 0","REQUIRED TYPE STRING"],
        "device_extensions" : ["OPTIONAL TYPE ARRAY MIN 0","REQUIRED TYPE STRING"],
        "layers" : ["OPTIONAL TYPE ARRAY MIN 0","REQUIRED TYPE STRING"],
        "debug_allow" : {
            "verbose" : ["OPTIONAL TYPE BOOL" , true],
            "info" : ["OPTIONAL TYPE BOOL" , true],
            "warn" : ["OPTIONAL TYPE BOOL" , true],
            "error" : ["OPTIONAL TYPE BOOL" , true],
            "others" : ["OPTIONAL TYPE BOOL" , true]
        },
        "score_multiplier" : {
            "api_version" : ["TYPE DOUBLE", 0.6 ],
            "discrete_gpu" : ["TYPE DOUBLE", 2.0 ],
            "image_dim2d" : ["TYPE DOUBLE", 0.2 ],
            "need_geometry" : ["TYPE BOOL" , true],
            "fail_load" : ["TYPE BOOL" , false]
        },
        "verbose_extensions" : ["TYPE BOOL" , false],
        "shaders" : ["TYPE ARRAY MIN 1",{
            "vert" : "TYPE STRING",
            "vert_raw" : "TYPE STRING",

            "frag" : ["TYPE STRING",""],
            "frag_raw" : ["TYPE STRING",""]
        }],
        "shader_detail_count" : ["TYPE INT MIN 0" , 128]
    }
})";
#endif