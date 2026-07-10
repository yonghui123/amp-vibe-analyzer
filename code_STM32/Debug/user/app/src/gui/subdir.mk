################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../user/app/src/gui/gui_core.c \
../user/app/src/gui/gui_calib.c \
../user/app/src/gui/gui_widgets.c \
../user/app/src/gui/screen_acq_settings.c \
../user/app/src/gui/screen_alarm_records.c \
../user/app/src/gui/screen_channel_setup.c \
../user/app/src/gui/screen_main_display.c \
../user/app/src/gui/screen_touch_test.c 

OBJS += \
./user/app/src/gui/gui_core.o \
./user/app/src/gui/gui_calib.o \
./user/app/src/gui/gui_widgets.o \
./user/app/src/gui/screen_acq_settings.o \
./user/app/src/gui/screen_alarm_records.o \
./user/app/src/gui/screen_channel_setup.o \
./user/app/src/gui/screen_main_display.o \
./user/app/src/gui/screen_touch_test.o 

C_DEPS += \
./user/app/src/gui/gui_core.d \
./user/app/src/gui/gui_calib.d \
./user/app/src/gui/gui_widgets.d \
./user/app/src/gui/screen_acq_settings.d \
./user/app/src/gui/screen_alarm_records.d \
./user/app/src/gui/screen_channel_setup.d \
./user/app/src/gui/screen_main_display.d \
./user/app/src/gui/screen_touch_test.d 


# Each subdirectory must supply rules for building sources it contributes
user/app/src/gui/%.o user/app/src/gui/%.su user/app/src/gui/%.cyclo: ../user/app/src/gui/%.c user/app/src/gui/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -DLV_CONF_INCLUDE_SIMPLE -c -I../Core/Inc -I../Drivers/CMSIS/DSP/Include -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/LVGL -I../Middlewares/Third_Party/LVGL/src -I../user/app/inlcude -I../user/app/inlcude/gui -I../user/bsp/include -I../user/fonts -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-user-2f-app-2f-src-2f-gui

clean-user-2f-app-2f-src-2f-gui:
	-$(RM) ./user/app/src/gui/gui_core.cyclo ./user/app/src/gui/gui_core.d ./user/app/src/gui/gui_core.o ./user/app/src/gui/gui_core.su ./user/app/src/gui/gui_calib.cyclo ./user/app/src/gui/gui_calib.d ./user/app/src/gui/gui_calib.o ./user/app/src/gui/gui_calib.su ./user/app/src/gui/gui_widgets.cyclo ./user/app/src/gui/gui_widgets.d ./user/app/src/gui/gui_widgets.o ./user/app/src/gui/gui_widgets.su ./user/app/src/gui/screen_acq_settings.cyclo ./user/app/src/gui/screen_acq_settings.d ./user/app/src/gui/screen_acq_settings.o ./user/app/src/gui/screen_acq_settings.su ./user/app/src/gui/screen_alarm_records.cyclo ./user/app/src/gui/screen_alarm_records.d ./user/app/src/gui/screen_alarm_records.o ./user/app/src/gui/screen_alarm_records.su ./user/app/src/gui/screen_channel_setup.cyclo ./user/app/src/gui/screen_channel_setup.d ./user/app/src/gui/screen_channel_setup.o ./user/app/src/gui/screen_channel_setup.su ./user/app/src/gui/screen_main_display.cyclo ./user/app/src/gui/screen_main_display.d ./user/app/src/gui/screen_main_display.o ./user/app/src/gui/screen_main_display.su ./user/app/src/gui/screen_touch_test.cyclo ./user/app/src/gui/screen_touch_test.d ./user/app/src/gui/screen_touch_test.o ./user/app/src/gui/screen_touch_test.su

.PHONY: clean-user-2f-app-2f-src-2f-gui

