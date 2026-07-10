################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../user/app/src/stubs/stub_config_store.c \
../user/app/src/stubs/stub_data_logger.c \
../user/app/src/stubs/stub_fatfs.c 

OBJS += \
./user/app/src/stubs/stub_config_store.o \
./user/app/src/stubs/stub_data_logger.o \
./user/app/src/stubs/stub_fatfs.o 

C_DEPS += \
./user/app/src/stubs/stub_config_store.d \
./user/app/src/stubs/stub_data_logger.d \
./user/app/src/stubs/stub_fatfs.d 


# Each subdirectory must supply rules for building sources it contributes
user/app/src/stubs/%.o user/app/src/stubs/%.su user/app/src/stubs/%.cyclo: ../user/app/src/stubs/%.c user/app/src/stubs/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -DLV_CONF_INCLUDE_SIMPLE -c -I../Core/Inc -I../Drivers/CMSIS/DSP/Include -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/LVGL -I../Middlewares/Third_Party/LVGL/src -I../user/app/inlcude -I../user/app/inlcude/gui -I../user/bsp/include -I../user/fonts -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-user-2f-app-2f-src-2f-stubs

clean-user-2f-app-2f-src-2f-stubs:
	-$(RM) ./user/app/src/stubs/stub_config_store.cyclo ./user/app/src/stubs/stub_config_store.d ./user/app/src/stubs/stub_config_store.o ./user/app/src/stubs/stub_config_store.su ./user/app/src/stubs/stub_data_logger.cyclo ./user/app/src/stubs/stub_data_logger.d ./user/app/src/stubs/stub_data_logger.o ./user/app/src/stubs/stub_data_logger.su ./user/app/src/stubs/stub_fatfs.cyclo ./user/app/src/stubs/stub_fatfs.d ./user/app/src/stubs/stub_fatfs.o ./user/app/src/stubs/stub_fatfs.su

.PHONY: clean-user-2f-app-2f-src-2f-stubs

