app := tc

.PHONY: all clean unit-tests invalid-tests listing-test

CXX := clang
CXXFLAGS := -m64 -Ofast -D IS_LINUX
ifdef LISTING
CXXFLAGS += -D LISTING
endif

srcfiles := $(shell find . -name "*.c")
incfiles := $(shell find . -name "*.h")
LDLIBS   := -lm

all: $(app)

$(app): $(srcfiles) $(incfiles)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(app) $(srcfiles) $(LDLIBS)
	ls -l $(app)

clean:
	rm -f $(app) tc-listing

bench: $(app)
	./$(app) bench.tc

unit-tests: $(app)
	./$(app) unit-tests.tc

invalid-tests: $(app)
	@set -e; for test in invalid-tests/*.tc; do \
		if ./$(app) "$$test" >/dev/null 2>&1; then \
			echo "expected $$test to fail"; exit 1; \
		fi; \
		echo "$$test: rejected (as expected)"; \
	done

tc-listing: $(srcfiles) $(incfiles)
	$(CXX) $(CXXFLAGS) -D LISTING -o tc-listing $(srcfiles) $(LDLIBS)

listing-test: tc-listing
	$(CXX) $(CXXFLAGS) -D LISTING -o tc-listing $(srcfiles) $(LDLIBS)
	rm -f listing.txt
	-./tc-listing listing-expected.tc
	diff listing-expected.txt listing.txt
	# rm -f ./tc-listing

bin: $(app)
	cp -u -p $(app) ~/bin/
