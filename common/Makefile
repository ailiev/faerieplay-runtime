include ../pir/common.make



SRCS=gate.cc

_CCOBJS=$(SRCS:.cc=.o)
OBJS=$(_CCOBJS:.c=.o)


CPPFLAGS += -I../pir

LDFLAGS+=-static

LDLIBS += -lcommon -L../pir/common


LIBFILE=libsfdl-common.a


all: $(LIBFILE)

$(LIBFILE): $(OBJS)
	ar -ru $@ $^

include ../footer.make
