CC=gcc
SRCS=dxr.c dxr_png.c
FLAGS = -Wall -Wextra -Werror

all: dxr2png

dxr2png: $(SRCS)
	$(CC) $(FLAGS) -o $(@) $(SRCS) -lpng

clean:
	rm -fv dxr2png *.c~

test:
	./run-tests.sh
