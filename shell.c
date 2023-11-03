#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

#include "built_in.h"
#include "jobs.h"

int new_input();
char** parse_input(char* inputs);
void free_str_arr();
void exec_by_path(char* path, char** arguments);
void jobsBuiltIn(int* job_count);
char* arrToStr(char** arr);
void bg_job(job* job);
void fg_job(job* job);
void kill_job(job* job);
job* getJob(int jobid);
job* getJobByPID(int jobPID);
void addJob(job* job);
void removeJob(job* job);
void signal_handler(int signum) { 
    printf("\nRecieved signal %d. Exit with \"> exit\"\n", signum);
}

//made job_arr a global variable and it's associated
//information variables to make access and management easier


job** job_arr;
int maxIndex = 0;
size_t job_arr_length = 0;
sig_atomic_t fg_pid = -1;
//static volatile sig_atomic_t signalPid = -1;
//sighandler handles sigint
void sigHandler(int sig, siginfo_t *info, void *context){
    if(fg_pid != -1){
        job* job = getJobByPID(fg_pid);
        removeJob(job);
        kill(fg_pid, SIGINT);
        int s;
        waitpid(fg_pid, &s, 0);
        fg_pid = -1;
    } else {
      printf("\n");
    }
}
void sig_tstp_handler(int sig, siginfo_t *info, void *context){
    //printf("\tReceived SIGTSTP\n");
    if(fg_pid != -1){
       job* t = getJobByPID(fg_pid);
       if(t != NULL) t->status = STOPPED;
      // printf("\tStopping process %d\n", fg_pid);
       kill(fg_pid, SIGTSTP);
       fg_pid = -1;
    } else {
        printf("\n");
    }

}
void sigchld_handler(int sig, siginfo_t *info, void *context){
    int signalPID= info->si_pid;
    int siCode = info->si_code;
    
    //if the child has exited through normal means
    if(siCode == CLD_EXITED){
        //printf("\tPROCESS %d EXITED\n", signalPID);    
        job* t = getJobByPID(signalPID);
        if(signalPID == fg_pid) fg_pid = -1;
        if(t != NULL){
       //     printf("\tRemoving job %d from array\n", t->jobID);  
            //if(signalPID != fg_pid) printf("\n[%d] %d done\n", t->jobID, t->PID);
            removeJob(t); 
        }
        
    } 
    //if child has abnormally terminated

    else if (siCode == CLD_KILLED || siCode == CLD_DUMPED){
        if(signalPID == fg_pid) fg_pid = -1;
        //printf("\tPROCESS %d ABNORMALLY TERMINATED\n", signalPID);    
        job* t = getJobByPID(signalPID);
        if(t != NULL){
            printf("[%d] %d terminated by signal 2\n", t->jobID, signalPID);
            //printf("\tRemoving job %d from array\n", t->jobID);  
            removeJob(t); 
        }

    }

    
}

int main() {
   // signal(SIGINT, signal_handler);
   // signal(SIGTSTP, signal_handler);
 

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = sigHandler;
    sigaction(SIGINT, &sa, NULL);

    struct sigaction ss;
    ss.sa_flags = SA_SIGINFO;
    sigemptyset(&ss.sa_mask);
    ss.sa_sigaction = sig_tstp_handler;
    sigaction(SIGTSTP, &ss, NULL);

    struct sigaction sc;
    sc.sa_flags = SA_SIGINFO;
    sigemptyset(&sc.sa_mask);
    sc.sa_sigaction = sigchld_handler;
    sigaction(SIGCHLD, &sc, NULL);
    //pause();

    job_arr = malloc(sizeof(*job_arr));
    job_arr_length = 1;
    int job_count = 1;
    job_arr[0] = NULL;
    

    
    while(1) {
        new_input(&job_count);
      /* 
       // commented out the previous method of handling the job_arr in case
       // we want to roll back / to compare if issues arise
        new_input(job_arr, &job_count);

        if(job_count-1 >= job_arr_length) {
            job_arr = realloc(job_arr, 2*job_arr_length*sizeof(job*));
            job_arr_length = job_arr_length * 2;
            for(int i = job_count; i < job_arr_length; i++) job_arr[i] = NULL;

            printf("\treallocating job_arr to %ld bytes\n", job_arr_length*sizeof(job_arr));
        }
    */
    }

}
/*
    when this function is called it prints ">" and waits for input
    when it gets input, that is passed to parse_input() to be parsed
*/
int new_input(int* job_count) {
    printf("> ");

    char *buffer = NULL;
    int read;
    long unsigned int len;

    int error_in_getline = 0;
    error_in_getline++;
    error_in_getline--;

    read = getline(&buffer, &len, stdin);
    if (read == -1){
        // printf("err: %s\n", strerror(errno));
        if(errno==0) {
            printf("\tctrl-d\n");
            for(int i = 0; i <= maxIndex; i++){
                if(job_arr[i] != NULL){
                    kill(job_arr[i]->PID, SIGHUP);
                    kill(job_arr[i]->PID, SIGCONT);
                }
            }  
            for(int i = 0; i <= maxIndex; i++){
                if(job_arr[i] != NULL) {
                    kill(job_arr[i]->PID, SIGHUP);
                    free(job_arr[i]->call);
                    free(job_arr[i]);
                }
            }
            free(job_arr);
            free(buffer);
            exit(0);

        }
        error_in_getline = errno;
        errno = 0;
        free(buffer);
        clearerr(stdin);
        //printf("\n");
        return new_input(job_count);
    }
    char** str_arr = parse_input(buffer);
    
    // if theres no input, try again
    if (read==1) {
        free(buffer);
        return new_input(job_count);
    }
    
    // str_arr is an array of all the arguments
    

    if(str_arr == NULL) {
    //    printf("\tstr_arr is null\n");
        free(buffer);
        return new_input(job_count);
    }

    if (read == -1) { // if str_arr is not null and read == -1, this should mean that ctrl-d was pressed
    //    printf("\tctrl-d pressed\n");
    }
    


    if(!strcmp(str_arr[0], "exit") || strchr(buffer, 4)) {
        signal(SIGCHLD, SIG_DFL);
        // todo: send signals to subprocesses, then exit
        fg_pid = -1;
      for(int i = 0; i <= maxIndex; i++){
            if(job_arr[i] != NULL){
                kill(job_arr[i]->PID, SIGHUP);
                kill(job_arr[i]->PID, SIGCONT);
            }
      }  
      for(int i = 0; i <= maxIndex; i++){
          if(job_arr[i] != NULL) {
            kill(job_arr[i]->PID, SIGHUP);
            free(job_arr[i]->call);
            free(job_arr[i]);
          }
        }
        free(job_arr);
        free(buffer);
        free_str_arr(str_arr);
        exit(0);
    }

    // ----- do stuff with arguments in str_arr here -----
    pid_t pid = 0;
    pid = pid + 0;
    // todo: check if the last argument or the last character is & to indicate running in bg

    // if its an absolute or relative path, it should be executed
    if (!strncmp(str_arr[0], "/", 1) || !strncmp(str_arr[0], "./", 2) || !strncmp(str_arr[0], "../", 3)) {
        if (!access(str_arr[0], F_OK)) {
            exec_by_path(str_arr[0], str_arr);
        } else {
            printf("%s: No such file or directory\n", str_arr[0]);
        }
        
    }
    // check for built-in functions
    else if (!strcmp(str_arr[0], "test"))
        test();
    else if (!strcmp(str_arr[0], "bg")){
        int jobid = atoi(str_arr[1]+1);
        if(str_arr[1][0] == '%'){
        job* tempJob = getJob(jobid);
        if(tempJob == NULL){
          printf("\tinvalid jobID found\n");
        }
        else{
          bg_job(tempJob);
        }
        }
    }
    else if (!strcmp(str_arr[0], "fg")){
      int jobid = atoi(str_arr[1]+1);
      job* tempJob = getJob(jobid);
      if(tempJob == NULL){
     //   printf("\tinvalid jobID found\n");
      }
      else{
        fg_job(tempJob);
      }
    }
    else if(!strcmp(str_arr[0], "kill")){
        
        int jobid = atoi(str_arr[1]+1);
        job* tempJob = getJob(jobid);
        if(tempJob == NULL){
       //     printf("\tinvalid jobID found\n");
        }
        else{
            kill_job(tempJob);
        }
    }
    else if(!strcmp(str_arr[0], "jobs")){
        jobsBuiltIn(job_count);
    }
    else if (!strcmp(str_arr[0], "cd")) {
        
        int arr_len = -1;
        while (str_arr[++arr_len] != NULL) {}

        char path[512];
        if (arr_len == 1) {
            printf("\t%s\n", getcwd(path, 512));
        } 
        else {
            int chdir_return = chdir(str_arr[1]);
            printf("\t%s\n", getcwd(path, 512));

            if(chdir_return != 0) {
                printf("%s: %s\n", str_arr[1], strerror(errno));
            }   
        }
    }
    // last check for default paths
    else {
        // String sequence for /usr/bin/[cmd]
        char * usrbin = (char*) malloc((strlen("/usr/bin/") + strlen(str_arr[0]) + 1) * sizeof(char));
        strcpy(usrbin, "/usr/bin/");
        strcat(usrbin, str_arr[0]);

        // String sequence for /bin/[cmd]
        char * bin = (char*) malloc((strlen("/bin/") + strlen(str_arr[0]) + 1) * sizeof(char));
        strcpy(bin, "/bin/");
        strcat(bin, str_arr[0]);
        // Check access to default paths
        if (!access(usrbin, F_OK)) {
            exec_by_path(usrbin, str_arr);
        } else if (!access(bin, F_OK)) {
            exec_by_path(bin, str_arr);
        } else {
            printf("%s: command not found\n", str_arr[0]);
        }
        free(usrbin);
        free(bin);
    }
    
    // add job to job arr
    /*
    job* new_job = malloc(sizeof(job));
    new_job->jobID = *job_count;
    new_job->PID = job_pid;
    //char* call = malloc(20*sizeof(char));
    char* call = arrToStr(str_arr);
    new_job->call = call;
    new_job->status = 0;
    new_job->position = 0;
    
    addJob(new_job);
    */
    //job_arr[*job_count-1] = new_job;uu
   // jobsBuiltIn(job_arr, job_count);
   // printf("\tadded job to job array at index %d\n", *job_count-1);
    //print_job(new_job);

    *job_count+=1;

    free_str_arr(str_arr);
    free(buffer);
    return 0;
    }


/*
    can treat this one like a black box nothing important happens in here

    parse_input recieves a string (obtained in new_input()) and returns a char** that
    contains every substring split by a space. each substring should be null terminated
    ignores multiple spaces.
    the last element in the return value is NULL
*/
char** parse_input(char* inputs) {
    // printf("\tlen: %d\n", strlen(inputs));
    // printf("\tinputs: %s", inputs);
    
    int elements = 0;
    if (inputs[0] !=32) elements = 1;
    char prev_char = inputs[0];
    for (int i=1; i<strlen(inputs); i++) {
        if (prev_char == 32 && prev_char != inputs[i])
            elements++;

        prev_char = inputs[i];
    }

    // printf("\telements: %d\n", elements);

    char** results = malloc(sizeof(char*)*(elements+1));
    results[elements] = NULL;
    int res_ndx = 0;
    int word_start = 0;
    int word_end = 0;

    for (int i=0; i<strlen(inputs); i++) {
        if (inputs[i] == 10) { // reached newline
            // make word_end i
            // take substring between word_start and word_end
            // return results
            word_end = i;
            char* word = malloc(sizeof(char)*(word_end-word_start)+1);
            memcpy(word, &inputs[word_start], word_end-word_start);
            word[word_end-word_start] = '\0';
            results[res_ndx] = word;
            res_ndx+=1;

            // return results;
        } else if (inputs[i] == 32) { // reached space
            // if the previous character was not a space, take a substring from word_start to i
            // if the next character is not a space, make word_start i+1. i++
            if (i!=0 && inputs[i-1] != 32) {
                char* word = malloc(sizeof(char)*(i-word_start)+1);
                memcpy(word, &inputs[word_start], i-word_start);
                word[i-word_start] = '\0';
                results[res_ndx] = word;
                res_ndx+=1;
            }
            if (inputs[i+1] != 32) {
                word_start = i+1;
                i++;
            }
        }
    }

    for (int i=0; i<res_ndx; i++) {
    //    printf("\targument[%d]: %s\n", i, results[i]);
    }

    if (res_ndx == 0){
        free(results);
        return NULL;
    }

    results[res_ndx] = NULL;

    return results;
}

/*
 *
    frees a char** and all the char*'s from the start of
    char** to where it reaches NULL

    b
    call this at the end of new_input()
*/
void free_str_arr(char** to_free) {
    int arr_len = -1;
    while (to_free[++arr_len] != NULL) { }

    for (int i=0; i<arr_len; i++) {
        // printf("\tfreeing index %d of to_free\n", i);
        free(to_free[i]);
    }

    // printf("\tfreeing to_free\n");
    free(to_free);
}


/*
    forks, replaces the forked process with a new process specified by its path 
*/



void exec_by_path(char* path, char** arguments) {
    //printf("\trunning exec_by_path with arguments:\n");
    int i=0;
    while(arguments[i] != NULL) {
     //   printf("\t\t%s\n", arguments[i]);
        i++;
    }
    int status;
    job* new_job = (job*)malloc(sizeof(job));
    addJob(new_job);
   // new_job = job_arr[maxIndex];
    new_job->jobID = maxIndex;
    
    new_job->status = 0;
    //0 for fg 1 for bg
    int bg = 0;
    // first check if the last character of the last argument is &
    char* ampersand_pos = strstr(arguments[i-1], "&");
    if(ampersand_pos != NULL) {
        if (ampersand_pos != arguments[i-1])
            ampersand_pos[0] = '\0'; // dumb looking way to check if & appears in its own argument and remove it if so
        bg = 1; 
        new_job->position = 1;
    }

    // then check if the last argument is just &
    
    if(!strcmp(arguments[i-1], "&")){
        bg = 1;
        new_job->position = 1;
        free(arguments[i-1]);
        arguments[i-1] = NULL;
    }
    
    char* call = arrToStr(arguments);
    new_job->call = call;
    
    new_job->PID = fork();
    if (new_job->PID == 0) { // child
        // when implementing running in background, make sure to use dup2() to change stdout to null or smthn idk
        setpgid(0,0);
        char* newenviron[] = { NULL };
        int ret = execve(path, arguments, newenviron);
        
        if (ret == -1) {
        //    printf("\tthere was a problem running %s\n", path);
            char err = errno;
            // i think if execve had real environment variables it wouldnt print a random ascii character at the start
            // either should pass real environment variables to execve or just manually put the name of the program if thers an error
            free(arguments);
            perror(&err);
            exit(1);
        } else {
            exit(0);
        }
    }

    if(new_job != NULL){
        setpgid(new_job->PID, new_job->PID);
    // when implementing running in background, make sure to NOT wait if the program was run in the background
        if(bg == 1){
            printf("[%d] %d\n", new_job->jobID, new_job->PID);
        }
        if(bg == 0){
            fg_pid = new_job->PID;
        //    printf("\tWaiting for process [%d]\n", new_job->PID);
            if(new_job!=NULL){
                waitpid(new_job->PID, &status, 0);
            }
        }
     //   printf("\tchild completed\n");
    }
    //print_job(new_job);
}

/*
    Executes the builtIn command Jobs
    if there are no jobs in the array, then the program will print nothing, otherwise
    it will print jobs for each line in the format of...
    job ID, processID, current status, command (including  foreground/background)
    
    [jobID] processID status command foreground/background
    [1] 1432 Running ./test1 &

*/



void jobsBuiltIn(int* job_count){
   //if there are no jobs print nothing.
    
    job* j = NULL;
    for(int i = 0; i < maxIndex+1; i++){
        //temporary variable j -> job
        j = job_arr[i];
        //TODO: print the ampersand on call not based on call, but based on whether or not 
        //the job is currently a background or a foreground job
        //if the job is null we pass it, otherwise keep printing? this should perhaps handle
        //no jobs running as well as any jobs killed / finished before later ones
       // printf("\tjobsBuiltIn loop reached\n");
        if(job_arr[i] != NULL){
        // printf("\tjobsBUiltIn finds non-null job\n");
            if(j->status == RUNNING){ 
                printf("[%d] ", j->jobID);
                printf("%d ", j->PID);
                printf("RUNNING ");
                if(j->position == FG){
                    printf("%s\n", j->call);
                }
                else printf("%s&\n", j->call);
            }
            else{
                printf("[%d] ", j->jobID);
                printf("%d ", j->PID);
                printf("STOPPED ");
                printf("%s\n", j->call);
            }
        }   
       // else printf("\tjobsBuiltIn finds null job\n");
    } 
}

/*
    
    bg_job takes a job struct pointer, of a paused job
    unpauses it in the background and sets it's position to
    bg as well.

*/

void bg_job(job* job){
    if(job->PID == fg_pid) fg_pid = -1;
    job->status = RUNNING;
    job->position = BG;
    kill(job->PID, SIGCONT);
}


/*  
    bg_job takes a job struct pointer, of a paused job or a background job and brings it to run in 
    the foreground.


*/
void fg_job(job* job){
    int status;
    job->position = FG;
    fg_pid = job->PID;
    if(job->status == STOPPED){
      job->status = RUNNING;
      kill(job->PID, SIGCONT);
    }
    waitpid(job->PID, &status, 0);
    
}

/*
    
   Kill takes a a job struct pointer, and kills the job as well as 
   frees its job struct information

*/

void kill_job(job* job){
    if(job->PID == fg_pid) fg_pid = -1;
    printf("[%d] %d terminated by signal 15\n", job->jobID, job->PID);
    kill(job->PID, SIGTERM);
    removeJob(job);
}


/*
 arrToStr takes a String array and makes one large string comprised of all
 the srings concatenated together - this is used for forming the call
 part of jobs properly
*/

char* arrToStr(char** arr){
    
    char* out = (char*)malloc(sizeof(char));
    int outsize = 1;
    out[0] = '\0';
    int arrlen = -1;
    while(arr[++arrlen]!=NULL) { }

    int wordlen = 0;
    int i;
    for(i = 0; i < arrlen; i++){

        wordlen = -1;
        while(arr[i][++wordlen]!='\0') { }
        outsize = outsize +  wordlen + 3;
        out = (char*)realloc(out, outsize);
        strcat(out, arr[i]);
        strcat(out, " ");
    }
    
    out[outsize-2] = '\0';
    return out;

}


/*
 
   getJob takes a int jobid and job** arr as a parameter and looks through 
   the list to return either NULL if it can not find the
   job or a pointer to th job..

*/

job* getJob(int jobid){  
    for(int i = 0; i <= maxIndex; i++){
        if(job_arr[i] != NULL && job_arr[i]->jobID == jobid) return job_arr[i];
    }
    return NULL;
}

job* getJobByPID(int jobPID){
    for(int i = 0; i <= maxIndex; i++){
      if(job_arr[i] != NULL){
         if(job_arr[i]->PID == jobPID) return job_arr[i];
      }
    }
 //   printf("\tNo such job with pid %d found\n", jobPID);
    return NULL;

}
/*
 *
   remove job takes job* job and removes it from job_arr and then sets the
   index to null, and updates maxIndex accordingly

   
*/
void removeJob(job* job){
    int i;
    for(i = 0; i <= maxIndex; i++){
      if(job_arr[i] != NULL){
        if(job_arr[i]->jobID == job->jobID){
     //       printf("\tJob %d successfully removed from array\n", job->PID);
            free(job->call);
            free(job);
            job = NULL;
            job_arr[i] = NULL;
            break;
        }
      }
    }
    if( i >= maxIndex){
      //decrement maxIndex until either 0 or the next highest index in the arraty
        while(maxIndex > 0 && job_arr[maxIndex] == NULL) maxIndex--;
    }


}

/*
    
   adds job after thhe current highest index job 

*/

void addJob(job* job){
     
    if( maxIndex >=  job_arr_length -1){
        job_arr = realloc(job_arr,  sizeof(struct job *) * 2 * (int)job_arr_length );
        job_arr_length = job_arr_length * 2;
        for(int j = maxIndex+1; j < job_arr_length; j++) job_arr[j] = NULL;
        //printf("\treallocating job_arr to %ld bytes\n", job_arr_length*sizeof(job_arr));
    }
    
    maxIndex++;
   // printf("\tJob %d added to Array\n", maxIndex);  
    job_arr[maxIndex] = job;
    job_arr[maxIndex]->jobID = maxIndex;

}






