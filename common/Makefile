include ../header.make

include $(PIR)/common.make

LIBS=libsfdl-common.$(LIBEXT)

TARGETS=$(LIBS)

SRCS=misc.cc gate.cc

all: build

build: $(LIBS)

$(LIBS): $(OBJS)
	ar -ru $@ $^

install: build
	install $(LIBS) $(LEEDS_LIB)


include ../footer.make
