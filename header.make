# -*- makefile -*-

# to be included by Makefiles in card/ etc
# keep in mind that the dirs defined here will be used from one firectory
# deeper.


PIR=../../pir
PIRCARD=$(PIR)/card
PIRHOST=$(PIR)/host

# the code/ directory
CPPFLAGS += -I../../ -I$(PIR)
