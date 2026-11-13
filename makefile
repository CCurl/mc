app := tc

.PHONY: all clean unit-tests invalid-tests

CXX := clang
CXXFLAGS := -m32 -O3 -D IS_LINUX

srcfiles := $(shell find . -name "*.c")
incfiles := $(shell find . -name "*.h")
LDLIBS   := -lm

all: $(app)

$(app): $(srcfiles) $(incfiles)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(app) $(srcfiles) $(LDLIBS)
	ls -l $(app)

clean:
	rm -f $(app)

unit-tests: $(app)
	./$(app) unit-tests.tc

invalid-tests: $(app)
	@set -e; for test in invalid-tests/*.tc; do \
		if ./$(app) "$$test" >/dev/null 2>&1; then \
			echo "expected $$test to fail"; exit 1; \
		fi; \
		echo "$$test: rejected (as expected)"; \
	done

bin: $(app)
	cp -u -p $(app) ~/bin/
