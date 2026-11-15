app := mc

.PHONY: all clean bench bin test test-invalid test-listing

CXX := clang
CXXFLAGS := -m64 -Ofast -D IS_LINUX

srcfiles := $(shell find . -name "*.c")
incfiles := $(shell find . -name "*.h")

all: $(app)

$(app): $(srcfiles) $(incfiles)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(app) $(srcfiles)
	ls -l $(app)

clean:
	rm -f $(app)

bench: $(app)
	./$(app) bench.tc

test: $(app)
	./$(app) unit-tests.tc

test-invalid: $(app)
	@set -e; for test in invalid-tests/*.tc; do \
		if ./$(app) "$$test" >/dev/null 2>&1; then \
			echo "expected $$test to fail"; exit 1; \
		fi; \
		echo "$$test: rejected (as expected)"; \
	done

test-listing: $(app)
	-./$(app) -l listing-expected.tc
	diff listing-expected.txt listing.txt

bin: $(app)
	cp -u -p $(app) ~/bin/$(app)
