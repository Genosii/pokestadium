# Builds the scripted-input plugin for mupen64plus. See README.md.
CC ?= gcc

mupen64plus-input-script.so: input_script.c
	$(CC) -shared -fPIC -O2 -Wall -o $@ $< -ldl

clean:
	rm -f mupen64plus-input-script.so

.PHONY: clean
