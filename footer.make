# -*-makefile-*-

depend : CPPFLAGS += -I$(TOP)/$(TREE)/include
depend: $(SRCS)
	$(CXX) -M $(CPPFLAGS) $^ > $@

include depend

clean:
	rm -f $(TARGETS) $(OBJS)
