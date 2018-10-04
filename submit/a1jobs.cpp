#include <sys/time.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <vector>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <ctime>
#include <chrono>
#include <unistd.h>

#define COLOR_END "\x1B[0m"
#define COLOR_YELLOW "\x1B[33m"

#define RLIM_CUR 10 * 60
#define RLIM_MAX 10 * 60

#define MAX_JOBS 32
#define MAX_ARG_NUM 4
using namespace std;

const unsigned int MICROSECOND_SLEEP = 5000;

struct job_t {
    string cmd;
    int pid;
};
job_t jobs[MAX_JOBS];

//Sends a kill signal to a given "job". Will check if job id is valid, and attempts
//to check if the job exists. Returns 1 for an error and 0 for success. Verbose is true
//if we want to print out errors via cout. Verbose is used for user input.
int sendSignal(string inputJob, int signal, bool verbose=true){
    //Check if string is numeric
    int status;
    if (!inputJob.empty() && inputJob.find_last_not_of("0123456789")==string::npos){
        int jid = stoi(inputJob);
        //Prevent segfault errors by preventing out of index reference
        if (jid>MAX_JOBS){
            if (verbose) cout<<"Error: Job number exceeds maximum jobs allowed"<<endl;
            return 1;
        }
        //Referenced job in job array is null.
        else if (jobs[jid].pid == 0 && jobs[jid].cmd == ""){
            if (verbose) cout<<"Error: Job does not exist"<<endl;
            return 1;
        }
        //Send null signal to ensure process exists
        if (kill(jobs[jid].pid,0) != -1){
            if (kill(jobs[jid].pid, signal) == -1){
                cout<<strerror(errno)<<endl;
            }
            //Collect child process with waitpid() if child is terminated.
            if(signal == SIGTERM) {
                waitpid(jobs[jid].pid, &status, WUNTRACED);
                //if the child process is stopped, don't block the parent process
                //in trying to collect a non-defunct process
                if(WIFSTOPPED(status)){
                    if (verbose) cout<<"Process is currently stopped. Resuming process to kill it..."<<endl;
                    kill(jobs[jid].pid,SIGCONT);
                    kill(jobs[jid].pid,SIGTERM);
                    waitpid(jobs[jid].pid, &status, 0);
                }
            }
        }
        else{
            if (verbose) cout<<"Error: Process no longer exists"<<endl;
            return 1;
        }
    }
    else{
        if (verbose) cout<<"Error: Input was not a number"<<endl;
        return 1;
    }
    return 0;
}

//Calculate and print the difference between to clock times.
//Takes a label to print the time as.
void printClockDiff(clock_t startClock,clock_t endClock,string label){
    double timeDiff = double(abs(endClock-startClock))/CLOCKS_PER_SEC;
    printf("%s:%6.0f seconds.\n",label.c_str(),timeDiff);
}


int main(int argc, char* argv[]){
    int jobCount = 0;
    //Set the maximum run time of the program to 10 minutes
    struct rlimit rlim;
    if (getrlimit(RLIMIT_CPU, &rlim) == -1){
        cout<<"Error:Rlimit set failed"<<endl;
        cout<<strerror(errno)<<endl;
    }
    rlim.rlim_cur = RLIM_CUR;
    rlim.rlim_max = RLIM_MAX;
    if (setrlimit(RLIMIT_CPU, &rlim) == -1){
        cout<<"Error:Rlimit set failed"<<endl;
        cout<<strerror(errno)<<endl;
    }
    if (getrlimit(RLIMIT_CPU, &rlim) == -1){
        cout<<"Error:Rlimit set failed"<<endl;
        cout<<strerror(errno)<<endl;
    }

    //Get initial times
    struct tms initTimes{}, finalTimes{};
    auto startClock = std::chrono::high_resolution_clock::now();
    times(&initTimes);

    int pid, status;
    while (true){
        string exe;
        string input,command;

        //display prompt
        cout<<COLOR_YELLOW<<"a1jobs["<<getpid()<<"]: "<<COLOR_END;

        //Get command input and parse into an array.
        getline(cin,input);

        stringstream ssInput(input);
        ssInput >> command;
        ssInput >> exe;
        string inArg[MAX_ARG_NUM] = {};

        for (int i=0;ssInput.good()&&i<MAX_ARG_NUM;i++){
            ssInput >> inArg[i];
        }
        //Parse input arguments and add them to the execution arguments if they are valid.
        //add null pointer at the end.
        char * exeArgs[MAX_ARG_NUM+2] = {(char*)exe.c_str()};
        int nullTermPos = 1;
        for(int i=0;i<MAX_ARG_NUM;i++){
            if (inArg[i] != ""){
                exeArgs[i+1] =(char*)inArg[i].c_str();
                nullTermPos += 1;
            }
        }
        exeArgs[nullTermPos]= nullptr;

        if (command=="run"){
            if (jobCount >= MAX_JOBS){
                cout<<"Error: Maximum jobs reached"<<endl;
            }
            else if (exeArgs[0]=="\0"){
                cout<<"Error: No executable given"<<endl;
            }
            else{
                pid = fork();
                if (pid==0){
                    //Child Process
                    if(execvp(exeArgs[0],exeArgs)==-1){
                        cout<<"Error in running "<<exeArgs[0]<<": "<<endl;
                        perror(exeArgs[0]);
                        exit(1);
                    }
                    else{
                        exit(0);
                    }
                }
                else{
                    //Parent Process
                    jobs[jobCount].cmd = exe;
                    jobs[jobCount].pid = pid;

                    //short pause to let child exit(1) if it needs to
                    usleep(MICROSECOND_SLEEP);
                    jobCount++;

                    if (waitpid(pid,&status,WNOHANG)!=0){
                        if(WIFEXITED(status)){
                            if (WEXITSTATUS(status) == 1){
                                //if process exited with an exit status of 1, an error has occurred.
                                //since process was not valid, do not count it as a job.
                                jobCount--;
                            }
                        }
                    }
                }
            }
        }
        else if (command == "list"){
            for(int i=0;i<jobCount;i++){
                //If the process hasn't been terminated, print it.
                int killStatus = kill(jobs[i].pid,0);
                if (killStatus != -1){
                    printf("%i: (pid= %8i, cmd= %s)\n",i,jobs[i].pid,jobs[i].cmd.c_str());
                }
            }
        }
        else if (command == "suspend"){
            sendSignal(exe,SIGSTOP);
        }
        else if (command == "resume"){
            sendSignal(exe,SIGCONT);
        }
        else if (command == "terminate"){
            sendSignal(exe,SIGTERM);
        }
        else if (command == "exit"){
            for(int i=0;i<jobCount;i++){
                //send a kill signal to all jobs
                if(sendSignal(to_string(i),SIGTERM,false)==0){
                    cout<<"killed job pid "<<jobs[i].pid<<endl;
                }
            }
            break;
        }
        else if (command == "quit"){
            break;
        }
        else if (command.empty()){
            //Do nothing
        }
        else if (command == "help"){
            cout<<"run [bash cmd]: Create a child process and execute a bash command"<<endl;
            cout<<"list: List all non-terminated jobs"<<endl;
            cout<<"suspend [jobNo]: Send a suspend signal to a given job"<<endl;
            cout<<"resume [jobNo]: Send a resume signal to a given suspended job"<<endl;
            cout<<"terminate [jobNo]: Send a terminate signal to a given job"<<endl;
            cout<<"quit: Quit program without terminating child processes"<<endl;
            cout<<"exit: Exit program and terminating child processes"<<endl;
        }
        else{
            cout<<command+": command not found."<<endl;
            cout<<"Type \"help\" for a list of commands."<<endl;
        }
        cout.flush();
    }

    //Get final times, and calculate the total times.
    auto endClock = std::chrono::high_resolution_clock::now();

    auto milliTime = std::chrono::duration_cast<std::chrono::milliseconds>(endClock-startClock).count();
    printf("%s:%6.2f seconds.\n","real",(float)milliTime/1000);
    printClockDiff(initTimes.tms_utime,finalTimes.tms_utime,"user");
    printClockDiff(initTimes.tms_stime,finalTimes.tms_stime,"sys");
    printClockDiff(initTimes.tms_cutime,finalTimes.tms_cutime,"child user");
    printClockDiff(initTimes.tms_cstime,finalTimes.tms_cstime,"child sys");

    return 0;
}
