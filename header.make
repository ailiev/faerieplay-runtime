# -*- makefile -*-

# to be included by Makefiles in card/ etc
# keep in mind that the dirs defined here will be used from one firectory
# deeper.


# this reading of MAKEFILE_LIST has to be done before any other files are
# include-d!
this_file := $(lastword $(MAKEFILE_LIST))
this_dir := $(dir $(realpath $(this_file)))

include $(this_dir)/config.make

# the code/ directory
CPPFLAGS += -I$(SHARED_DIR) -I$(SHARED_DIR)/..
