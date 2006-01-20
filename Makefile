SUBDIRS = common card host

subdirs: $(SUBDIRS)

card host: common

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

all: subdirs
clean: subdirs
install: subdirs

.PHONY: subdirs $(SUBDIRS)

