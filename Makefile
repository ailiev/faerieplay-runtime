SUBDIRS = common card host

action ?= all

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(action)

clean : action=clean
clean: subdirs

depend : action=depend
depend: subdirs

card host: common

.PHONY: subdirs $(SUBDIRS)

