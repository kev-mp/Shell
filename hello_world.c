#include <stdio.h>

int main(int argc, char** argv) {
    printf("hello world!\n");
    printf("argc: %d\n", argc);

    printf("argv:");
    for(int i=0; i<argc; i++)
        printf("%s\n", argv[i]);
}

// test program to call from shell.exe