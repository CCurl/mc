app := mc

CXX := clang
CXXFLAGS := -m64 -O3 -D IS_LINUX

srcfiles := $(shell find . -name "*.c")
incfiles := $(shell find . -name "*.h")
LDLIBS   := -lm

all: $(app)

$(app): $(srcfiles) $(incfiles)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(app) $(srcfiles) $(LDLIBS)
	ls -l $(app)

clean:
	rm -f $(app)

test1: $(app)
	cat test1.mc | ./$(app)

test2: $(app)
	cat test2.mc | ./$(app)

bin: $(app)
	cp -u -p $(app) ~/.local/bin/
