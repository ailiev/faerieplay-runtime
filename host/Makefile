include ../header.make

SRCS=

DESTDIR=$(DIST_BIN)

# no space problem on the host usually
#CFLAGS += -g

LIBDIRS	    += $(LEEDS_LIB) .
LDLIBFILES  += -lsfdl-common -lpircommon -lfaerieplay-common -lboost_filesystem
vpath %.so $(LIBDIRS)
vpath %.a $(LIBDIRS)

TARGETS=prep-circuit

all: build

build: $(TARGETS)

install: build
#	install $(TARGETS) $(DESTDIR)

$(TARGETS): $(OBJS) $(PIRHOST)/objio.o $(LDLIBFILES)
	$(CXXLINK)


include ../footer.make

