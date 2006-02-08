# !!
# this makefile needs the env variables TOP and TREE pointing into the 4758
# linux distribution

include ../header.make

include $(PIR)/common.make


CPPFLAGS += -I$(4758_KERNEL_INCLUDE) -I$(SCC_INCLUDE)

# for use by stream/processor.h
# CPPFLAGS += -I../../..

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

ifdef HAVE_OPENSSL
LDLIBS	    += -lssl -lcrypto
CPPFLAGS    += -DHAVE_OPENSSL
endif


TARGETS=array permute runtime enc-circuit

all: build

build: $(TARGETS)

install: build
#	strip $(TARGETS)
	$(INSTALL) $(TARGETS) $(DESTDIR)

LIBOBJS=$(LIBSRCS:.cc=.o)


# external libraries. they get added into LDLIBS in common.make
LIBDIRS		+= $(LEEDS_LIB) ../common
LDLIBFILES	+= -lcard -lsfdl-common -lcommon


vpath %.so $(LIBDIRS)
vpath %.a $(LIBDIRS)

# the commands and all for run-circuit and enc-circuit are identical!
CCT_FLAGS=-I$(4758_KERNEL_INCLUDE) -I$(SCC_INCLUDE)
CCT_DEPS=$(LIBOBJS) $(LDLIBFILES)

run-circuit.o : CPPFLAGS += $(CCT_FLAGS)
runtime: run-circuit.o $(CCT_DEPS)
	$(CXXLINK)

enc-circuit.o : CPPFLAGS += $(CCT_FLAGS)
enc-circuit: enc-circuit.o $(LDLIBFILES) #$(CCT_DEPS)
	$(CXXLINK)

batcher-permute-main.o : CPPFLAGS += -D_TESTING_BATCHER_PERMUTE
batcher-permute-main.o: batcher-permute.cc
	$(CXXCOMP)
permute: $(patsubst batcher-permute.o,batcher-permute-main.o,$(CCT_DEPS))
	$(CXXLINK)

# generate a different object file with a main in it
array-main.o : CPPFLAGS += -DTESTING_ARRAY
array-main.o: array.cc
	$(CXXCOMP)
array: $(patsubst array.o,array-main.o,$(CCT_DEPS))
	$(CXXLINK)

# give the dependency generation step all the include dirs
depend : CPPFLAGS += $(CCT_FLAGS)

include ../footer.make
