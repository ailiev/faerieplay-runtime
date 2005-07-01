include ../pir/common.make

PIRCARD=../pir/card
PIRHOST=../pir/host

SRCS=prep-circuit.cc


DESTDIR=~/leeds_root/

CPPFLAGS += -I../pir

CFLAGS += -g

LDLIBS += -lsfdl-common -L../common \
	  -lcommon      -L../pir/common

VPATH=../pir/common
LDLIBFILES=-lsfdl-common -lcommon 

TARGET=prep-circuit

all: build

build: $(TARGET)

install: build
	install $(TARGET) $(DESTDIR)

$(TARGET): $(OBJS) $(PIRHOST)/objio.o $(LDLIBFILES)
	$(CXXLINK)


include ../footer.make
