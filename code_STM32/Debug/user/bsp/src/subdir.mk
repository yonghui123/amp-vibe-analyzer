################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../user/bsp/src/bsp_ad7606.c \
../user/bsp/src/bsp_lcd_ra8875.c \
../user/bsp/src/bsp_ra8875.c \
../user/bsp/src/bsp_tft_lcd.c \
../user/bsp/src/disp_ra8875_lvgl.c 

OBJS += \
./user/bsp/src/bsp_ad7606.o \
./user/bsp/src/bsp_lcd_ra8875.o \
./user/bsp/src/bsp_ra8875.o \
./user/bsp/src/bsp_tft_lcd.o \
./user/bsp/src/disp_ra8875_lvgl.o 

C_DEPS += \
./user/bsp/src/bsp_ad7606.d \
./user/bsp/src/bsp_lcd_ra8875.d \
./user/bsp/src/bsp_ra8875.d \
./user/bsp/src/bsp_tft_lcd.d \
./user/bsp/src/disp_ra8875_lvgl.d 


# Each subdirectory must supply rules for building sources it contributes
user/bsp/src/%.o user/bsp/src/%.su user/bsp/src/%.cyclo: ../user/bsp/src/%.c user/bsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -DLV_CONF_INCLUDE_SIMPLE -c -I../Core/Inc -I../Drivers/CMSIS/DSP/Include -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/LVGL -I../Middlewares/Third_Party/LVGL/src -I../Middlewares/Third_Party/FatFs/src -I../FATFS/App -I../FATFS/Target -I../user/app/inlcude -I../user/app/inlcude/gui -I../user/bsp/include -I../user/fonts -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-user-2f-bsp-2f-src

clean-user-2f-bsp-2f-src:
	-$(RM) ./user/bsp/src/bsp_ad7606.cyclo ./user/bsp/src/bsp_ad7606.d ./user/bsp/src/bsp_ad7606.o ./user/bsp/src/bsp_ad7606.su ./user/bsp/src/bsp_lcd_ra8875.cyclo ./user/bsp/src/bsp_lcd_ra8875.d ./user/bsp/src/bsp_lcd_ra8875.o ./user/bsp/src/bsp_lcd_ra8875.su ./user/bsp/src/bsp_ra8875.cyclo ./user/bsp/src/bsp_ra8875.d ./user/bsp/src/bsp_ra8875.o ./user/bsp/src/bsp_ra8875.su ./user/bsp/src/bsp_tft_lcd.cyclo ./user/bsp/src/bsp_tft_lcd.d ./user/bsp/src/bsp_tft_lcd.o ./user/bsp/src/bsp_tft_lcd.su ./user/bsp/src/disp_ra8875_lvgl.cyclo ./user/bsp/src/disp_ra8875_lvgl.d ./user/bsp/src/disp_ra8875_lvgl.o ./user/bsp/src/disp_ra8875_lvgl.su

.PHONY: clean-user-2f-bsp-2f-src

