################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../user/app/src/lvgl_wrappers/lvgl_cal_dd.c \
../user/app/src/lvgl_wrappers/lvgl_core.c \
../user/app/src/lvgl_wrappers/lvgl_draw.c \
../user/app/src/lvgl_wrappers/lvgl_draw_sw.c \
../user/app/src/lvgl_wrappers/lvgl_extra.c \
../user/app/src/lvgl_wrappers/lvgl_font.c \
../user/app/src/lvgl_wrappers/lvgl_font_ms10.c \
../user/app/src/lvgl_wrappers/lvgl_font_ms12.c \
../user/app/src/lvgl_wrappers/lvgl_font_ms14.c \
../user/app/src/lvgl_wrappers/lvgl_font_ms16.c \
../user/app/src/lvgl_wrappers/lvgl_hal.c \
../user/app/src/lvgl_wrappers/lvgl_misc.c \
../user/app/src/lvgl_wrappers/lvgl_w_btnmatrix.c \
../user/app/src/lvgl_wrappers/lvgl_w_dropdown.c \
../user/app/src/lvgl_wrappers/lvgl_w_label.c \
../user/app/src/lvgl_wrappers/lvgl_w_roller.c \
../user/app/src/lvgl_wrappers/lvgl_w_switch.c \
../user/app/src/lvgl_wrappers/lvgl_w_table.c \
../user/app/src/lvgl_wrappers/lvgl_widgets.c 

OBJS += \
./user/app/src/lvgl_wrappers/lvgl_cal_dd.o \
./user/app/src/lvgl_wrappers/lvgl_core.o \
./user/app/src/lvgl_wrappers/lvgl_draw.o \
./user/app/src/lvgl_wrappers/lvgl_draw_sw.o \
./user/app/src/lvgl_wrappers/lvgl_extra.o \
./user/app/src/lvgl_wrappers/lvgl_font.o \
./user/app/src/lvgl_wrappers/lvgl_font_ms10.o \
./user/app/src/lvgl_wrappers/lvgl_font_ms12.o \
./user/app/src/lvgl_wrappers/lvgl_font_ms14.o \
./user/app/src/lvgl_wrappers/lvgl_font_ms16.o \
./user/app/src/lvgl_wrappers/lvgl_hal.o \
./user/app/src/lvgl_wrappers/lvgl_misc.o \
./user/app/src/lvgl_wrappers/lvgl_w_btnmatrix.o \
./user/app/src/lvgl_wrappers/lvgl_w_dropdown.o \
./user/app/src/lvgl_wrappers/lvgl_w_label.o \
./user/app/src/lvgl_wrappers/lvgl_w_roller.o \
./user/app/src/lvgl_wrappers/lvgl_w_switch.o \
./user/app/src/lvgl_wrappers/lvgl_w_table.o \
./user/app/src/lvgl_wrappers/lvgl_widgets.o 

C_DEPS += \
./user/app/src/lvgl_wrappers/lvgl_cal_dd.d \
./user/app/src/lvgl_wrappers/lvgl_core.d \
./user/app/src/lvgl_wrappers/lvgl_draw.d \
./user/app/src/lvgl_wrappers/lvgl_draw_sw.d \
./user/app/src/lvgl_wrappers/lvgl_extra.d \
./user/app/src/lvgl_wrappers/lvgl_font.d \
./user/app/src/lvgl_wrappers/lvgl_font_ms10.d \
./user/app/src/lvgl_wrappers/lvgl_font_ms12.d \
./user/app/src/lvgl_wrappers/lvgl_font_ms14.d \
./user/app/src/lvgl_wrappers/lvgl_font_ms16.d \
./user/app/src/lvgl_wrappers/lvgl_hal.d \
./user/app/src/lvgl_wrappers/lvgl_misc.d \
./user/app/src/lvgl_wrappers/lvgl_w_btnmatrix.d \
./user/app/src/lvgl_wrappers/lvgl_w_dropdown.d \
./user/app/src/lvgl_wrappers/lvgl_w_label.d \
./user/app/src/lvgl_wrappers/lvgl_w_roller.d \
./user/app/src/lvgl_wrappers/lvgl_w_switch.d \
./user/app/src/lvgl_wrappers/lvgl_w_table.d \
./user/app/src/lvgl_wrappers/lvgl_widgets.d 


# Each subdirectory must supply rules for building sources it contributes
user/app/src/lvgl_wrappers/%.o user/app/src/lvgl_wrappers/%.su user/app/src/lvgl_wrappers/%.cyclo: ../user/app/src/lvgl_wrappers/%.c user/app/src/lvgl_wrappers/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -DLV_CONF_INCLUDE_SIMPLE -c -I../Core/Inc -I../Drivers/CMSIS/DSP/Include -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/LVGL -I../Middlewares/Third_Party/LVGL/src -I../user/app/inlcude -I../user/app/inlcude/gui -I../user/bsp/include -I../user/fonts -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-user-2f-app-2f-src-2f-lvgl_wrappers

clean-user-2f-app-2f-src-2f-lvgl_wrappers:
	-$(RM) ./user/app/src/lvgl_wrappers/lvgl_cal_dd.cyclo ./user/app/src/lvgl_wrappers/lvgl_cal_dd.d ./user/app/src/lvgl_wrappers/lvgl_cal_dd.o ./user/app/src/lvgl_wrappers/lvgl_cal_dd.su ./user/app/src/lvgl_wrappers/lvgl_core.cyclo ./user/app/src/lvgl_wrappers/lvgl_core.d ./user/app/src/lvgl_wrappers/lvgl_core.o ./user/app/src/lvgl_wrappers/lvgl_core.su ./user/app/src/lvgl_wrappers/lvgl_draw.cyclo ./user/app/src/lvgl_wrappers/lvgl_draw.d ./user/app/src/lvgl_wrappers/lvgl_draw.o ./user/app/src/lvgl_wrappers/lvgl_draw.su ./user/app/src/lvgl_wrappers/lvgl_draw_sw.cyclo ./user/app/src/lvgl_wrappers/lvgl_draw_sw.d ./user/app/src/lvgl_wrappers/lvgl_draw_sw.o ./user/app/src/lvgl_wrappers/lvgl_draw_sw.su ./user/app/src/lvgl_wrappers/lvgl_extra.cyclo ./user/app/src/lvgl_wrappers/lvgl_extra.d ./user/app/src/lvgl_wrappers/lvgl_extra.o ./user/app/src/lvgl_wrappers/lvgl_extra.su ./user/app/src/lvgl_wrappers/lvgl_font.cyclo ./user/app/src/lvgl_wrappers/lvgl_font.d ./user/app/src/lvgl_wrappers/lvgl_font.o ./user/app/src/lvgl_wrappers/lvgl_font.su ./user/app/src/lvgl_wrappers/lvgl_font_ms10.cyclo ./user/app/src/lvgl_wrappers/lvgl_font_ms10.d ./user/app/src/lvgl_wrappers/lvgl_font_ms10.o ./user/app/src/lvgl_wrappers/lvgl_font_ms10.su ./user/app/src/lvgl_wrappers/lvgl_font_ms12.cyclo ./user/app/src/lvgl_wrappers/lvgl_font_ms12.d ./user/app/src/lvgl_wrappers/lvgl_font_ms12.o ./user/app/src/lvgl_wrappers/lvgl_font_ms12.su ./user/app/src/lvgl_wrappers/lvgl_font_ms14.cyclo ./user/app/src/lvgl_wrappers/lvgl_font_ms14.d ./user/app/src/lvgl_wrappers/lvgl_font_ms14.o ./user/app/src/lvgl_wrappers/lvgl_font_ms14.su ./user/app/src/lvgl_wrappers/lvgl_font_ms16.cyclo ./user/app/src/lvgl_wrappers/lvgl_font_ms16.d ./user/app/src/lvgl_wrappers/lvgl_font_ms16.o ./user/app/src/lvgl_wrappers/lvgl_font_ms16.su ./user/app/src/lvgl_wrappers/lvgl_hal.cyclo ./user/app/src/lvgl_wrappers/lvgl_hal.d ./user/app/src/lvgl_wrappers/lvgl_hal.o ./user/app/src/lvgl_wrappers/lvgl_hal.su ./user/app/src/lvgl_wrappers/lvgl_misc.cyclo ./user/app/src/lvgl_wrappers/lvgl_misc.d ./user/app/src/lvgl_wrappers/lvgl_misc.o ./user/app/src/lvgl_wrappers/lvgl_misc.su ./user/app/src/lvgl_wrappers/lvgl_w_btnmatrix.cyclo ./user/app/src/lvgl_wrappers/lvgl_w_btnmatrix.d ./user/app/src/lvgl_wrappers/lvgl_w_btnmatrix.o ./user/app/src/lvgl_wrappers/lvgl_w_btnmatrix.su ./user/app/src/lvgl_wrappers/lvgl_w_dropdown.cyclo ./user/app/src/lvgl_wrappers/lvgl_w_dropdown.d ./user/app/src/lvgl_wrappers/lvgl_w_dropdown.o ./user/app/src/lvgl_wrappers/lvgl_w_dropdown.su ./user/app/src/lvgl_wrappers/lvgl_w_label.cyclo ./user/app/src/lvgl_wrappers/lvgl_w_label.d ./user/app/src/lvgl_wrappers/lvgl_w_label.o ./user/app/src/lvgl_wrappers/lvgl_w_label.su ./user/app/src/lvgl_wrappers/lvgl_w_roller.cyclo ./user/app/src/lvgl_wrappers/lvgl_w_roller.d ./user/app/src/lvgl_wrappers/lvgl_w_roller.o ./user/app/src/lvgl_wrappers/lvgl_w_roller.su ./user/app/src/lvgl_wrappers/lvgl_w_switch.cyclo ./user/app/src/lvgl_wrappers/lvgl_w_switch.d ./user/app/src/lvgl_wrappers/lvgl_w_switch.o ./user/app/src/lvgl_wrappers/lvgl_w_switch.su ./user/app/src/lvgl_wrappers/lvgl_w_table.cyclo ./user/app/src/lvgl_wrappers/lvgl_w_table.d ./user/app/src/lvgl_wrappers/lvgl_w_table.o ./user/app/src/lvgl_wrappers/lvgl_w_table.su ./user/app/src/lvgl_wrappers/lvgl_widgets.cyclo ./user/app/src/lvgl_wrappers/lvgl_widgets.d ./user/app/src/lvgl_wrappers/lvgl_widgets.o ./user/app/src/lvgl_wrappers/lvgl_widgets.su

.PHONY: clean-user-2f-app-2f-src-2f-lvgl_wrappers

