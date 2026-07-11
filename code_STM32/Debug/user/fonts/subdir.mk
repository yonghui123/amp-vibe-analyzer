################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../user/fonts/asc12.c \
../user/fonts/asc16.c \
../user/fonts/hz12.c \
../user/fonts/hz16.c \
../user/fonts/hz24.c \
../user/fonts/hz32.c \
../user/fonts/ra8875_asc_width.c 

O_SRCS += \
../user/fonts/hz16.o 

OBJS += \
./user/fonts/asc12.o \
./user/fonts/asc16.o \
./user/fonts/hz12.o \
./user/fonts/hz16.o \
./user/fonts/hz24.o \
./user/fonts/hz32.o \
./user/fonts/ra8875_asc_width.o 

C_DEPS += \
./user/fonts/asc12.d \
./user/fonts/asc16.d \
./user/fonts/hz12.d \
./user/fonts/hz16.d \
./user/fonts/hz24.d \
./user/fonts/hz32.d \
./user/fonts/ra8875_asc_width.d 


# Each subdirectory must supply rules for building sources it contributes
user/fonts/%.o user/fonts/%.su user/fonts/%.cyclo: ../user/fonts/%.c user/fonts/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -DLV_CONF_INCLUDE_SIMPLE -c -I../Core/Inc -I../Drivers/CMSIS/DSP/Include -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/LVGL -I../Middlewares/Third_Party/LVGL/src -I../Middlewares/Third_Party/FatFs/src -I../FATFS/App -I../FATFS/Target -I../user/app/inlcude -I../user/app/inlcude/gui -I../user/bsp/include -I../user/fonts -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-user-2f-fonts

clean-user-2f-fonts:
	-$(RM) ./user/fonts/asc12.cyclo ./user/fonts/asc12.d ./user/fonts/asc12.o ./user/fonts/asc12.su ./user/fonts/asc16.cyclo ./user/fonts/asc16.d ./user/fonts/asc16.o ./user/fonts/asc16.su ./user/fonts/hz12.cyclo ./user/fonts/hz12.d ./user/fonts/hz12.o ./user/fonts/hz12.su ./user/fonts/hz16.cyclo ./user/fonts/hz16.d ./user/fonts/hz16.o ./user/fonts/hz16.su ./user/fonts/hz24.cyclo ./user/fonts/hz24.d ./user/fonts/hz24.o ./user/fonts/hz24.su ./user/fonts/hz32.cyclo ./user/fonts/hz32.d ./user/fonts/hz32.o ./user/fonts/hz32.su ./user/fonts/ra8875_asc_width.cyclo ./user/fonts/ra8875_asc_width.d ./user/fonts/ra8875_asc_width.o ./user/fonts/ra8875_asc_width.su

.PHONY: clean-user-2f-fonts

