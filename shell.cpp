#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>

#include <vector>
#include <string>
#include <cstring>
#include <fcntl.h>
#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define YELLOW "\033[1;33m"
#define BLUE "\033[1;34m"
#define WHITE "\033[1;37m"
#define NC "\033[0m"

using namespace std;

int main()
{

    const size_t dic_size = 1024;
    char current_directory[dic_size];

    char prev_directory[dic_size];
    getcwd(prev_directory, dic_size);

    int fd_org_in = dup(0);
    int fd_org_out = dup(1);
    vector<pid_t> background_process;
    for (;;)
    {

        //CHECK BACKGROUND
        int Backstatus;
        for(size_t i = 0; i < background_process.size(); i++){
            waitpid(background_process.at(i), &Backstatus, WNOHANG);
        }

        // need date/time, username, and absolute path to current dir
        auto username = getenv("USER");
        time_t current_time;
        time(&current_time);
        string cur_time = ctime(&current_time);
        // use compy to keep previous
        getcwd(current_directory, dic_size);

        cout << cur_time.substr(4, 16) << username << ":" << current_directory;

        cout << YELLOW << "$" << NC << " ";

        // get user inputted command
        string input;
        getline(cin, input);

        if (input == "exit")
        { // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl
                 << "Goodbye" << NC << endl;
            break;
        }

        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError())
        { // continue to next prompt if input had an error
            continue;
        }

        // If it is a single commeand
        // cout << tknr.commands.size() << endl;
        if (tknr.commands.size() == 1)
        {
            if (tknr.commands.at(0)->args.at(0) == "cd")
            {
                string new_dir = tknr.commands.at(0)->args.at(1);
                if (new_dir == "-")
                {
                    // cout << "prev " << prev_directory << endl;
                    // cout << "current " << current_directory << endl;

                    strcpy(current_directory, prev_directory);
                    getcwd(prev_directory, dic_size);
                    if (chdir((char *)(current_directory)) != 0)
                    {
                        cerr << "Directory not found" << endl; // what should this output be
                    }
                }
                else if (new_dir.substr(0, 2) == "..")
                {
                    getcwd(prev_directory, dic_size);
                    if (chdir((char *)new_dir.c_str()) != 0)
                    {
                        cerr << "Directory not found" << endl; // what should this output be
                    }
                }
                else
                {
                    getcwd(prev_directory, dic_size);
                    strcpy(current_directory, new_dir.c_str());
                    // cout << "prev " << prev_directory << endl;
                    // cout << "current " << current_directory << endl;
                    if (chdir((char *)current_directory) != 0)
                    {
                        cerr << "Directory not found" << endl; // what should this output be
                    }
                }
            }
            else
            {
                pid_t pid = fork();
                if (pid < 0)
                { // error check
                    perror("fork");
                    exit(2);
                }

                if (pid == 0)
                {   
                    //Check if its a background proccess
                    if (tknr.commands.at(0)->hasInput())
                    {
                        int fd = open(tknr.commands.at(0)->in_file.c_str(), O_RDONLY, 0600);
                        dup2(fd, 0);
                    }
                    if (tknr.commands.at(0)->hasOutput())
                    {
                        int fd = open(tknr.commands.at(0)->out_file.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
                        dup2(fd, 1);
                    }

                    char **args = new char *[tknr.commands.at(0)->args.size()];
                    for (size_t j = 0; j < tknr.commands.at(0)->args.size(); j++)
                    {
                        args[j] = (char *)tknr.commands.at(0)->args.at(j).c_str();
                    }

                    //

                    if (execvp(args[0], args) < 0)
                    { // error check
                        perror("execvp");
                        exit(2);
                    }
                    delete[] args;
                }
                else
                { // if parent, wait for child to finish
                    int status = 0;
                    if (tknr.commands.at(0)->isBackground()){ //check this 
                        background_process.push_back(pid);
                    }
                    else{
                        waitpid(pid, &status, 0);
                        if (status > 1)
                        { // exit if child didn't exec properly
                            exit(status);
                        }   
                    }
                    
                }
            }
        }
        else
        {
            vector<pid_t> pid_list;
            for (size_t i = 0; i < tknr.commands.size(); i++)
            {
                // create pipe
                int pip[2]; /* child-to-parent */
                char buffer[10];
                memset(buffer, 0, sizeof(buffer));
                if ((pipe(pip) < 0))
                {
                    printf("ERROR: Failed to open pipe\n");
                    exit(1);
                }

                pid_t pid = fork();

                if (pid < 0)
                { // error check
                    perror("fork");
                    exit(2);
                }

                if (pid == 0)
                { // if child, exec to run command
                    // If it it First or last commond check for redirection
                    if ((i == tknr.commands.size() - 1) || (i == 0))
                    {
                        if (tknr.commands.at(i)->hasInput())
                        {
                            int fd = open(tknr.commands.at(i)->in_file.c_str(), O_RDONLY, 0600);
                            dup2(fd, 0);
                        }
                        if (tknr.commands.at(i)->hasOutput())
                        {
                            int fd = open(tknr.commands.at(i)->out_file.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
                            dup2(fd, 1);
                        }
                    }

                    // If Not last comman redirect to write end of pipe
                    if (i != tknr.commands.size() - 1)
                    {
                        close(pip[0]);
                        // standard out to point write end
                        dup2(pip[1], 1);
                    }

                    // Run of execvp
                    char **args = new char *[tknr.commands.at(i)->args.size()];
                    for (size_t j = 0; j < tknr.commands.at(i)->args.size(); j++)
                    {
                        args[j] = (char *)tknr.commands.at(i)->args.at(j).c_str();
                    }

                    if (execvp(args[0], args) < 0)
                    { // error check
                        perror("execvp");
                        exit(2);
                    }
                    delete[] args;
                }
                else
                { // if parent, wait for child to finish

                    // close write end
                    close(pip[1]);
                    // redirect to the read end
                    dup2(pip[0], 0);
                    
                    if (tknr.commands.at(i)->isBackground()){ //check this 
                        background_process.push_back(pid);
                    }
                    else{
                        pid_list.push_back(pid);
                    }

                    if (i == tknr.commands.size() - 1)
                    {
                        int status = 0;
                        for (size_t j = 0; j < pid_list.size(); j++)
                        {
                            status = 0;
                            waitpid(pid_list.at(j), &status, 0);
                            if (status > 1)
                            { // exit if child didn't exec properly
                                exit(status);
                            }
                        }
                    }
                }
            }
        }
        dup2(fd_org_in, 0);
        dup2(fd_org_out, 1);
    }
}
