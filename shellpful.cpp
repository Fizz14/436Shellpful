#include <stdio.h>
#include<iostream>
#include <vector>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctime>
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>

bool showDevMessages = 1;

//quick debug info
#define D(a) if(showDevMessages) {std::cout << "                       " << #a << ": " << (a) << endl;}

int MAXPATHLEN = 1000;

using namespace std;

const char* homedir = getenv("HOME");

const std::string WHITESPACE = " \n\r\t\f\v";
 
string trim(string &input) {
    input.erase(input.begin(), std::find_if(input.begin(), input.end(), std::bind1st(std::not_equal_to<char>(), ' ')));
    return input;
}

string trim_after(const string &s) {
    int last = s.size() - 1;
    while (last >= 0 && s[last] == ' ')
        --last;
    return s.substr(0, last + 1);
}

vector<string> splitString (string s, char delimiter) {
    int start, end;
	start = 0;
    string token;
    vector<string> ret;
	end = s.find(delimiter, start);
    while (end != string::npos) {
        token = s.substr (start, end - start);
        start = end + 1;
        ret.push_back (token);
		end = s.find(delimiter, start);
    }

    ret.push_back (s.substr (start));
    return ret;
}

std::string getexepath() {
    char result[ 1500 ];
    ssize_t count = readlink( "/proc/self/exe", result, 1500 );
    return std::string( result, (count > 0) ? count : 0 );
}

bool thisIsRedirectOutput = 0;
bool thisIsRedirectInput = 0;

int sequential = 1; //run commands sequentially instead of parrallely

vector<int> bgprocesses;

string lastdir = homedir;
string overlastdir = homedir;

int main (){
    int savedSTIN = dup(0);
    int savedSTOUT = dup(1);
    //cout << "\033[0m\n";
    time_t now = time(0);
    char* dt = ctime(&now);
    string dtstr = dt;
    dtstr.erase(std::remove(dtstr.begin(), dtstr.end(), '\n'), dtstr.end());


    while (true){
        cout << "\033[1;31mShellpful" + (string)get_current_dir_name() + "$\033[0m";
        string inputline;
        dup2(savedSTIN, 0);
        dup2(savedSTOUT, 1);



        getline (cin, inputline);   // get a line from standard input

        if (inputline == string("exit")){
            cout << "End of shell" << endl;
            break;
        }

        sequential = inputline[inputline.size()-1] != '&';

        //if '|' is in quotes, replace it with something else
        bool inquotes = 0;
        for(int i = 0; i < inputline.size(); i++) {
            if(inputline[i] == '\'' || inputline[i] == '\"') {
                inquotes = !inquotes;
                continue;
            }
            if(inputline[i] == '|' && inquotes) {
                inputline[i] = '^';
            }
        }

        vector<string> c = splitString(inputline, '|');
        for(int i = 0; i < c.size(); i++) {
            trim(c[i]);
            c[i] = trim_after(c[i]);
            for(int j = 0; j < c[i].size(); j++) {
                if(c[i][j] == '^') {
                    c[i][j] = '|';
                }
            }
        }
        
        

        for(int ci = 0; ci < c.size(); ci++) {
            int fd[2];
            pipe(fd);
            int pid = fork();
            if (pid == 0){ //child process
                //redirect stout to pipeout unless ci + 1 == c.size() (last pipe command)
                
                // preparing the input command for execution
                vector<string> parts = splitString(c.at(ci), ' ');
                // for(auto x : parts) {
                //     D(x);
                // }
                
                //to add support for "", we need to scrub thru elements of parts
                for(int i = 0; i < parts.size(); i++) {
                    
                    if(parts[i][0] == '\"') {
                        //cout << "head of the snake is " << parts[i] << endl;
                        //if this element starts with "...
                        for(int j = i+1; j < parts.size(); j++) {
                            
                            if(parts[j][parts[j].size() - 1] == '\"') {
                                // cout << "tail of the snake is " << parts[j] << endl;
                                // cout << "j = " << j <<  endl;
                                // cout << "before tail is " << parts[j-1] << endl;
                                //parts[i->j] are really one parameter, lets combine them
                                for(int c = i+1; c <= j; c++) {
                                    parts[i] += " " + parts[c];
                                }
                                //remove i+1..j from the vector
                                for(int c = i+1; c <= j; c++) {
                                    parts.erase(parts.begin()+ i + 1);
                                }
                                
                                break;
                            }
                        }    
                    }
                    if(parts[i][0] == '\'') {
                        //cout << "head of the snake is " << parts[i] << endl;
                        //if this element starts with "...
                        for(int j = i+1; j < parts.size(); j++) {
                            
                            if(parts[j][parts[j].size() - 1] == '\'') {
                                // cout << "tail of the snake is " << parts[j] << endl;
                                // cout << "j = " << j <<  endl;
                                // cout << "before tail is " << parts[j-1] << endl;
                                //parts[i->j] are really one parameter, lets combine them
                                for(int c = i+1; c <= j; c++) {
                                    parts[i] += " " + parts[c];
                                }
                                //remove i+1..j from the vector
                                for(int c = i+1; c <= j; c++) {
                                    parts.erase(parts.begin()+ i + 1);
                                }
                                
                                break;
                            }
                        }    
                    }
                    
                }

                //strip ",' from begining and end of params
                for(auto &x : parts) {
                    
                    if(x[0] == '\"' && x[x.length()-1] == '\"') {
                        x.erase(x.length()-1);
                        x.erase(0, 1);
                    }
                    if(x[0] == '\'' && x[x.length()-1] == '\'') {
                        x.erase(x.length()-1);
                        x.erase(0, 1);
                    }
                }


                //output redirection
                thisIsRedirectOutput = 0;
                thisIsRedirectInput = 0;
                string channel = "unset";
                string inchannel = "unset";
                bool redirectedOutput = 0;
                bool redirectedInput = 0;
                for(auto &x : parts) {
                    if(thisIsRedirectOutput) {
                        channel = x;
                        thisIsRedirectOutput = 0;
                        redirectedOutput = 1;
                        //parts.erase(std::remove(parts.begin(), parts.end(), x), parts.end());
                    }
                    if(thisIsRedirectInput) {
                        inchannel = x;
                        thisIsRedirectInput = 0;
                        redirectedInput = 1;
                    }
                    if (x == ">") {
                        thisIsRedirectOutput = 1;
                        //parts.erase(std::remove(parts.begin(), parts.end(), x), parts.end());
                    }
                    if(x == "<") {
                        thisIsRedirectInput = 1;
                    }
                }
                if(redirectedOutput) {
                    string dir = getexepath();
                    //channel now is dir/start because thats the program we're running
                    dir = dir.substr(0, dir.length() - 5);
                    channel = dir + channel;
                    remove(channel.c_str());
                    int outputfd = open(channel.c_str(), O_CREAT | O_RDWR, 0666);
                    dup2(outputfd, 1);
                }

                if(redirectedInput) {
                    //string dir = getexepath();
                    //channel now is dir/start because thats the program we're running
                    //dir = dir.substr(0, dir.length() - 5);
                    //inchannel = dir + inchannel;
                    //remove(inchannel.c_str());
                    int inputfd = open(inchannel.c_str(), O_RDONLY, 0666);
                    dup2(inputfd, 0);
                    
                }

                //remove redirection from parts
                bool deleteNext = 0;
                for(int i = 0; i < parts.size(); i++) {
                    if(deleteNext) {
                        parts.erase(std::remove(parts.begin(), parts.end(), parts[i]), parts.end());
                        deleteNext = 0;
                        i--;
                    }

                    if(parts[i] == ">" || parts[i] == "<") {
                        deleteNext = 1;
                        parts.erase(std::remove(parts.begin(), parts.end(), parts[i]), parts.end());
                        i--;
                    }
                }   

                //check if a part has & to set sequential
                for(auto x : parts) {
                    if(x == "&") {
                        parts.erase(std::remove(parts.begin(), parts.end(), x), parts.end());
                        break;
                    }
                }

                char* args [parts.size() + 2] = {(char *) inputline.c_str()};
                for(int i = 0; i < parts.size(); i++) {
                    args[i] = (char *)parts.at(i).c_str();
                }
                args[parts.size() + 2] = NULL;
                
                
                
                string command = args[0];

                // Delete double-checking
                //cout << command << endl;
                if(command == "rm") {
                    cout << "Are you sure you want to delete? " << endl;
                    if(args[1][0] == '*'){
                        cout << "Careful! You are fixing to delete every file in the directory " << get_current_dir_name() << "." << endl;
                    }

                    //show files to be deleted

                    string response = "";
                    cin >> response;
                    if(response == "y" || response == "Y" || response == "yes" || response == "YES") {
                        cout << "Proceeding to delete" << endl;
                    } else {
                        cout << "Canceled deletion" << endl;
                        break;
                    }
                }

                // Map input to commands
                if(command == "Delete" | command == "delete" | command == "del" | command =="Del") {
                    cout << "Did you mean to use \"rm\"?" << endl;
                    break;
                }

                if(command == "Move" | command == "move") {
                    cout << "Did you mean to use \"mv\"?" << endl;
                    break;
                }

                if(command == "Copy" | command == "copy") {
                    cout << "Did you mean to use \"cp\"?" << endl;
                    break;
                }

                if(command == "Directory" | command == "directory" | command == "Dir" | command == "dir" | command == "Folder" | command == "folder") {
                    cout << "Did you mean to use \"cd\"?" << endl;
                    break;
                }

                if(command == "cd") {
                    char buffer[MAXPATHLEN];
                    char *path = getcwd(buffer, MAXPATHLEN);
                    overlastdir = lastdir;
                    lastdir = path;
                    string newPath = path;
                    if(parts.size() > 1) {
                        string first_arg = args[1];
                        if(first_arg == "..") {
                            
                            //chop this folder off of the working directory
                            vector <string> directories = splitString(path, '/');
                            // for(auto x : directories) {
                            //     D(x);
                            // }
                            int i = 0;
                            newPath = "";
                            while(i < directories.size() - 1) {
                                newPath += "/" + directories[i];
                                
                                i++;
                            }
                            //erase first character of path, which is a deplicated '/'
                            newPath.erase(0,1);
                            //D(newPath);
                        } else if(first_arg == "-") {
                            //go to last directory   
                            newPath = overlastdir;
                        } else if(first_arg == "/" || first_arg == "/home/") {
                            //set to root dir
                            newPath = homedir;
                        }
                        
                        else {
                            //there's a first argument, but it's not ".."
                            //do other built-in checks, but ultimately assume that it's a directory
                            
                            newPath += '/' + first_arg;
                        }
                    } else {
                        //cd command without any flags
                        // go to home directory
                        newPath = homedir;

                    }
                    
                    
                    chdir(newPath.c_str());
                } else {
                    
                    //D("about to run the command");
                    
                    if(c.size() > 1) {
                        if(ci < c.size() - 1) {
                            dup2(fd[1], 1);
                        } else {
                            //dup2(savedSTOUT, 1);
                        }
                    }
                    execvp (args [0], args);
                }
                
                
            }else{
                if(sequential) {
                    if(ci == c.size() - 1) {
                        waitpid (pid, 0, 0); // wait for the child process 
                    }
                } else {
                    //otheriwse, push the pid back on bgprocesses and check up on it
                    bgprocesses.push_back(pid);
                }
                
                dup2(fd[0],0);
                close(fd[1]);
                
                
                
                //check up on bgprocesses without causing a hang in the program
                
                for(int i = 0; i < bgprocesses.size();i++) {
                    int res = waitpid(bgprocesses.at(i), 0, WNOHANG);
                    if(res != 0) {
                        bgprocesses.erase(std::remove(bgprocesses.begin(), bgprocesses.end(), bgprocesses[i]), bgprocesses.end());
                        //cout << "Released an old process with bgid " << i << endl;
                    }
                }
                
                
                
                //close(fds[1]);
                
                // we will discuss why waitpid() is preferred over wait()
            }
        }
    }
}