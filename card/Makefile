# !!
# this makefile needs the env variables TOP and TREE pointing into the 4758
# linux distribution

include ../header.make

include $(SHARED_DIR)/utils.make
include $(SHARED_DIR)/header.make



SUBDIRS:=stream


ifdef LOGVALS
	CPPFLAGS += -DLOGVALS
endif


ifdef HAVE_4758_CRYPTO
# done in header.make
#	CPPFLAGS += -I$(4758_KERNEL_INCLUDE)
endif
ifdef COMM_SCC
	CPPFLAGS += -I$(SCC_INCLUDE)
endif


# CPPFLAGS += -DNDEBUG

ifdef RUNONCARD
	CFLAGS += -march=i486
endif

LIBSRCS=array.cc utils.cc batcher-permute.cc batcher-network.cc \
	run-circuit.cc enc-circuit.cc prep-circuit.cc
SRCS=cvm.cc $(LIBSRCS)

TESTSRCS=$(wildcard test-*.cc)


# LDFLAGS+=-static


# the headers for the 4758 crypto interface are in the IBM patched kernel source
#4758_KERNEL_INCLUDE = $(TOP)/$(TREE)/card/linux-2.2.17/drivers

# SCC communication headers
SCC_INCLUDE = $(TOP_4758)/$(TREE_4758)/include

#ifdef HAVE_OPENSSL
#LDLIBS	    += -lssl -lcrypto
#CPPFLAGS    += -DHAVE_OPENSSL
#endif

LIB = sfdl-card

LIBFILE = lib$(LIB).$(LIBEXT)
EXES = cvm

TARGETS=$(LIBFILE) $(EXES)

all: subdirs build


lib$(LIB).so: $(LIBOBJS)
	$(CXXLINK)
lib$(LIB).a: $(LIBOBJS)
	ar -ru $@ $^


-l$(LIB): $(LIBFILE)


build: $(TARGETS)

install: subdirs build
#	strip $(EXES)
	$(INSTALL) $(LIBFILE)	$(DIST_LIB)
	$(INSTALL) $(EXES)	$(DIST_BIN)


# external libraries. they get added into LDLIBS in common.make
LIBDIRS		+= $(DIST_LIB) . ../common
LDLIBFILES	+= -lcard -lsfdl-common -lcommon -lcard-stream -ljson

vpath %.so $(LIBDIRS)
vpath %.a $(LIBDIRS)

$(EXES) : LDLIBFILES := -lsfdl-card $(LDLIBFILES)
$(EXES): -lsfdl-card


# the main driver file
cvm: cvm.o $(LDLIBFILES)
	$(CXXLINK)


$(TESTEXES): $(LIBOBJS)

tests: $(TESTEXES)



subdirs: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: subdirs $(SUBDIRS)


# give the dependency generation step all the include dirs
dep : CPPFLAGS += $(CCT_FLAGS)

# recursive dep call.
PREDEPCMD = $(MAKE) -C stream dep

include ../footer.make
