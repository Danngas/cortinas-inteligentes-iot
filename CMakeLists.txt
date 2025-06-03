# ====================================================================================
# Generated Cmake Pico project file
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(main C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()


include_directories(${CMAKE_SOURCE_DIR}/lib)
# Add executable. Default name is the project name, version 0.1


add_executable(${PROJECT_NAME}
    ${PROJECT_NAME}.c
        lib/ssd1306.c
      
)



pico_set_program_name(${PROJECT_NAME} "mqtt_client")
pico_set_program_version(${PROJECT_NAME} "0.1")

# Generate PIO header - matriz de led
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/lib/ws2818b.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 1)

# Add the standard library to the build
target_link_libraries(${PROJECT_NAME}
    pico_lwip_mqtt
    pico_mbedtls
    pico_lwip_mbedtls
    pico_stdlib
    hardware_gpio
    hardware_adc
    pico_cyw43_arch_lwip_threadsafe_background
    pico_bootsel_via_double_reset # PARA COLOCAR A PLACA NO MODO DE GRAVACAO
    pico_bootrom # PARA COLOCAR A PLACA NO MODO DE GRAVACAO
    hardware_clocks # para matriz de leds
    hardware_pwm
    hardware_i2c
    pico_time
    hardware_pio # para matriz de leds
)





# Add the standard include files to the build
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(${PROJECT_NAME} 
        
        )

pico_add_extra_outputs(${PROJECT_NAME})

