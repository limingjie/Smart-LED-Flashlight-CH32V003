all : flash

TARGET:=flashlight
ADDITIONAL_C_FILES:=button.c

TARGET_MCU?=CH32V003
include ./ch32fun/ch32fun.mk

flash : cv_flash
clean : cv_clean
