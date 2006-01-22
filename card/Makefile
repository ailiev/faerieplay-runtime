# !!
# this makefile needs the ENV variables TOP and TREE pointing into the 4758
# linux distribution

include ../header.make

include $(PIR)/common.make


CPPFLAGS += -I$(4758_KERNEL_INCLUDE) -I$(SCC_INCLUDE)

# CPPFLAGS += -DNDEBUG

CFLAGS += #-O -march=i486

DESTDIR=$(LEEDS_BIN)

LIBSRCS=array.cc utils.cc batcher-permute.cc batcher-network.cc 
SRCS=run-circuit.cc enc-circuit.cc $(LIBSRCS)


# LDFLAGS+=-static


# the headers for the 4758 crypto interface are in the IBM patched kernel source
4758_KERNEL_INCLUDE = $(TOP)/$(TREE)/card/linux-2.2.17/drivers
# SCC communication headers
SCC_INCLUDE = $(TOP)/$(TREE)/include

#ifndef NOPENSSL
LDLIBS += -lssl -lcrypto
#endif

ifdef NOPENSSL
CPPFLAGS += -D_NO_OPENSSL
endif


TARGETS=runtime enc-circuit

all: build

build: $(TARGETS)

install: build
#	strip $(TARGETS)
	install $(TARGETS) $(DESTDIR)

LIBOBJS=$(LIBSRCS:.cc=.o)


# external libraries. they get added into LDLIBS in common.make
LIBDIRS = $(LEEDS_LIB) ../common
LDLIBFILES = -lcard -lsfdl-common -lcommon
vpath %.so $(LIBDIRS)


# the commands and all for run-circuit and enc-circuit are identical!
CCT_FLAGS=-I$(4758_KERNEL_INCLUDE) -I$(SCC_INCLUDE)
CCT_DEPS=$(LIBOBJS) $(LDLIBFILES)

run-circuit.o : CPPFLAGS += $(CCT_FLAGS)
runtime: run-circuit.o $(CCT_DEPS)
	$(CXXLINK)

enc-circuit.o : CPPFLAGS += $(CCT_FLAGS)
enc-circuit: enc-circuit.o $(CCT_DEPS)
	$(CXXLINK)

permute : CPPFLAGS += -D_TESTING_BATCHER_PERMUTE
permute: batcher-permute.o $(CCT_DEPS)
	$(CXXLINK)

array : CPPFLAGS += -DTESTING_ARRAY
array: array.o $(CCT_DEPS)
	$(CXXLINK)

# give the dependency generation step all the include dirs
depend : CPPFLAGS += $(CCT_FLAGS)

include ../footer.make
