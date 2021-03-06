# makefile (C)2016 Tim Ruehsen

ifndef VERBOSE
	SILENT=@
endif

CC=$(SILENT)gcc -g -Wall -Wextra
LN=$(SILENT)gcc 

TARGETS=\
	tr46
OBJECTS=\
	tr46.o

all:
	@$(MAKE) --no-print-directory objects targets

verbose:
	@VERBOSE=1 $(MAKE) --no-print-directory objects targets

objects: $(OBJECTS)
	@echo -n

targets: $(TARGETS)
	@echo -n

clean:
	@echo Removing objects and binaries...
	-@rm -f $(OBJECTS) $(TARGETS)

.SECONDARY:

# default rule to create .o files from .c files
%.o: %.c
	@echo Compiling $(@F)
	$(CC) -c $< -o $@

# default rule to link executables
%: %.o
	@echo Linking $(@F) ...
#	$(LN) $^ -o $@ -licuuc -lunistring -lidn2
	$(LN) $^ -o $@ -licuuc -lunistring ~/src/libidn2/.libs/libidn2.a
