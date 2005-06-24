include ../pir/common.make

PIRCARD=../pir/card

CPPFLAGS += -I../pir

DESTDIR=~/leeds_root/bin/

SRCS=run-circuit.cc

LIBOBJS=

LDFLAGS+=-static

LDLIBS += -lsfdl-common -L../common \
	  -lcommon      -L../pir/common

all: install

build: runtime

install: build
	install runtime $(DESTDIR)

runtime: run-circuit.o $(LIBOBJS) $(PIRCARD)/hostcall.o
	$(CXXLINK)

include ../footer.make
