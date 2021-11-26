# Select the LEDs model to be compiled in the program

ifeq ($(V_IOBOARD),bordeaux)
OBJS += leds_model_bordeaux.o
endif

ifeq ($(V_IOBOARD),$(filter $(V_IOBOARD),nrb0206 lark))
OBJS += leds_model_nrb0206.o
endif
