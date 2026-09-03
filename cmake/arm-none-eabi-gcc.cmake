# cmake/arm-none-eabi-gcc.cmake
#
# CMake'e "hedef bir Windows/Linux bilgisayari degil, isletim sistemi
# olmayan (bare-metal) bir ARM Cortex-M4 cipi" oldugunu soyleyen dosya.
# Bu dosya olmadan CMake, varsayilan olarak host bilgisayarin kendi
# derleyicisini (x86/x64 gcc) kullanmaya calisir -- bize gereken ise
# arm-none-eabi-gcc.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY      arm-none-eabi-objcopy)
set(CMAKE_SIZE         arm-none-eabi-size)

# Bare-metal hedefte CMake'in derleyiciyi test etmek icin ornek bir
# program calistirmaya calismasini engelliyoruz (calistiracak bir
# isletim sistemi/host yok, sadece derleme ve linkleme test edilebilir).
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
