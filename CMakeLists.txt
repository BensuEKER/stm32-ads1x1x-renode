# CMakeLists.txt
#
# STM32F407VG_ADS1115 firmware build tanimi. arm-none-eabi-gcc ile
# capraz derleme (cross-compile) yapar, cikti olarak .elf uretir.
#
# Kullanim (repo kokunden):
#   cmake --preset debug
#   cmake --build --preset debug
#
# ya da preset kullanmadan dogrudan:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake
#   cmake --build build

cmake_minimum_required(VERSION 3.20)

project(STM32F407VG_ADS1x1x C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --------------------------------------------------------------------
# MCU'ya ozel derleyici bayraklari
# --------------------------------------------------------------------
set(MCU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)

add_compile_options(
    ${MCU_FLAGS}
    -Wall
    -ffunction-sections
    -fdata-sections
    $<$<CONFIG:Debug>:-Og>
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Release>:-O2>
)
add_compile_definitions(USE_HAL_DRIVER STM32F407xx)

# --------------------------------------------------------------------
# Kaynak dosyalar
# --------------------------------------------------------------------
file(GLOB CORE_SOURCES        ${CMAKE_SOURCE_DIR}/Core/Src/*.c)
file(GLOB HAL_SOURCES         ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/*.c)
set(STARTUP_SOURCE            ${CMAKE_SOURCE_DIR}/Core/Startup/startup_stm32f407vgtx.s)
set(ADS1X1X_SOURCES           ${CMAKE_SOURCE_DIR}/lib/ads1x1x/src/ads1x1x.c)

add_executable(${PROJECT_NAME}
    ${CORE_SOURCES}
    ${HAL_SOURCES}
    ${STARTUP_SOURCE}
    ${ADS1X1X_SOURCES}
)
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

# --------------------------------------------------------------------
# Include (header) dizinleri
# --------------------------------------------------------------------
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/lib/ads1x1x/inc
)

# --------------------------------------------------------------------
# Linker ayarlari
# --------------------------------------------------------------------
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/STM32F407VGTX_FLASH.ld)

target_link_options(${PROJECT_NAME} PRIVATE
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -specs=nano.specs
    -specs=nosys.specs
    -Wl,-Map=${PROJECT_NAME}.map
    -Wl,--gc-sections
    -static
    -Wl,--start-group -lc -lm -Wl,--end-group
)
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})

# --------------------------------------------------------------------
# Derleme sonrasi: boyut bilgisini ekrana bas
# --------------------------------------------------------------------
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
)
