################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../CodeSegments.c \
../main.c \
../rom.c 

OBJS += \
./CodeSegments.o \
./main.o \
./rom.o 

C_DEPS += \
./CodeSegments.d \
./main.d \
./rom.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I"/home/rjhender/git/R4300-code-analysis/r4300-code-analysis" -I"/home/rjhender/git/R4300-code-analysis/r4300-code-analysis/InstructionSets" -I"/home/rjhender/git/R4300-code-analysis/r4300-code-analysis/Translations" -I"/home/rjhender/git/R4300-code-analysis/r4300-code-analysis/Debugger" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


