# -*-makefile-*-

ifndef make_incl_dir
    $(error Need value for variable make_incl_dir, should be set in header.make)
endif

include $(make_incl_dir)/footer.make
