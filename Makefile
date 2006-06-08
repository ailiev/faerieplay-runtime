SUBDIRS = common card # host

subdirs: $(SUBDIRS)

card host: common

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

all: subdirs
clean: subdirs
install: subdirs
dep: subdirs

doc: doxygen.project
	doxygen doxygen.project

.PHONY: subdirs $(SUBDIRS)

