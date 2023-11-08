TARGETS  = arboretum
CFLAGS   = -Iinclude/ -Wno-format -Wno-sign-compare -Wno-c++11-compat-deprecated-writable-strings
default: $(TARGETS)

%.o: %.cc $(wildcard include/*.h)
	g++ $(CFLAGS) -Iinclude/ $< -g -c -o $@

%.o: %.cc
	g++ $(CFLAGS) -Iinclude/ $< -g -c -o $@

scanner.cc: src/scanner.ll parser.hh
	flex -o $@ $<

parser.cc parser.hh: src/parser.yy
	bison -y $< --defines=parser.hh -o parser.cc

arboretum: scanner.o parser.o $(patsubst %.cc,%.o,$(wildcard src/*.cc))
	g++ $^ -o $@

clean::
	rm -f $(TARGETS) src/*.o scanner.c parser.c parser.hh *.o
