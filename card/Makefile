include ../pir/common.make

# this makefile needs the ENV variables TOP and TREE pointing into the 4758
# linux distribution


PIRCARD=../pir/card


CPPFLAGS = -I../pir -I. -I..
CPPFLAGS += -I$(4758_KERNEL_INCLUDE) -I$(SCC_INCLUDE)

CPPFLAGS += -DNDEBUG

CFLAGS += -O -march=i486

DESTDIR=~/leeds_root/bin/

SRCS=run-circuit.cc

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


all: build

build: runtime

install: build
	strip runtime
	install runtime $(DESTDIR)

run-circuit.o : CPPFLAGS += -I$(4758_KERNEL_INCLUDE) -I$(SCC_INCLUDE)
runtime: run-circuit.o $(LIBOBJS) $(PIRCARD)/hostcall.o $(PIRCARD)/4758_sym_crypto.o $(LDLIBFILES)
	$(CXXLINK)

# give the dependency generation step all the include dirs
depend : CPPFLAGS += -I$(4758_KERNEL_INCLUDE) -I$(SCC_INCLUDE)

include ../footer.make
