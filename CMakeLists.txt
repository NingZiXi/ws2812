add_library(ws2812 STATIC ${CMAKE_CURRENT_LIST_DIR}/ws2812.c)
target_include_directories(ws2812 PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(ws2812 PUBLIC stm32cubemx)