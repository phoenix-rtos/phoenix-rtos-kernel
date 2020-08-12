#
# Makefile for Phoenix-RTOS kernel (ARMv7 HAL)
#
# Copyright 2016-2017, 2019-2020 Phoenix Systems
#

OBJS += $(addprefix $(PREFIX_O)hal/armv7/, spinlock.o cpu.o hal.o exceptions.o syspage.o pmap.o)


ifneq (, $(findstring stm32, $(TARGET)))
	OBJS += $(addprefix $(PREFIX_O)hal/armv7/, _init-stm32.o interrupts-stm32.o timer-stm32.o)
	
	ifneq (, $(findstring l1, $(TARGET)))
		OBJS += $(addprefix $(PREFIX_O)hal/armv7/, stm32l1.o console-stm32l1.o)
	else
		OBJS += $(addprefix $(PREFIX_O)hal/armv7/, stm32l4.o console-stm32l4.o)
	endif

else ifneq (, $(findstring imxrt, $(TARGET)))
	OBJS += $(addprefix $(PREFIX_O)hal/armv7/, _init-imxrt.o interrupts-imxrt.o timer-imxrt.o)
	ifneq (, $(findstring 1170, $(TARGET)))
		OBJS += $(addprefix $(PREFIX_O)hal/armv7/, imxrt1170.o console-imxrt1170.o)
	else
		OBJS += $(addprefix $(PREFIX_O)hal/armv7/, imxrt.o console-imxrt.o)
	endif
endif
