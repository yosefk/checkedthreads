default:
	@./build.py
clean:
	rm -rf bin lib obj
help:
	@echo make
	@echo make VERBOSE=1
	@echo make VERBOSE=2
	@echo make clean
