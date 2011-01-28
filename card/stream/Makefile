include ../../header.make

LIBSRCS = processor.cc

LIB=libcard-stream

LIBFILE=$(LIB).$(LIBEXT)

all: libs


$(LIB).so: $(LIBOBJS)
	$(CXXLINK)

$(LIB).a: $(LIBOBJS)
	ar -ru $@ $^


# external libraries. they get added into LDLIBS in common.make
# need most of these only for the test code.
LIBDIRS		+= $(DIST_LIB) ../common
LDLIBFILES	+= -lcard -lsfdl-common -lpircommon -lfaerieplay-common
vpath %.so $(LIBDIRS)
vpath %.a $(LIBDIRS)

install: libs
	$(INSTALL) $(LIBFILE) $(DIST_LIB)

TESTSRCS=$(wildcard test-*.cc)

tests: $(TESTEXES)

libs: $(LIBFILE)


include ../../footer.make
