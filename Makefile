# makefile (C)2016 Tim Ruehsen

ifndef VERBOSE
	SILENT=@
endif

CC=$(SILENT)gcc -Wall -Wextra
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
	$(LN) $^ -o $@ -licuuc -lunistring -lidn2
