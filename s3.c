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

int parse_pipes(char line[], char *commands[])
{
    int count = 0;
    char *token = strtok(line, "|");
    
    while (token != NULL && count < MAX_ARGS) {
        // Trim leading spaces
        while (*token == ' ' || *token == '\t') token++;

        char *end = token + strlen(token) - 1;
        while (end > token && (*end == ' ' || *end == '\t' || *end == '\n')) {
            *end = '\0';
            end--;
        }

        commands[count++] = token;
        token = strtok(NULL, "|");
    }
    return count;
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

//separate single line into multiple command lines
int parse_batch(char line[], char *lines[])
{
    int count = 0;
    char *start = line;
    int depth = 0;
    
    for (char *p = line; *p != '\0'; p++) {
        if (*p == '(') depth++;
        if (*p == ')') depth--;
        
        if (*p == ';' && depth == 0) {
            // Found a semicolon outside parentheses
            *p = '\0';  // Null-terminate this command
            
            // Trim the command
            char *token = start;
            while (*token == ' ' || *token == '\t') token++;
            
            char *end = p - 1;
            while (end > token && (*end == ' ' || *end == '\t' || *end == '\n')) {
                *(end + 1) = '\0';
                end--;
            }
            
            lines[count++] = token;
            start = p + 1;  // Next command starts after the semicolon
        }
    }
    
    // Handle the last command (after the last semicolon or the only command)
    if (*start != '\0') {
        char *token = start;
        while (*token == ' ' || *token == '\t') token++;
        
        char *end = start + strlen(start) - 1;
        while (end > token && (*end == ' ' || *end == '\t' || *end == '\n')) {
            *(end + 1) = '\0';
            end--;
        }
        
        lines[count++] = token;
    }
    
    return count;
}

void extract_subshell(char *line, char *out)
{
    char *start = strchr(line, '(');
    if (start == NULL) {
        out[0] = '\0';
        return;
    }
    
    // Find matching closing parenthesis
    int depth = 0;
    char *end = start;
    while (*end != '\0') {
        if (*end == '(') depth++;
        if (*end == ')') {
            depth--;
            if (depth == 0) break;
        }
        end++;
    }
    
    if (depth != 0 || *end != ')') {
        out[0] = '\0';
        return;
    }

    size_t len = end - start - 1;
    strncpy(out, start + 1, len);
    out[len] = '\0';

    //trim leading whitespace
    char *trimmed = out;
    while (*trimmed == ' ' || *trimmed == '\t'){
        trimmed++;
    }

    //trim trailing whitespace
    char *trail = out + strlen(out) - 1;
    while (trail > out && (*trail == ' ' || *trail == '\t' || *trail == '\n')) {
        *trail = '\0';
        trail--;
    }

    //move trimmed string to beginning if needed
    if (trimmed != out){
        memmove(out, trimmed, strlen(trimmed) + 1);
    }
}

// Functions to check for redirection, cd, and pipes
bool is_command_with_redirection(char line[]) 
{
    int depth = 0;
    for (char *p = line; *p != '\0'; p++) {
        if (*p == '(') depth++;
        if (*p == ')') depth--;
        if ((*p == '<' || *p == '>') && depth == 0) {
            return 1;
        }
    }
    return 0;
}

bool is_cd(char line[])
{
    if (strncmp(line, "cd", 2) == 0) {
        return 1;
    }
    return 0;
}

bool is_command_with_pipe(char line[])
{
    int depth = 0;
    for (char *p = line; *p != '\0'; p++) {
        if (*p == '(') depth++;
        if (*p == ')') depth--;
        if (*p == '|' && depth == 0) {
            return 1;
        }
    }
    return 0;
}

bool is_command_with_batch(char line[])
{
    int depth = 0;
    for (char *p = line; *p != '\0'; p++) {
        if (*p == '(') depth++;
        if (*p == ')') depth--;
        if (*p == ';' && depth == 0) {
            return 1;  // Found semicolon outside parentheses
        }
    }
    return 0;
}

bool is_command_with_subshell(char line[])
{
    if (strchr(line, '(') != NULL){
        return 1;
    }
    return 0;
}

void init_lwd(char lwd[])
{
    if (getcwd(lwd, MAX_PROMPT_LEN-6) == NULL) {
        perror("getcwd failed");
        exit(1);
    }
}
void run_cd(char *args[], int argsc, char lwd[])
{
    char cwd[MAX_PROMPT_LEN - 6];
    
    // Save current directory as last directory
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd failed");
        return;
    }
    
    if (argsc == 1) {
        // if only argument is "cd" go to home directory
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
        // "cd -" (go to previous directory)
        if (chdir(lwd) != 0) {
            perror("cd failed");
            return;
        }
        printf("%s\n", lwd);  // Print the directory we switched to
    }
    else {
        // go to directory stated in the argument
        if (chdir(args[1]) != 0) {
            perror("cd failed");
            return;
        }
    }
    
    // Update lwd with the old cwd
    strcpy(lwd, cwd);
}

///Launch related functions
void child(char *args[], int argsc, int *pipefds, int pipe_count, int cmd_idx)
{
    
    // Handles pipes if they exist
    if (pipefds != NULL) {
        
        // read from previous pipe unless it is the first command
        if (cmd_idx > 0) {
            dup2(pipefds[(cmd_idx -1) * 2], STDIN_FILENO);
        }
        
        // write to next pipe unless it is the last command
        if (cmd_idx < pipe_count) {
            dup2(pipefds[cmd_idx*2 + 1], STDOUT_FILENO);
        }
        // close all pipe file descriptors
        for (int i = 0; i < pipe_count * 2; i++) {
            close(pipefds[i]);
        }

    }

    
    
    execvp(args[ARG_PROGNAME], args);
    ///If it returns, execvp failed
    perror("execvp failed");
    exit(1); 
}

void child_with_output_redirected(char *args[], int argsc, RedirInfo info, int *pipefds, int pipe_count, int cmd_idx)
{
    // Handle pipes
    if (pipefds != NULL && cmd_idx > 0) {
        dup2(pipefds[(cmd_idx-1) * 2], STDIN_FILENO);
    }
    
    if (pipefds != NULL) {
        for (int i = 0; i < pipe_count * 2; i++){
            close(pipefds[i]);
        }
    }
    
    // Redirection
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

void child_with_input_redirected(char *args[], int argsc, RedirInfo info, int *pipefds, int pipe_count, int cmd_idx)
{
    // Handle pipes - send output to the next command
    if (pipefds != NULL && cmd_idx < pipe_count) {
        dup2(pipefds[cmd_idx*2 + 1], STDOUT_FILENO);
    }
    
    if (pipefds != NULL) {
        for (int i = 0; i < pipe_count * 2; i++) {
            close(pipefds[i]);
        }
    }
    
    // Redirection
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

void child_subshell(char *subcmd, int *pipefds, int pipe_count, int cmd_idx) 
{
    //handle pipes if they exist
    if (pipefds != NULL){
        if (cmd_idx > 0){
            dup2(pipefds[(cmd_idx-1)*2], STDIN_FILENO);
        }

        if (cmd_idx < pipe_count){
            dup2(pipefds[cmd_idx * 2 + 1], STDOUT_FILENO);
        }

        for (int i = 0; i < pipe_count * 2; i++){
            close(pipefds[i]);
        }
    }

    //execute the subshell
    char *argv[] = {"./s3shell", "-c", subcmd, NULL};
    execvp(argv[0], argv);
    perror("execvp failed for subshell");
    exit(1);
}


void launch_program(char *args[], int argsc, int *pipefds, int pipe_count, int cmd_idx)
{
    /// shell exits for 'exit' command
    if (strcmp(args[ARG_PROGNAME], "exit") == 0) {
        exit(0);
    }
    
    int rc = fork();
    if (rc < 0) {
        perror("fork failed");
        exit(1);
    }
    else if (rc == 0) {
        child(args, argsc, pipefds, pipe_count, cmd_idx);
    }
}

void launch_program_with_redirection(char *args[], int argsc, RedirInfo info, int *pipefds, int pipe_count, int cmd_idx)
{
    int rc = fork();
    
    if (rc < 0) {
        perror("fork failed");
        exit(1);
    }
    
    else if (rc == 0) {
        if (info.type == OUTPUT_REDIR || info.type == APPEND_OUTPUT_REDIR) {
            child_with_output_redirected(args, argsc, info, pipefds, pipe_count, cmd_idx);
        }
        else {
            child_with_input_redirected(args, argsc, info, pipefds, pipe_count, cmd_idx);
        }
    }
}

void launch_program_with_pipes(char *commands[], int command_count)
{
    // Create flat array of pipe fds
    int pipefds[(command_count - 1) * 2];
    
    // Create all pipes before forking
    for (int i = 0; i < command_count - 1; i++) {
        if (pipe(pipefds + i*2) < 0) {
            perror("pipe failed");
            exit(1);
        }
    }
    
    // Each command becomes a separate child process
    for (int i = 0; i < command_count; i++) {
        char cmd_buffer[MAX_LINE];
        strcpy(cmd_buffer, commands[i]);

        //check for subshell
        if(is_command_with_subshell(cmd_buffer)){
            char inner[MAX_LINE];
            extract_subshell(cmd_buffer, inner);
            launch_subshell(inner, pipefds, command_count - 1, i);
        }
        // Check for file redirection
        else if (is_command_with_redirection(cmd_buffer)) {
            char cmd_copy[MAX_LINE];
            strcpy(cmd_copy, cmd_buffer);  // Make a copy for parse_redirection
            RedirInfo info = parse_redirection(cmd_copy);
            char *args[MAX_ARGS];
            int argsc;  
            parse_command(cmd_copy, args, &argsc);
            // Handle both pipes and redirection
            launch_program_with_redirection(args, argsc, info, pipefds, command_count - 1, i);
        } else {
            char *args[MAX_ARGS];
            int argsc;
            parse_command(cmd_buffer, args, &argsc);
            // Only handle pipes
            launch_program(args, argsc, pipefds, command_count - 1, i);
        }
    }
    
    // Parent closes all pipes
    for (int i = 0; i < (command_count - 1) * 2; i++) {
        close(pipefds[i]);
    }
    
    // Wait for all children
    for (int i = 0; i < command_count; i++) {
        wait(NULL);
    }
}

void launch_program_with_batch(char line[])
{
    char line_copy[MAX_LINE];
    strcpy(line_copy, line);  // Work on a copy
    
    char *batch_commands[MAX_ARGS];
    int batch_count = parse_batch(line_copy, batch_commands);

    char lwd_local[MAX_PROMPT_LEN-6];
    init_lwd(lwd_local);

    char *args[MAX_ARGS];
    int argsc;
    
    //for each batch command execute sequentially
    for (int i = 0; i < batch_count; i++){
        char cmd[MAX_LINE];
        strcpy(cmd, batch_commands[i]);

        if(is_cd(cmd)){
            parse_command(cmd, args, &argsc);
            run_cd(args, argsc, lwd_local);
        }
        else if (is_command_with_subshell(cmd)) {
            char inner[MAX_LINE];
            extract_subshell(cmd, inner);
            launch_subshell(inner, NULL, 0, 0);
            reap();
        }
        else if(is_command_with_pipe(cmd)){
            char cmd_copy[MAX_LINE];
            strcpy(cmd_copy, cmd);  // Make a copy for parse_pipes
            char *pipe_cmds[MAX_ARGS];
            int command_count = parse_pipes(cmd_copy, pipe_cmds);
            launch_program_with_pipes(pipe_cmds, command_count);
        }
        else if (is_command_with_redirection(cmd)){
            RedirInfo info = parse_redirection(cmd);
            parse_command(cmd, args, &argsc);
            launch_program_with_redirection(args, argsc, info, NULL, 0, 0);
            reap();
        }
        else {
            parse_command(cmd, args, &argsc);
            launch_program(args, argsc, NULL, 0, 0);
            reap();
        }
    }
}


void launch_subshell(char *subcmd, int *pipefds, int pipe_count, int cmd_idx)

{
    pid_t pid = fork();

    if (pid < 0){
        perror("fork failed");
        return;
    }

    else if (pid == 0){
        //call child function
        child_subshell(subcmd, pipefds, pipe_count, cmd_idx);
    }
}




void launch_subshell_with_redirection(char *subcmd, RedirInfo info)
{
    
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("fork failed");
        return;
    }
    else if (pid == 0) {
        // Child: set up redirection then execute subshell
        
        if (info.type == OUTPUT_REDIR || info.type == APPEND_OUTPUT_REDIR) {
            int flags;
            if (info.type == APPEND_OUTPUT_REDIR) {
                flags = O_WRONLY | O_CREAT | O_APPEND;
            } else {
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
        }
        else if (info.type == INPUT_REDIR) {
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
        }
        
        // Execute the subshell with redirected I/O
        char *argv[] = {"./s3shell", "-c", subcmd, NULL};
        execvp(argv[0], argv);
        perror("execvp failed for subshell");
        exit(1);
    }
}
