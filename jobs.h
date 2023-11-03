enum JobStatus { RUNNING, STOPPED };
enum Position { FG, BG };

struct job
{
    int jobID;
    int PID;
    char* call;
    enum JobStatus status;
    enum Position position;
};

typedef struct job job;

void print_job(job* j) {
    printf("[%d]", j->jobID);
    printf(" %d", j->PID);
    // printf("\tstatus: ");
    if(j->status == 2)
        printf(" Terminated");
    else if(j->status == 5247)
        printf(" Stopped");
    else if(WIFEXITED(j->status))
        printf(" Exited");
    else
        printf(" running, or something else with status: %d\n", j->status);

    printf(" %s", j->call);
    printf(" %s", j->position ? "":"&");
    printf("\n");
}