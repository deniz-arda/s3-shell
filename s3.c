#include "s3.h"

///Simple for now, but will be expanded in a following section
void construct_shell_prompt(char shell_prompt[], char lwd[])
{
    char cwd[MAX_PROMPT_LEN-16];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("get cwd failed");
        strcpy(shell_prompt, "[s3]$ ");
        return;
    }
    snprintf(shell_prompt, MAX_PROMPT_LEN, "[%s s3]$ ", cwd);
}

///Prints a shell prompt and reads input from the user
void read_command_line(char line[], char lwd[])
{
    char shell_prompt[MAX_PROMPT_LEN];
    construct_shell_prompt(shell_prompt, lwd);
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

void parse_commands_with_pipes(char line[], char **commands[], int *command_count)
{
    // Split line by pipes
    *command_count = 0;
    char *token = strtok(line, "|");
    
    while (token != NULL && *command_count < MAX_ARGS) {
        // Allocate space for this command's arguments
        commands[*command_count] = malloc(MAX_ARGS * sizeof(char*));
        
        // Trim leading whitespace from token
        while (*token == ' ') token++;
        
        // Parse this command into arguments
        char *arg_token = strtok(token, " ");
        int arg_idx = 0;
        while (arg_token != NULL && arg_idx < MAX_ARGS - 1) {
            commands[*command_count][arg_idx++] = arg_token;
            arg_token = strtok(NULL, " ");
        }
        commands[*command_count][arg_idx] = NULL;
        
        (*command_count)++;
        token = strtok(NULL, "|");
    }
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

    return info;
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

void init_lwd(char lwd[])
{
    if (getcwd(lwd, MAX_PROMPT_LEN-6) == NULL) {
        perror("getcwd failed");
        exit(1);
    }
}

bool is_cd(char line[])
{
    if (strncmp(line, "cd", 2) == 0) {
        return 1;
    }
    return 0;
}

void run_cd(char *args[], int argsc, char lwd[])
{
    char cwd[MAX_PROMPT_LEN - 6];
    
    // Save current directory as "last" directory BEFORE changing
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd failed");
        return;
    }
    
    // Determine where to change directory to
    if (argsc == 1) {
        // No argument: "cd" → go to home directory
        char *home = getenv("HOME");
        if (home == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return;
        }
        
        if (chdir(home) != 0) {
            perror("cd failed");
            return;
        }
    }
    else if (strcmp(args[1], "-") == 0) {
        // Argument is "-": "cd -" → go to previous directory
        if (chdir(lwd) != 0) {
            perror("cd failed");
            return;
        }
        printf("%s\n", lwd);  // Print the directory we switched to
    }
    else {
        // Regular argument: "cd path" → go to specified directory
        if (chdir(args[1]) != 0) {
            perror("cd failed");
            return;
        }
    }
    
    // Update lwd with the old cwd (now that we've successfully changed)
    strcpy(lwd, cwd);
}

bool command_with_redirection(char line[]) 
{
    ///Check occurence of <, >, >>
    if (strchr(line, '<') != NULL || strchr(line, '>') != NULL) {
        return 1;
    } 
    return 0;
}

//check occurence of pipe
bool command_with_pipe(char line[]){
    if (strchr(line, '|')!= NULL){
        return 1;
    }
    return 0;
}

//counts number of pipes
int count_pipes(char line[]){
    int count = 0;
    char *p = line;
    while(p = strtchr(p, '|') != NULL){
        count ++;
        p++;
    }

    return count;
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

void launch_program_with_pipes(char **commands[], int command_count)
{
    if (command_count == 0) return;
    
    int i;
    int pipefds[2 * (command_count - 1)]; // Need 2 FDs per pipe
    
    // Create all pipes
    for (i = 0; i < command_count - 1; i++) {
        if (pipe(pipefds + i * 2) < 0) {
            perror("pipe failed");
            exit(1);
        }
    }
    
    // Fork and execute each command
    for (i = 0; i < command_count; i++) {
        int rc = fork();
        
        if (rc < 0) {
            perror("fork failed");
            exit(1);
        }
        
        if (rc == 0) {
            // Child process
            
            // If not the first command, redirect stdin from previous pipe
            if (i > 0) {
                if (dup2(pipefds[(i - 1) * 2], STDIN_FILENO) < 0) {
                    perror("dup2 failed");
                    exit(1);
                }
            }
            
            // If not the last command, redirect stdout to next pipe
            if (i < command_count - 1) {
                if (dup2(pipefds[i * 2 + 1], STDOUT_FILENO) < 0) {
                    perror("dup2 failed");
                    exit(1);
                }
            }
            
            // Close all pipe file descriptors
            int j;
            for (j = 0; j < 2 * (command_count - 1); j++) {
                close(pipefds[j]);
            }
            
            // Execute the command
            execvp(commands[i][0], commands[i]);
            perror("execvp failed");
            exit(1);
        }
    }
    
    // Parent: close all pipe file descriptors
    for (i = 0; i < 2 * (command_count - 1); i++) {
        close(pipefds[i]);
    }
    
    // Wait for all children
    for (i = 0; i < command_count; i++) {
        wait(NULL);
    }
    
    // Free allocated memory
    for (i = 0; i < command_count; i++) {
        free(commands[i]);
    }
}