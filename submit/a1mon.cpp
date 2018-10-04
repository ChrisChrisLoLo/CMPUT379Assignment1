#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sstream>
#include <vector>
#include <map>
#include <signal.h>
#include <wait.h>

#define DEF_INTERVAL 10
#define MAX_CHARS 2000
#define MAX_DISP_CHARS 95

#define PID 1
#define PPID 2
#define CMD 5

using namespace std;

struct proc_t{
    string cmd;
    int id;
    int parentId;
};

//Parses the read processes and adds any children of the process to a watchlist.
//After adding the process to the watchlist, the function recurses, now checking
//if there are any children belonging to the child process. Requires a vector to store
//results, a vector of all processes, and the target process id. Returns nothing but
//alters the watchlist.
void findChildren(vector<proc_t>& watchlist,vector<proc_t> processList, int targetPid){
    for(int i=0;i<processList.size();i++){
        if (processList[i].parentId == targetPid){
            watchlist.push_back(processList[i]);
            //search for children of the child process.
            findChildren(watchlist,processList,processList[i].id);
        }
    }
}

int main(int argc, char* argv[]){
    if (argc < 2){
        cout<<"Error: No target pid specified"<<endl;
        exit(1);
    }
    else if (argc > 3){
        cout<<"Error: too many parameters specified"<<endl;
    }


    string targetPidIn = "";
    string intervalIn = "";

    if (argv[1]) targetPidIn = (string)argv[1];
    if (argv[2]) intervalIn = (string)argv[2];
    bool running = true;
    int tpid;
    int interval = DEF_INTERVAL;
    int counter = 0;


    if(targetPidIn.empty()) {
        cout<<"Error: Target pid not defined"<<endl;
        exit(-1);
    }
    else if(targetPidIn.find_last_not_of("0123456789")!=string::npos){
        cout<<"Error: Target is not a number"<<endl;
        exit(-1);
    }
    if (intervalIn.find_last_not_of("0123456789")!=string::npos){
        cout<<"Error: interval is not a number"<<endl;
        exit(-1);
    }
    else if (!intervalIn.empty()){
        interval = stoi(intervalIn);
    }

    tpid = stoi(targetPidIn);
    vector<proc_t> watchlist;

    printf("a1mon [counter= %i, pid= %i, target_pid= %i, interval= %i sec]: \n", counter, getpid(), tpid, interval);
    while(running) {
        FILE *pf;
        pf = popen("ps -u $USER -o user,pid,ppid,state,start,cmd --sort start", "r");
        if (pf == nullptr) {
            cout << "Error: Pipe could not open" << endl;
        }
        vector<proc_t> processVector;

        //Open the pipe, read each line from the pipe, and convert into a string
        char line[MAX_CHARS];
        //Print the header line
        cout<<fgets(line,MAX_CHARS,pf)<<endl;

        bool targetIsAlive = false;

        while(fgets(line,MAX_CHARS,pf)){
            string attr;
            string lineString(line);
            lineString.resize(MAX_DISP_CHARS);
            cout<<lineString;
            if(lineString.back() != '\n' ||lineString.back() != '\0'){
                cout<<endl;
            }
            //convert string to string stream, then look at each word in the stream.
            //if the word matches the PID,PPID, or CMD column of ps, then save each
            //word, and put them into a process struct.

            istringstream iLineString(lineString, istringstream::in);
            string pid,ppid,cmd;
            for(int column=0;iLineString >> attr;column++){
                if(column == PID){
                    pid = attr;
                }
                else if(column == PPID){
                    ppid = attr;
                }
                else if(column == CMD) {
                    cmd = attr;
                }
            }
            int pidInt = stoi(pid);
            int ppidInt = stoi(ppid);
            if (pidInt == tpid){
                targetIsAlive = true;
            }
            proc_t process = {cmd,pidInt,ppidInt};
            //Add process struct to list
            processVector.push_back(process);
        }

        if(!targetIsAlive){
            cout<<"a1mon: target appears to have terminated or never existed to begin with; cleaning up"<<endl;
            int status;
            if(watchlist.size()) {
                for (int i = 0; i < watchlist.size(); i++) {
                    cout << "Terminating [" << watchlist[i].id << ", " << watchlist[i].cmd << "]" << endl;
                    kill(watchlist[i].id, SIGTERM);
                    waitpid(watchlist[i].id, &status, 0);
                }
            }
            cout<<"exiting a1mon"<<endl;
            exit(0);
        }
        watchlist.clear();
        findChildren(watchlist,processVector,tpid);

        cout<<"-------------------------------------------------"<<endl;
        cout<<"List of monitored processes:"<<endl;
        cout<<"[";
        for(int i=0;i<watchlist.size();i++){
            cout<<i<<":["<<watchlist[i].id<<","<<watchlist[i].cmd<<"]";
            if(i != watchlist.size()-1){
                cout<<", ";
            }
        }
        cout<<"]"<<endl;
        pclose(pf);
        flush(cout);
        sleep(interval);
    }
}
