include header.make

SUBDIRS = common card

subdirs: $(SUBDIRS)

card: common

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

all: subdirs
clean: subdirs

install: subdirs

dep: subdirs

doc: doxygen.project
	doxygen doxygen.project

.PHONY: subdirs $(SUBDIRS)
