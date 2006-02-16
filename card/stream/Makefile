include ../../header.make

include $(PIR)/common.make


LIBSRCS = processor.cc

LIB=libcard-stream
LIBFILE=$(LIB).$(LIBEXT)

$(LIB).so: $(LIBOBJS)
	$(CXXLINK)

$(LIB).a: $(LIBOBJS)
	ar -ru $@ $^


# external libraries. they get added into LDLIBS in common.make
# need most of these only for the test code
LIBDIRS		+= $(LEEDS_LIB) ../common
LDLIBFILES	+= -lcard -lsfdl-common -lcommon
vpath %.so $(LIBDIRS)
vpath %.a $(LIBDIRS)

all: libs tests

install: libs
	$(INSTALL) $(LIBFILE) $(LEEDS_LIB)

TESTSRCS=$(wildcard test-*.cc)

tests: $(TESTEXES)

libs: $(LIBFILE)


include ../../footer.make
