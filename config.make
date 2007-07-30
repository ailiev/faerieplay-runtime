# -*- Makefile -*-

# this reading of MAKEFILE_LIST has to be done before any other files are
# include-d!
this_file := $(lastword $(MAKEFILE_LIST))
this_dir := $(dir $(realpath $(this_file)))

# $(info this_dir = $(this_dir))

SHARED_DIR = $(realpath $(this_dir)/../pir)

JSON_DIR = $(realpath $(this_dir)/../json)


# produce a trace of gate values. comment out to disable.
LOGVALS := 1


# $(info SHARED_DIR = $(SHARED_DIR))


# this sets DIST_ROOT.
include $(SHARED_DIR)/config.make

# Could also set DIST_ROOT here.
# DIST_ROOT = /usr/local/faerieplay
