# !!
# this makefile needs the env variables TOP and TREE pointing into the 4758
# linux distribution

include ../header.make

include $(PIR)/utils.make
include $(PIR)/header.make



SUBDIRS:=stream



ifdef HAVE_4758_CRYPTO
# done in header.make
#	CPPFLAGS += -I$(4758_KERNEL_INCLUDE)
endif
ifdef COMM_SCC
	CPPFLAGS += -I$(SCC_INCLUDE)
endif


# CPPFLAGS += -DNDEBUG

ifdef RUNONCARD
	CFLAGS += -march=i486
endif

DESTDIR=$(LEEDS_BIN)

LIBSRCS=array.cc utils.cc batcher-permute.cc batcher-network.cc 
SRCS=run-circuit.cc enc-circuit.cc prep-circuit.cc $(LIBSRCS)

TESTSRCS=$(wildcard test-*.cc)


# LDFLAGS+=-static


# the headers for the 4758 crypto interface are in the IBM patched kernel source
#4758_KERNEL_INCLUDE = $(TOP)/$(TREE)/card/linux-2.2.17/drivers

# SCC communication headers
SCC_INCLUDE = $(TOP)/$(TREE)/include

ifdef HAVE_OPENSSL
LDLIBS	    += -lssl -lcrypto
CPPFLAGS    += -DHAVE_OPENSSL
endif

LIB = sfdl-card

LIBFILE = lib$(LIB).$(LIBEXT)
EXES = runtime enc-circuit prep-circuit

TARGETS=$(LIBFILE) $(EXES)

all: subdirs build


lib$(LIB).so: $(LIBOBJS)
	$(CXXLINK)
lib$(LIB).a: $(LIBOBJS)
	ar -ru $@ $^


-l$(LIB): $(LIBFILE)


build: $(TARGETS)

install: subdirs build
#	strip $(EXES)
	$(INSTALL) $(LIBFILE)	$(LEEDS_LIB)
	$(INSTALL) $(EXES)	$(LEEDS_BIN)


# external libraries. they get added into LDLIBS in common.make
LIBDIRS		+= $(LEEDS_LIB) . ../common
LDLIBFILES	+= -lcard -lsfdl-common -lcommon -lcard-stream

vpath %.so $(LIBDIRS)
vpath %.a $(LIBDIRS)

$(EXES) : LDLIBFILES := -lsfdl-card $(LDLIBFILES)
$(EXES): -lsfdl-card


runtime: run-circuit.o $(LDLIBFILES)
	$(CXXLINK)


$(TESTEXES): $(LIBOBJS)

tests: $(TESTEXES)



subdirs: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: subdirs $(SUBDIRS)


# give the dependency generation step all the include dirs
dep : CPPFLAGS += $(CCT_FLAGS)

include ../footer.make
