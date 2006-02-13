include ../../header.make

include ../../../pir/common.make

# external libraries. they get added into LDLIBS in common.make
LIBDIRS		+= $(LEEDS_LIB) ../common
LDLIBFILES	+= -lcard -lsfdl-common -lcommon
vpath %.so $(LIBDIRS)
vpath %.a $(LIBDIRS)

TESTSRCS=$(wildcard test-*.cc)

tests: $(TESTEXES)

all: tests

