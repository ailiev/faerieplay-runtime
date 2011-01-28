# -*- Makefile -*-

# this reading of MAKEFILE_LIST has to be done before any other files are
# include-d!
this_file := $(lastword $(MAKEFILE_LIST))
this_dir := $(dir $(realpath $(this_file)))


FAERIEPLAY_DIST_ROOT ?= $(HOME)/faerieplay

DIST_ROOT ?= $(FAERIEPLAY_DIST_ROOT)

ifeq ($(DIST_ROOT),)
$(error Please set the environment variable FAERIEPLAY_DIST_ROOT)
endif


EXTRA_INCLUDE_DIRS = $(DIST_ROOT)/include

# produce a trace of gate values. comment out to disable.
LOGVALS := 1
