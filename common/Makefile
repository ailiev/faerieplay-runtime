include ../header.make


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
	$(INSTALL) $(LIBS) $(DIST_LIB)


include ../footer.make
