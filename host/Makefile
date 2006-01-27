include ../header.make
include $(PIR)/common.make


PIRCARD=$(PIR)/card
PIRHOST=$(PIR)/host

SRCS=prep-circuit.cc

DESTDIR=$(LEEDS_BIN)

# no space problem on the host usually
#CFLAGS += -g

LIBDIRS = $(PIR)/common ../common
LDLIBFILES=-lsfdl-common -lcommon -lboost_filesystem
vpath %.so $(LIBDIRS)


TARGETS=prep-circuit

all: build

build: $(TARGETS)

install: build
	install $(TARGETS) $(DESTDIR)

$(TARGETS): $(OBJS) $(PIRHOST)/objio.o $(LDLIBFILES)
	$(CXXLINK)


include ../footer.make
