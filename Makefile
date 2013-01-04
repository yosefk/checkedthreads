export PYTHONDONTWRITEBYTECODE=1
default: build tests
build:
	@./build.py
tests:
	@./test.py
clean:
	rm -rf bin lib obj
help:
	@echo make "# build and test"
	@echo make VERBOSE=1 "# print stuff"
	@echo make VERBOSE=2 "# print more stuff"
	@echo make build "# without testing"
	@echo make tests "# without building"
	@echo make clean "# remove bin/, lib/, and obj/"

