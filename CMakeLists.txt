add_library(ws2812 STATIC ${CMAKE_CURRENT_LIST_DIR}/ws2812.cpp)
target_include_directories(ws2812 PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_options(ws2812 PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)
set_target_properties(ws2812 PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_link_libraries(ws2812 PUBLIC stm32cubemx)