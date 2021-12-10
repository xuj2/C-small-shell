#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<ctype.h>
#include<limits.h>
#include<pwd.h>
#include<time.h>
#include<fcntl.h>

#define MAX_LEN 2048
#define MAX_ARG 512

int exitValue = 0;


// initialize the shell and also prompts the user with :
void cmd_prompt(int iflag)
{
    if (iflag)
    {
        printf("\033[H\033[J");
        printf("$ smallsh\n");
    }
    printf(": ");
}

// this is where other commands are executed
void exec_cmd(char *newargv[], int bgFlag, int outFlag, int inFlag, char *outFilename, char *inFilename)
{
    int childStatus;
    pid_t spawnPid = fork();
    int inFD;
    int outFD;
    int result;

    switch(spawnPid) 
    {
        case -1:
                perror("fork() failed\n");
                exit(1);
                break;
        case 0:
                // redirect output
                if (outFlag == 1)
                {
                    outFD = open(outFilename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    result = dup2(outFD, 1);
                    fcntl(outFD, F_SETFD, FD_CLOEXEC); 
                    if (result == -1)
                    {
                        perror("Output redirection error!\n");
                        exit(1);
                    }
                }
                // redirect input
                if (inFlag == 1)
                {
                    inFD = open(inFilename, O_RDONLY);
                    result = dup2(inFD, 0);
                    fcntl(inFD, F_SETFD, FD_CLOEXEC); 
                    if (result == -1)
                    {
                        perror("Input redirection error!\n");
                        exit(2);
                    }
                }
                execvp(newargv[0], newargv);
                fflush(stdout);
                perror("execve");
                exit(2);
                break;
        default:
                waitpid(spawnPid, &childStatus, 0);
                // printf("PARENT(%d): child(%d) terminated. Existing\n", getpid(), spawnPid);
                break;
    }
}

void cmd_process()
{
    char cmd_line[MAX_LEN];
    int bflag = 0; // 1 if input is blank line
    fgets(cmd_line, MAX_LEN, stdin);
    char *saveptr;
    char *token = strtok_r(cmd_line, " ", &saveptr);

    // ignores comments and blank lines
    for (int i = 0; i < (strlen(cmd_line)-1); i++)
    {
        if (isblank(cmd_line[i]) == 0)
        {
            bflag = 1;
        }
    }
    if (cmd_line[0] == '#' || !bflag)
    {
        return;
    }

    // exit built-in command
    if (strncmp(token, "exit", 4) == 0)
    {
        token = strtok_r(NULL, " ", &saveptr);
        if (token == NULL || strcmp(token,"\n") == 0)
        {
            printf("$\n");
            exit(0);
        }
    }
    // status built-in command
    else if (strncmp(token, "status", 6) == 0)
    {
        token = strtok_r(NULL, " ", &saveptr);
        if (token == NULL || strcmp(token,"\n") == 0)
        {
            printf("exit value %d\n", exitValue);
        }
    }
    // cd built-in command
    else if (strncmp(token, "cd", 2) == 0)
    {
        token = strtok_r(NULL, " ", &saveptr);
        if (token == NULL || strcmp(token,"\n") == 0) //if no argument specified
        {
            uid_t uid = getuid();
            struct passwd* pwd = getpwuid(uid);
            chdir(pwd->pw_dir);
            char cwd[PATH_MAX];
            getcwd(cwd, sizeof(cwd));
        }
        else //argument specified
        {
            token[strlen(token) - 1] = '\0';
            int cdflag = chdir(token);
            if (cdflag == 0) 
            {
                char cwd[PATH_MAX];
                getcwd(cwd, sizeof(cwd));
            }
            else {
                printf("Directory does not exist!\n");
            }
        }
    }
    // other commands
    else 
    {
        int outFlag = 0;
        int inFlag = 0;
        int bgFlag = 0;
        char outFilename[100] = "";
        char inFilename[100] = "";

        // extracting the file path or command to execute
        char cmd[100] = "";
        strcat(cmd, token);
        if (cmd[strlen(cmd) - 1] == '\n')
        {
            cmd[strlen(cmd) - 1] = '\0';
        }
        char *argv[MAX_ARG] = {cmd};

        token = strtok_r(NULL, " ", &saveptr);
        int i = 1;
        while (token != NULL) // iterates through input
        {
            if (strcmp(token, ">") == 0) // check for output redirection
            {
                outFlag = 1;
                token = strtok_r(NULL, " ", &saveptr);
                strcat(outFilename, token);
                token = strtok_r(NULL, " ", &saveptr);
                if (token == NULL) 
                {
                    break;
                }
            }
            else if (strcmp(token, "<") == 0) // check for input redirection
            {
                inFlag = 1;
                token = strtok_r(NULL, " ", &saveptr);
                strcat(inFilename, token);
                token = strtok_r(NULL, " ", &saveptr);
            }
            else if (outFlag == 0) // ignores arguments after output redirection detected
            {
                argv[i] = token;
                i++;
                token = strtok_r(NULL, " ", &saveptr);
            }
            else // keep iterating through input
            {
                token = strtok_r(NULL, " ", &saveptr);
            }
        }
        // get rid of the newline characters at the end
        if (outFilename[strlen(outFilename) - 1] == '\n')
        {
            outFilename[strlen(outFilename) - 1] = '\0';
        }
        if (inFilename[strlen(inFilename) - 1] == '\n')
        {
            inFilename[strlen(inFilename) - 1] = '\0';
        }
        if (argv[i - 1][strlen(argv[i - 1]) - 1] == '\n')
        {
            argv[i - 1][strlen(argv[i - 1]) - 1] = '\0';
            if (strcmp(argv[i-1], "&") == 0) //set the flag if & is at the end of input
            {
                bgFlag = 1;
                argv[i-1] = NULL;
            }
        }
        exec_cmd(argv, bgFlag, outFlag, inFlag, outFilename, inFilename);
    }
}

int main()
{
    int iflag = 1; // initialization of shell
    while (1)
    {
        cmd_prompt(iflag);
        iflag = 0;
        cmd_process();
    }
    return 0;
}