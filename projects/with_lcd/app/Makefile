# Makefile for a custom application comprised of 
# a loable kernel module and a user space program
# (or a set of related modules and user space programs).
# If your application doesn't need to have a kernel module or
# a user space program, edit the two goals below to
# exclude one or the other. 
all		: modules apps
clean		: clean_modules clean_apps

# Path to the kernel modules directory in context of which
# these loadable modules are built
KERNELDIR	:=  $(INSTALL_ROOT)/linux

# These are flags/tools used to build user-space programs
CFLAGS		:= -Os -mcpu=cortex-m3 -mthumb -I$(KERNELDIR)/include
LDFLAGS		:= -mcpu=cortex-m3 -mthumb
CC		= $(CROSS_COMPILE_APPS)gcc

# Edit the line below to modify a set of user-space programs
# you need to build 
APPS		= lcd_tool lcd_menu 
apps		: $(APPS)	

lcd_tool	: lcd_tool.o lcd_api.o lcd_font.o
lcd_menu	: lcd_menu.o lcd_api.o lcd_font.o

# Clean-up after user-space programs
clean_apps	:
	-rm -f $(APPS) *.gdb *.o

# Edit the line below to modify a set of loadable modules
# you need to build 
obj-m		+= coretftfb.o coretftfb_dev.o

# Define dependencies for a particular module
# sample-y	:= some.o

# Build the modules in this directory
modules		:
	make -C $(KERNELDIR) M=`pwd` modules CFLAGS="" LDFLAGS=""

# Clean-up after modules
clean_modules:
	make -C $(KERNELDIR) M=`pwd` clean
	rm -f modules.order

