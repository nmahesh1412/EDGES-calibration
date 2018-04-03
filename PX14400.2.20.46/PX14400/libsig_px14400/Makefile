# By 	Mike DeKoker
# 		W.K. Poh

LIBRARY		:= libsig_px14400
HEADER		:= px14.h
VERSION		:= 2
MINOR			:= 3
LIBDESTDIR	:= /usr/lib
HDRDESTDIR	:= /usr/local/include
HDRSRCDIR	:= .

LINKNAME		:= $(LIBRARY).so
SONAME		:= $(LINKNAME).$(VERSION)
TARGET		:= $(SONAME).$(MINOR)
MYCPPEXTRA	:= -Wno-unknown-pragmas -fno-strict-aliasing
CPPFLAGS		+= -Wall $(MYCPPEXTRA) -O2 `xml2-config --cflags` -fPIC
#CPPFLAGS	+= -Wall -g $(MYCPPEXTRA) `xml2-config --cflags` -fPIC

MYSRCFILES	:= px14.cpp px14_acquire.cpp px14_bootbuf.cpp px14_clock.cpp \
					px14_dmabuf.cpp px14_file_io.cpp px14_fixed_logic.cpp \
					px14_fw.cpp px14_fw_patch_32p.cpp px14_fwctx.cpp \
					px14_hw_set.cpp px14_jtag.cpp px14_nulloff.cpp \
					px14_plat.cpp px14_record.cpp px14_recth_imp.cpp \
					px14_recth_pcibuf.cpp px14_recth_pcibuf_chained.cpp \
					px14_recth_ramacq.cpp px14_reg_io.cpp px14_remote.cpp \
					px14_srd_file.cpp px14_timestamp.cpp px14_unicode.cpp \
					px14_versions.cpp px14_virtual.cpp px14_volt_rng.cpp \
					px14_xfer.cpp px14_xml.cpp px14_xsvf_player.cpp \
					px14_zip.cpp sig_srd_file.cpp sig_xsvf_player.cpp \
					sigsvc_utility.cpp

SRC_OBJS		:= $(subst .cpp,.o,$(MYSRCFILES))

all : $(TARGET)

.PHONY : all install uninstall clean distclean tidy docs tags depend
.PHONY : dump_exports dump_fnames dump_missing_funcs list_targets

list_targets :
	@echo "This makefile contains the following explicit targets:"
	@echo "all install uninstall clean distclean tidy docs tags depend"
	@echo "dump_exports dump_fnames dump_missing_funcs list_targets"

title :
	@echo "> Building user-mode library..."

%.o : %.cpp
	@echo "  Compiling" $<...
	$(MYHIDE) $(CXX) $(CXXFLAGS) $(CPPFLAGS) $(INCLUDES) $(TARGET_ARCH) -c $< -o $@

$(TARGET) : title $(SRC_OBJS)
	$(MYHIDE) g++ -shared -Wl,-soname,$(SONAME) -o $@ $(SRC_OBJS) -lc -lpthread `xml2-config --libs` -lrt

install : $(TARGET)
	@echo "> Installing user-mode library..."
# Install library
	$(MYHIDE) install -d $(LIBDESTDIR)
	$(MYHIDE) install -c $(TARGET) $(LIBDESTDIR)
	$(MYHIDE) ln -sf $(LIBDESTDIR)/$(TARGET) $(LIBDESTDIR)/$(LINKNAME)
	$(MYHIDE) ln -sf $(LIBDESTDIR)/$(TARGET) $(LIBDESTDIR)/$(SONAME)
	$(MYHIDE) /sbin/ldconfig
# Install header
	$(MYHIDE) install -d $(HDRDESTDIR)
	$(MYHIDE) install --mode=664 -c $(HDRSRCDIR)/$(HEADER) $(HDRDESTDIR)

uninstall :
	@echo "> Uninstalling user-mode library..."
# Uninstall library
	$(MYHIDE) rm -f $(LIBDESTDIR)/$(TARGET)
	$(MYHIDE) rm -f $(LIBDESTDIR)/$(LINKNAME)
	$(MYHIDE) rm -f $(LIBDESTDIR)/$(SONAME)
	$(MYHIDE) /sbin/ldconfig
# Uninstall header
	$(MYHIDE) rm -f $(HDRDESTDIR)/$(HEADER)

depend :
	makedepend -I$(HDRDESTDIR) -- $(CPPFLAGS) -- $(MYSRCFILES)

tags :
	$(MYHIDE) etags *.cpp *.h

clean :
	@echo "> Cleaning user-mode library..."
	$(MYHIDE) rm -f *.o *~ core $(TARGET) TAGS
	$(MYHIDE) rm -rf ./doc

tidy :
	$(MYHIDE) rm -f *~

docs :
	$(MYHIDE) doxygen ./Doxyfile

distclean : clean

dump_exports : $(TARGET)
	@nm -g $(TARGET) | grep -o " .*PX14$$" | cut -b4- | sort

dump_fnames :
	@grep -o "PX14API [[:alnum:]]*PX14" pda16.h | cut -b8- | sort

dump_missing_funcs : $(TARGET)
	@nm -g $(TARGET) | grep -o " .*PX14$$" | cut -b4- | sort > EXPORTS.MY
	@grep -o "PX14API [[:alnum:]]*PX14" px14.h | cut -b8- | sort > PROTOS.MY
	@comm -2 -3 PROTOS.MY EXPORTS.MY
	@rm -f ./EXPORTS.MY ./PROTOS.MY

