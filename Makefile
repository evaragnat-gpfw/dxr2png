CC=gcc
SRCS=dxr.c dxr_png.c

all: dxr2png

dxr2png: $(SRCS)
	$(CC) -W -Wall -Wextra -o $(@) $(SRCS) -lpng

clean:
	rm -fv dxr2png *.c~

test:
	./run-tests.sh
