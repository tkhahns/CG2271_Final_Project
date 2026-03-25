################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../board/board.c \
../board/clock_config.c \
../board/peripherals.c \
../board/pin_mux.c 

C_DEPS += \
./board/board.d \
./board/clock_config.d \
./board/peripherals.d \
./board/pin_mux.d 

OBJS += \
./board/board.o \
./board/clock_config.o \
./board/peripherals.o \
./board/pin_mux.o 


# Each subdirectory must supply rules for building sources it contributes
board/%.o: ../board/%.c board/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -D__REDLIB__ -DCPU_MCXC444VLH -DCPU_MCXC444VLH_cm0plus -DSDK_OS_BAREMETAL -DSERIAL_PORT_TYPE_UART=1 -DSDK_DEBUGCONSOLE=1 -DCR_INTEGER_PRINTF -DPRINTF_FLOAT_ENABLE=0 -DSDK_DEBUGCONSOLE_UART -DSDK_OS_FREE_RTOS -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/board" -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/source" -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/drivers" -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/CMSIS" -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/CMSIS/m-profile" -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/utilities" -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/utilities/debug_console/config" -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/device" -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/device/periph2" -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/utilities/debug_console" -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/component/serial_manager" -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/component/lists" -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/utilities/str" -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/component/uart" -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/freertos/freertos-kernel/include" -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/freertos/freertos-kernel/portable/GCC/ARM_CM0" -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/freertos/freertos-kernel/template" -I"/Users/ethanlam/Documents/MCUXpressoIDE_25.6.136/workspace/Final_Project_CG2271/freertos/freertos-kernel/template/ARM_CM0" -O0 -fno-common -g3 -gdwarf-4 -Wall -c -ffunction-sections -fdata-sections -fno-builtin -fmerge-constants -fmacro-prefix-map="$(<D)/"= -mcpu=cortex-m0plus -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-board

clean-board:
	-$(RM) ./board/board.d ./board/board.o ./board/clock_config.d ./board/clock_config.o ./board/peripherals.d ./board/peripherals.o ./board/pin_mux.d ./board/pin_mux.o

.PHONY: clean-board

