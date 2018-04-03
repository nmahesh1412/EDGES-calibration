
ifeq ($(VERBOSE_MAKE),1)
MYHIDE :=
MYMAKEOPTS := -k
else
MYHIDE :=@
MYMAKEOPTS := --no-print-directory -k MYHIDE=$(MYHIDE)
endif

SUBDIRS    = driver libsig_px14400
INSTDIRS   = driver libsig_px14400
DOCDIRS    = driver libsig_px14400
UNINSTDIRS = driver libsig_px14400
UTILDIRS   =
EXAMPLEDIRS= examples/PciAcqPX14 examples/RamAcqPX14 examples/StandbyPX14
#EXAMPLEDIRS= examples/PciAcqPX14 examples/RamAcqPX14 examples/StandbyPX14 examples/FirmwareUpg
CLEANDIRS  = driver libsig_px14400 $(UTILDIRS) $(EXAMPLEDIRS)
DEPDONE    = depdone__

ifeq ($(wildcard $(CURDIR)/$(DEPDONE)),)
# only define UBDISTRO & RHDISTRO if depdone is missing
UBDISTRO := $(shell cat /etc/*release|grep -i 'ubuntu')
RHDISTRO := $(shell cat /etc/*release|grep -i 'red[ ]*hat')
endif

.PHONY : all subdirs clean tidy install uninstall
.PHONY : doc examples

all: subdirs

$(DEPDONE):
	@echo "------------------------"
	@echo " Setting up environment "
	@echo "------------------------"
ifdef UBDISTRO
	@sudo sudo apt-get install build-essential libxml2-dev -y 2>/dev/null
	@touch $(DEPDONE)
endif
ifdef RHDISTRO
	@sudo yum install gcc gcc-c++ kernel-devel-$(shell uname -r) libxml2-devel -y 2>/dev/null
	@touch $(DEPDONE)
endif

subdirs: $(DEPDONE)
	@echo "---------------------------"
	@echo " Building PX14400 software "
	@echo "---------------------------"
	$(MYHIDE) for n in $(SUBDIRS); do $(MAKE) $(MYMAKEOPTS) -C $$n || exit 1; done

clean:
	@echo "---------------------------"
	@echo " Cleaning PX14400 software "
	@echo "---------------------------"
	$(MYHIDE) rm -f *.o *~
	$(MYHIDE) for n in $(CLEANDIRS); do $(MAKE) $(MYMAKEOPTS) -C $$n clean; done
	@rm -f $(DEPDONE)

install:
	@echo "-----------------------------"
	@echo " Installing PX14400 software "
	@echo "-----------------------------"
	$(MYHIDE) for n in $(INSTDIRS); do $(MAKE) $(MYMAKEOPTS) -C $$n install; done

uninstall:
	@echo "-------------------------------"
	@echo " Uninstalling PX14400 software "
	@echo "-------------------------------"
	$(MYHIDE) for n in $(UNINSTDIRS); do $(MAKE) $(MYMAKEOPTS) -C $$n uninstall; done

examples:
	@echo "---------------------------"
	@echo " Building PX14400 examples "
	@echo "---------------------------"
	$(MYHIDE) for n in $(EXAMPLEDIRS); do $(MAKE) $(MYMAKEOPTS) -C $$n; done

doc:
	$(MYHIDE) for n in $(DOCDIRS); do $(MAKE) $(MYMAKEOPTS) -C $$n doc; done


