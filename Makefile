SUBDIRS = common card host

action ?= all

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(action)

clean : action=clean
clean: subdirs

all : action=all
all: subdirs

install : action=install
install: subdirs

card host: common

.PHONY: subdirs $(SUBDIRS)

