include ../pir/common.make

# this makefile needs the ENV variables TOP and TREE pointing into the 4758
# linux distribution


PIRCARD=../pir/card


CPPFLAGS = -I../pir -I. -I..
CPPFLAGS += -I$(4758_KERNEL_INCLUDE) -I$(SCC_INCLUDE)

CPPFLAGS += -DNDEBUG

CFLAGS += #-O -march=i486

DESTDIR=~/leeds_root/bin/

SRCS=run-circuit.cc enc-circuit.cc

LIBOBJS=

LDFLAGS+=-static

LDLIBS += -lsfdl-common -L../common \
	  -lcommon      -L../pir/common

VPATH=../pir/common
LDLIBFILES=-lsfdl-common -lcommon


# the headers for the 4758 crypto interface are in the IBM patched kernel source
4758_KERNEL_INCLUDE = $(TOP)/$(TREE)/card/linux-2.2.17/drivers
# SCC communication headers
SCC_INCLUDE = $(TOP)/$(TREE)/include


EXES=runtime enc-circuit

all: build

build: $(EXES)

install: build
	strip $(EXES)
	install $(EXES) $(DESTDIR)

# the commands and all for run-circuit and enc-circuit are identical!
CCT_FLAGS=-I$(4758_KERNEL_INCLUDE) -I$(SCC_INCLUDE)
CCT_DEPS=$(LIBOBJS) $(PIRCARD)/hostcall.o $(PIRCARD)/4758_sym_crypto.o $(PIRCARD)/lib.o $(PIRCARD)/consts.o $(LDLIBFILES)

run-circuit.o : CPPFLAGS += $(CCT_FLAGS)
runtime: run-circuit.o $(CCT_DEPS)
	$(CXXLINK)

enc-circuit.o : CPPFLAGS += $(CCT_FLAGS)
enc-circuit: enc-circuit.o $(CCT_DEPS)
	$(CXXLINK)


# give the dependency generation step all the include dirs
depend : CPPFLAGS += -I$(4758_KERNEL_INCLUDE) -I$(SCC_INCLUDE)

include ../footer.make
