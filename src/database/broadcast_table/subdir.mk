################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/database/broadcast_table/aodv_broadcast_t.c 

OBJS += \
./src/database/broadcast_table/aodv_broadcast_t.o 

C_DEPS += \
./src/database/broadcast_table/aodv_broadcast_t.d 


# Each subdirectory must supply rules for building sources it contributes
src/database/broadcast_table/%.o: ../src/database/broadcast_table/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -DHASH_DEBUG=1 -Wall -c -fmessage-length=0 -DHASH_FUNCTION=HASH_BER -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


