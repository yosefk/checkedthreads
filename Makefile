include defaults.mk

export PYTHONDONTWRITEBYTECODE=1

.PHONY: build valgrind tests clean help
default: build valgrind tests
build:
	@./build.py
valgrind:
	@./valgrind/build.py
tests:
	@./test.py
clean:
	rm -rf bin lib obj
help:
	@echo make "          # build the libraries and the valgrind tool and test"
	@echo make VERBOSE=1 "# print stuff"
	@echo make VERBOSE=2 "# print more stuff"
	@echo make build "    # build the libraries"
	@echo make valgrind " # build the valgrind tool"
	@echo make tests "    # test the libraries and the valgrind tool"
	@echo make clean "    # remove bin/, lib/, and obj/"
my:
	@echo -n "go "
day:
	@echo ahead.

