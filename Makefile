CC=gcc
SRCS=dxr.c dxr_png.c

FLAGS = -Wall -Wextra -Werror
OS_NAME := $(shell uname)
ifeq ($(OS_NAME),Darwin)
	FLAGS += -D __MAC__
endif


all: dxr2png

dxr2png: $(SRCS)
	$(CC) $(FLAGS) -o $(@) $(SRCS) -lpng

clean:
	rm -fv dxr2png *.c~

test:
	./run-tests.sh
