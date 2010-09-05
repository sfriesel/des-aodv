################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/database/neighbor_table/nt.c 

OBJS += \
./src/database/neighbor_table/nt.o 

C_DEPS += \
./src/database/neighbor_table/nt.d 


# Each subdirectory must supply rules for building sources it contributes
src/database/neighbor_table/%.o: ../src/database/neighbor_table/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -DHASH_DEBUG=1 -Wall -c -fmessage-length=0 -DHASH_FUNCTION=HASH_BER -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


