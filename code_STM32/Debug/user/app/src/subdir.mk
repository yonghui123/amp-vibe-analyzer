################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../user/app/src/acq_pipeline.c \
../user/app/src/acq_task.c \
../user/app/src/alarm_manager.c \
../user/app/src/channel_data.c \
../user/app/src/form_dso_2ch.c \
../user/app/src/form_dso_xy.c \
../user/app/src/param.c \
../user/app/src/signal_algo.c \
../user/app/src/touch_bsp_calib.c \
../user/app/src/touch_bsp_test.c \
../user/app/src/touch_bsp_touch.c \
../user/app/src/touch_calib.c \
../user/app/src/wave_disp_2ch.c \
../user/app/src/wave_disp_xy.c 

OBJS += \
./user/app/src/acq_pipeline.o \
./user/app/src/acq_task.o \
./user/app/src/alarm_manager.o \
./user/app/src/channel_data.o \
./user/app/src/form_dso_2ch.o \
./user/app/src/form_dso_xy.o \
./user/app/src/param.o \
./user/app/src/signal_algo.o \
./user/app/src/touch_bsp_calib.o \
./user/app/src/touch_bsp_test.o \
./user/app/src/touch_bsp_touch.o \
./user/app/src/touch_calib.o \
./user/app/src/wave_disp_2ch.o \
./user/app/src/wave_disp_xy.o 

C_DEPS += \
./user/app/src/acq_pipeline.d \
./user/app/src/acq_task.d \
./user/app/src/alarm_manager.d \
./user/app/src/channel_data.d \
./user/app/src/form_dso_2ch.d \
./user/app/src/form_dso_xy.d \
./user/app/src/param.d \
./user/app/src/signal_algo.d \
./user/app/src/touch_bsp_calib.d \
./user/app/src/touch_bsp_test.d \
./user/app/src/touch_bsp_touch.d \
./user/app/src/touch_calib.d \
./user/app/src/wave_disp_2ch.d \
./user/app/src/wave_disp_xy.d 


# Each subdirectory must supply rules for building sources it contributes
user/app/src/%.o user/app/src/%.su user/app/src/%.cyclo: ../user/app/src/%.c user/app/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -DLV_CONF_INCLUDE_SIMPLE -c -I../Core/Inc -I../Drivers/CMSIS/DSP/Include -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/LVGL -I../Middlewares/Third_Party/LVGL/src -I../user/app/inlcude -I../user/app/inlcude/gui -I../user/bsp/include -I../user/fonts -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-user-2f-app-2f-src

clean-user-2f-app-2f-src:
	-$(RM) ./user/app/src/acq_pipeline.cyclo ./user/app/src/acq_pipeline.d ./user/app/src/acq_pipeline.o ./user/app/src/acq_pipeline.su ./user/app/src/acq_task.cyclo ./user/app/src/acq_task.d ./user/app/src/acq_task.o ./user/app/src/acq_task.su ./user/app/src/alarm_manager.cyclo ./user/app/src/alarm_manager.d ./user/app/src/alarm_manager.o ./user/app/src/alarm_manager.su ./user/app/src/channel_data.cyclo ./user/app/src/channel_data.d ./user/app/src/channel_data.o ./user/app/src/channel_data.su ./user/app/src/form_dso_2ch.cyclo ./user/app/src/form_dso_2ch.d ./user/app/src/form_dso_2ch.o ./user/app/src/form_dso_2ch.su ./user/app/src/form_dso_xy.cyclo ./user/app/src/form_dso_xy.d ./user/app/src/form_dso_xy.o ./user/app/src/form_dso_xy.su ./user/app/src/param.cyclo ./user/app/src/param.d ./user/app/src/param.o ./user/app/src/param.su ./user/app/src/signal_algo.cyclo ./user/app/src/signal_algo.d ./user/app/src/signal_algo.o ./user/app/src/signal_algo.su ./user/app/src/touch_bsp_test.cyclo ./user/app/src/touch_bsp_test.d ./user/app/src/touch_bsp_test.o ./user/app/src/touch_bsp_test.su ./user/app/src/touch_calib.cyclo ./user/app/src/touch_calib.d ./user/app/src/touch_calib.o ./user/app/src/touch_calib.su ./user/app/src/wave_disp_2ch.cyclo ./user/app/src/wave_disp_2ch.d ./user/app/src/wave_disp_2ch.o ./user/app/src/wave_disp_2ch.su ./user/app/src/wave_disp_xy.cyclo ./user/app/src/wave_disp_xy.d ./user/app/src/wave_disp_xy.o ./user/app/src/wave_disp_xy.su

.PHONY: clean-user-2f-app-2f-src

