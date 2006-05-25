include ../header.make

include $(PIR)/utils.make
include $(PIR)/header.make


PIRCARD=$(PIR)/card
PIRHOST=$(PIR)/host

SRCS=

DESTDIR=$(LEEDS_BIN)

# no space problem on the host usually
#CFLAGS += -g

LIBDIRS	    += $(LEEDS_LIB) .
LDLIBFILES  += -lsfdl-common -lcommon -lboost_filesystem -lsfdl-card
vpath %.so $(LIBDIRS)
vpath %.a $(LIBDIRS)

TARGETS=  #prep-circuit

all: build

build: $(TARGETS)

install: build
#	install $(TARGETS) $(DESTDIR)

$(TARGETS): $(OBJS) $(PIRHOST)/objio.o $(LDLIBFILES)
	$(CXXLINK)


include ../footer.make

