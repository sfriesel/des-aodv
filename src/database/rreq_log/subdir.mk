################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/database/rreq_log/rreq_log.c 

OBJS += \
./src/database/rreq_log/rreq_log.o 

C_DEPS += \
./src/database/rreq_log/rreq_log.d 


# Each subdirectory must supply rules for building sources it contributes
src/database/rreq_log/%.o: ../src/database/rreq_log/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -DHASH_DEBUG=1 -Wall -c -fmessage-length=0 -DHASH_FUNCTION=HASH_BER -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


