################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/database/packet_buffer/packet_buffer.c 

OBJS += \
./src/database/packet_buffer/packet_buffer.o 

C_DEPS += \
./src/database/packet_buffer/packet_buffer.d 


# Each subdirectory must supply rules for building sources it contributes
src/database/packet_buffer/%.o: ../src/database/packet_buffer/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -DHASH_DEBUG=1 -Wall -c -fmessage-length=0 -DHASH_FUNCTION=HASH_BER -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


