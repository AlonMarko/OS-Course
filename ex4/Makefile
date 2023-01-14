CC=g++
CXX=g++
RANLIB=ranlib

LIBSRC=VirtualMemory.cpp
LIBOBJ=$(LIBSRC:.cpp=.o)

INCS=-I.
CFLAGS = -Wall -std=c++11 $(INCS)
CXXFLAGS = -Wall -std=c++11 $(INCS)

VMLIB = libVirtualMemory.a
TARGETS = $(VMLIB)

TAR=tar
TARFLAGS=-cvf
TARNAME=ex4.tar
TARSRCS=$(LIBSRC) Makefile README

all: $(TARGETS)

$(TARGETS): $(LIBOBJ)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

clean:
	$(RM) libMapReduceFramework.a libMapReduceFramework.a  MapReduceFramework.o Barrier.o *~ *core

depend:
	makedepend -- $(CFLAGS) -- $(SRC) $(LIBSRC)

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)
