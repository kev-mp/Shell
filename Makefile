OUTPUT = shell
CFLAGS = -g -Wall -Wvla -Werror -fsanitize=address
LFLAGS = -lm

%: %.c
	gcc $(CFLAGS) -o $@ $< $(LFLAGS)

all: $(OUTPUT)

clean:
	rm -f *.o $(OUTPUT)
