################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../user/app/src/storage/config_codec.c \
../user/app/src/storage/envelope_csv.c \
../user/app/src/storage/json_kv.c \
../user/app/src/storage/path_layout.c \
../user/app/src/storage/sd_fs.c 

OBJS += \
./user/app/src/storage/config_codec.o \
./user/app/src/storage/envelope_csv.o \
./user/app/src/storage/json_kv.o \
./user/app/src/storage/path_layout.o \
./user/app/src/storage/sd_fs.o 

C_DEPS += \
./user/app/src/storage/config_codec.d \
./user/app/src/storage/envelope_csv.d \
./user/app/src/storage/json_kv.d \
./user/app/src/storage/path_layout.d \
./user/app/src/storage/sd_fs.d 


# Each subdirectory must supply rules for building sources it contributes
user/app/src/storage/%.o user/app/src/storage/%.su user/app/src/storage/%.cyclo: ../user/app/src/storage/%.c user/app/src/storage/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -DLV_CONF_INCLUDE_SIMPLE -c -I../Core/Inc -I../Drivers/CMSIS/DSP/Include -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/LVGL -I../Middlewares/Third_Party/LVGL/src -I../Middlewares/Third_Party/FatFs/src -I../FATFS/App -I../FATFS/Target -I../user/app/inlcude -I../user/app/inlcude/gui -I../user/bsp/include -I../user/fonts -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-user-2f-app-2f-src-2f-storage

clean-user-2f-app-2f-src-2f-storage:
	-$(RM) ./user/app/src/storage/config_codec.cyclo ./user/app/src/storage/config_codec.d ./user/app/src/storage/config_codec.o ./user/app/src/storage/config_codec.su ./user/app/src/storage/envelope_csv.cyclo ./user/app/src/storage/envelope_csv.d ./user/app/src/storage/envelope_csv.o ./user/app/src/storage/envelope_csv.su ./user/app/src/storage/json_kv.cyclo ./user/app/src/storage/json_kv.d ./user/app/src/storage/json_kv.o ./user/app/src/storage/json_kv.su ./user/app/src/storage/path_layout.cyclo ./user/app/src/storage/path_layout.d ./user/app/src/storage/path_layout.o ./user/app/src/storage/path_layout.su ./user/app/src/storage/sd_fs.cyclo ./user/app/src/storage/sd_fs.d ./user/app/src/storage/sd_fs.o ./user/app/src/storage/sd_fs.su

.PHONY: clean-user-2f-app-2f-src-2f-storage
