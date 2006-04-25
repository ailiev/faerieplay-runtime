include ../header.make

include $(PIR)/utils.make
include $(PIR)/header.make

LIBS=libsfdl-common.$(LIBEXT)

TARGETS=$(LIBS)

LIBSRCS=misc.cc gate.cc
SRCS=$(LIBSRCS)

all: build

build: $(LIBS)

libsfdl-common.so: $(LIBOBJS)
	$(CXXLINK)

libsfdl-common.a: $(LIBOBJS)
	ar -ru $@ $^


install: build
	$(INSTALL) $(LIBS) $(LEEDS_LIB)


include ../footer.make
