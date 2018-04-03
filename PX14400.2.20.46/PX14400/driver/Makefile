
RHEL		:= Red Hat
MODNAME	:= sig_px14400
TARGET	:= $(MODNAME).ko
DISTRO	:=	$(shell cat /proc/version 2> /dev/null)
KERNVER  := $(shell uname -r)
KDIR     := /lib/modules/$(KERNVER)/build
INSTDIR  := /lib/modules/$(KERNVER)/extra
UDEVRULE := /etc/udev/rules.d/10-$(MODNAME).rules
AUTODIR	:=	/etc/sysconfig/modules/$(MODNAME).modules
SRCFILES := px14_main.c px14_comp.c px14_isr.c px14_dma.c px14_ioctl.c	\
            px14_regio.c px14_eeprom.c px14_jtag.c px14_timestamps.c   	\
            px14_read.c

EXTRA_CFLAGS  := -Wno-unused-variable -Wno-unused-function

ifneq ($(KERNELRELEASE),)
obj-m        := $(MODNAME).o
$(MODNAME)-y := $(SRCFILES:.c=.o)

else
.PHONY : modules install uninstall clean

modules : $(TARGET)

$(TARGET) : $(SRCFILES)
	@echo "> Building kernel-mode driver..."
	$(MYHIDE) $(MAKE) -C $(KDIR) M=$(shell pwd) modules

install : $(TARGET)
	@echo "> Installing kernel-mode driver..."
	@if [ ! -d $(INSTDIR) ]; then mkdir $(INSTDIR); fi
	@echo "KERNEL==\"$(MODNAME)*\", MODE=\"0666\"" > $(UDEVRULE)
ifneq (,$(findstring $(RHEL), $(DISTRO)))
	@echo -e "#!/bin/sh\n/sbin/modinfo -F filename $(MODNAME)"\
				">/dev/null 2>&1\nif [ ""$$""? -eq 0 ]\nthen\n"\
			   "  modprobe $(MODNAME) >/dev/null 2>&1\nfi" > $(AUTODIR)
	@chmod +x $(AUTODIR)
endif
	@install -p -m 644 $(TARGET) $(INSTDIR)
	@depmod -a

clean :
	@echo "> Cleaning kernel-mode driver..."
	$(MYHIDE) $(MAKE) -C $(KDIR) M=$(shell pwd) clean

#.PHONY : tidy distclean docs tags help

#depend :
#	$(MYHIDE) makedepend -DCONFIG_PCI -- $(CFLAGS) -- $(SRCFILES)

uninstall :
	@echo "> Uninstalling kernel-mode driver..."
	$(MYHIDE) rm -f $(INSTDIR)/$(TARGET)
	$(MYHIDE) rm -f $(UDEVRULE)
	$(MYHIDE) rm -f $(AUTODIR)

#docs : $(SRCFILES) ../libsig_px14400/px14_private.h
#	$(MYHIDE) doxygen ./Doxyfile
#
#help :
#	$(MYHIDE) $(MAKE) -C $(KDIR) M=$(shell pwd) help
#
#tidy :
#	$(MYHIDE) rm -f *~
#
#distclean : clean tidy remove_docs
#	@echo "> Completely cleaning kernel-mode driver..."
#	$(MYHIDE) rm -f TAGS
#	$(MYHIDE) rm -f Module.symvers Module.markers modules.order
#
#tags :
#	$(MYHIDE) etags $(SRCFILES) px14_drv.h ../libsig_px14400/px14_private.h

endif
