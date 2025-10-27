#include "s3.h"

///Simple for now, but will be expanded in a following section
void construct_shell_prompt(char shell_prompt[])
{
    strcpy(shell_prompt, "[s3]$ ");
}

///Prints a shell prompt and reads input from the user
void read_command_line(char line[])
{
    char shell_prompt[MAX_PROMPT_LEN];
    construct_shell_prompt(shell_prompt);
    printf("%s", shell_prompt);

    ///See man page of fgets(...)
    if (fgets(line, MAX_LINE, stdin) == NULL)
    {
        perror("fgets failed");
        exit(1);
    }
    ///Remove newline (enter)
    line[strlen(line) - 1] = '\0';
}

void parse_command(char line[], char *args[], int *argsc)
{
    ///Implements simple tokenization (space delimited)
    ///Note: strtok puts '\0' (null) characters within the existing storage, 
    ///to split it into logical cstrings.
    ///There is no dynamic allocation.

    ///See the man page of strtok(...)
    char *token = strtok(line, " ");
    *argsc = 0;
    while (token != NULL && *argsc < MAX_ARGS - 1)
    {
        args[(*argsc)++] = token;
        token = strtok(NULL, " ");
    }
    
    args[*argsc] = NULL; ///args must be null terminated
}

RedirInfo parse_redirection(char line[]) 
{
    /// First initialize the redirection type to null
    RedirInfo info;
    info.type = NO_REDIR;
    info.filename = NULL;
    char* pos;
    
    /// Check for '>>' first
    pos = strstr(line, ">>");
    if (pos != NULL)
    {
        info.type = APPEND_OUTPUT_REDIR;
       *pos = '\0';
        pos += 2;
        while (*pos == ' ') pos++;
        info.filename = pos;
        return info;
    }
    
    /// Check for '>'
    pos = strstr(line, ">");
    if (pos!= NULL)
    {
        info.type = OUTPUT_REDIR;
        *pos = '\0';
        pos += 1;
        while (*pos == ' ') pos++;
        info.filename = pos;
        return info;
    }
    
    /// Check for '<'
    pos = strstr(line, "<");
    if (pos!= NULL)
    {
        info.type = INPUT_REDIR;
        *pos = '\0';
        pos += 1;
        while (*pos == ' ') pos++;
        info.filename = pos;
        return info;
    }
}

void child_with_output_redirected(char *args[], int argsc, RedirInfo info)
{
    int flags;

    if (info.type == APPEND_OUTPUT_REDIR) {
        flags = O_WRONLY | O_CREAT | O_APPEND;
    }
    else {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    }

    int fd = open(info.filename, flags, 0644);
    if (fd < 0) {
        perror("open failed");
        exit(1);
    }
    if (dup2(fd, STDOUT_FILENO) < 0) {
        perror("dup2 failed");
        close(fd);
        exit(1);
    }
    close(fd);

    execvp(args[ARG_PROGNAME], args);
    /// If it returns, execvp failed
    perror("execvp failed");
    exit(1);
}

void child_with_input_redirected(char *args[], int argsc, RedirInfo info)
{
    int fd = open(info.filename, O_RDONLY);

    if (fd < 0) {
        perror("open failed");
        exit(1);
    }
    if (dup2(fd, STDIN_FILENO) < 0) {
        perror("dup2 failed");
        close(fd);
        exit(1);
    }
    close(fd);
    
    execvp(args[ARG_PROGNAME], args);
    /// If it returns, execvp failed
    perror("execvp failed");
    exit(1);
}

void launch_program_with_redirection(char *args[], int argsc, RedirInfo info)
{
    int rc = fork();

    if (rc < 0) {
        perror("fork failed");
        exit(1);
    }

    else if (rc == 0) {
        if (info.type == OUTPUT_REDIR || info.type == APPEND_OUTPUT_REDIR) {
            child_with_output_redirected(args, argsc, info);
        }
        else {
            child_with_input_redirected(args, argsc, info);
        }
    }
}

bool command_with_redirection(char line[]) 
{
    ///Check occurence of <, >, >>
    if (strchr(line, '<') != NULL || strchr(line, '>') != NULL) {
        return 1;
    } 
    return 0;
}
///Launch related functions
void child(char *args[], int argsc)
{
    execvp(args[ARG_PROGNAME], args);
    ///If it returns, execvp failed
    perror("execvp failed");
    exit(1); 
}

void launch_program(char *args[], int argsc)
{
    /// Handle the 'exit' command so that the shell, not the child process exits
    if (strcmp(args[ARG_PROGNAME], "exit") == 0) {
        exit(0);
    }

    int rc = fork();
    if (rc < 0) {
        perror("fork failed");
        exit(1);
    }
    else if (rc == 0) {
        child(args, argsc);
    }
}
