# Search the build directory for precompiled headers
INC = -Ibuild -Isource/include -Igoogletest/googletest/include \
    -Irapidjson/include $(shell kcutilmgr conf -i)
LIB = -Lbuild -Lgoogletest/googletest $(shell kcutilmgr conf -l) -luuid \
    -lcurl -lmicrohttpd
UNITTESTLIBS = -lpthread -lgtest_main -lgtest -luuid
SOLIBS = $(shell kcutilmgr conf -l) -lpthread

CXXSOURCE = $(wildcard source/*.cc)
CSOURCE = $(wildcard source/*.c)
HEADERS = $(wildcard source/include/*.h source/include/*.hh)
SOURCE  = $(CSOURCE) $(CXXSOURCE)

COBJ = $(CSOURCE:source/%.c=build/%.o)
CXXOBJ = $(CXXSOURCE:source/%.cc=build/%.o)
OBJ = $(COBJ) $(CXXOBJ)

UNITTEST_CXXSOURCE = $(wildcard unittest/*.cc)
UNITTEST_CXXOBJ = $(UNITTEST_CXXSOURCE:unittest/%.cc=build/unittest/%.o)
UNITTEST_TARGETS = $(UNITTEST_CXXOBJ:%.o=%)
UNITTEST_LOGS = $(UNITTEST_TARGETS:%=%.xml)
PRECOMPILED_HEADERS = $(HEADERS:source/include/%.hh=build/%.hh.gch)
TESTDBS = $(UNITTEST_TARGETS:build/unittest/%=build/unittest/%.kct)

LIBTARGETS = build/libinvdb.so

CXXDEPEND = $(CXXSOURCE:source/%.cc=build/%.cc.d) \
    $(UNITTEST_CXXSOURCE:unittest/%.cc=build/unittest/%.cc.d)
HEADERDEPEND = $(HEADERS:source/include/%.hh=build/%.hh.d)
DEPEND = $(CXXDEPEND) $(HEADERDEPEND)

TARGETS = $(LIBTARGETS) $(UNITTEST_TARGETS)

UNITTEST_LD_LIBRARY_PATH = build

.PRECIOUS: $(DEPEND) $(OBJ) $(UNITTEST_CXXOBJ) $(UNITTEST_LOGS) \
    $(PRECOMPILED_HEADERS)
.SECONDARY: $(DEPEND) $(OBJ) $(UNITTEST_CXXOBJ) $(UNITTEST_LOGS) \
    $(PRECOMPILED_HEADERS)

CC  := gcc
CXX := g++
LD  := g++
DBMGR := kctreemgr
CXXFLAGS := -std=c++14 -fPIC -g
SOFLAGS  := -shared

.PHONY: all depend clean mrproper googletest submodules test \
            clean_testresults directories rapidjson docs deps

all: directories depend $(TARGETS)

deps:
	apt install libboost-dev
	apt install cmake
	apt install gdb
	apt install libkyotocabinet-dev kyotocabinet-utils
	apt install doxygen graphviz

depend: $(DEPEND)

docs:
	mkdir -p build/doxygen
	cd source && doxygen ../doxygen.conf

clean: clean_testresults
	rm -f build/*.o
	rm -f build/unittest/*.o
	rm -f $(TARGETS)
	rm -f $(UNITTEST_LOGS)
	rm -f build/unittest/*.db
	rm -f $(PRECOMPILED_HEADERS)

clean_testresults:
	rm -f $(UNITTEST_LOGS)

mrproper: clean
	rm -rf build/doxygen
	rm -f build/*.d
	rm -f build/unittest/*.d
	+cd googletest && make clean
	+cd googletest/googletest && make clean

rapidjson:
	git submodule update --init rapidjson
	+cd rapidjson && cmake . && make

googletest:
	git submodule update --init googletest
	+cd googletest && cmake . && make
	+cd googletest/googletest && cmake . && make

submodules: googletest rapidjson

directories:
	mkdir -p build/unittest
	mkdir -p googletest

%.kct:
	$(DBMGR) create $@

test: $(TESTDBS) $(UNITTEST_LOGS)

build/unittest/%.xml: build/unittest/% build/unittest/%.kct
	$(DBMGR) clear $(word 2, $^)
	LD_LIBRARY_PATH=$(UNITTEST_LD_LIBRARY_PATH) gdb -x unittest/gdbscript \
                                           --args $^  --gtest_output=xml:$@

build/%.cc.d: source/%.cc
	$(CXX) -MM $< -o $@ $(INC) $(CXXFLAGS)

build/%.o: source/%.cc build/%.cc.d $(PRECOMPILED_HEADERS)
	$(CXX) -c $< -o $@ $(INC) $(CXXFLAGS)

build/%.so: $(OBJ)
	$(LD) $(SOFLAGS) $(OBJ) $(SOLIBS) -o $@

build/unittest/%.cc.d: unittest/%.cc
	$(CXX) -MM $< -o $@ $(INC) $(CXXFLAGS)

build/unittest/%.o: unittest/%.cc build/unittest/*.cc.d $(PRECOMPILED_HEADERS)
	$(CXX) $(INC) -c $(CXXFLAGS) $< -o $@

build/unittest/%: build/unittest/%.o $(LIBTARGETS)
	$(LD) $(LIB) $< $(LIBTARGETS:build/lib%.so=-l%) \
        $(UNITTESTLIBS) -o $@

build/%.hh.d: source/include/%.hh
	$(CXX) -fpch-deps $(INC) -MM $(CXXFLAGS) $< -o $@

build/%.hh.gch: source/include/%.hh build/%.hh.d
	$(CXX) $(INC) -c $(CXXFLAGS) $< -o $@

include $(wildcard build/*.d)
include $(wildcard build/unittest/*.d)

