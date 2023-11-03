// const char* builtins[3][1] = {{"test1\n"},{"test2\n"},{"test3\n"}};

int bg(int jobID);
int cd(char* path);
void exit(int code);
int fg(int jobID);
int jobs();
void test();

void test() {
    printf("\there is a test function!\n");
    // printf("%s", builtins[0][0]);
    printf("\t%s\n", getenv("PWD"));
}

int bg(int jobID) {
    printf("\tbg called\n");
    return 0;
}

int fg(int jobID) {
    printf("\tfg called\n");
    return 0;
}

/*
not sure if its actually viable to have all these functions in a header file because they need to know
about the state of the other jobs and idk how they could get that information easily
*/