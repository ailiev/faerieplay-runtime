include ../pir/common.make



CPPFLAGS += -I../pir

CFLAGS += -O -march=i486

LDFLAGS+=-static

LDLIBS += -lcommon -L../pir/common


LIBFILE=libsfdl-common.a

SRCS=misc.cc gate.cc
all: build

build: $(LIBFILE)

$(LIBFILE): $(OBJS)
	ar -ru $@ $^

install: build

include ../footer.make
