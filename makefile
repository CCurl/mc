app := mc

.PHONY: all clean bin test test-invalid test-listing test-exe bench-vm

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

bench-vm: $(app)
	./$(app) bench.mc

test: $(app)
	./$(app) unit-tests.mc

test-invalid: $(app)
	@set -e; for test in invalid-tests/*.mc; do \
		if ./$(app) "$$test" >/dev/null 2>&1; then \
			echo "expected $$test to fail"; exit 1; \
		fi; \
		echo "$$test: rejected (as expected)"; \
	done

test-listing: $(app)
	-./$(app) -l listing-expected.mc
	diff listing-expected.txt listing.txt

bench: $(app) bench.mc
	./$(app) -n -e bench bench.mc

bench-exe: $(app) bench
	./bench

test-exe: $(app)
	./$(app) -n -e unit-tests.exe unit-tests.mc
	./$(app) unit-tests.mc > unit-tests.vm.txt
	./unit-tests.exe > unit-tests.exe.txt
	diff unit-tests.vm.txt unit-tests.exe.txt
	rm -f unit-tests.exe unit-tests.vm.txt unit-tests.exe.txt

bin: $(app)
	cp -u -p $(app) ~/bin/$(app)
