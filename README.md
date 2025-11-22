# s3 - Unix-Like Shell Implemented in C
 ### Software Systems Assignment 1
 **Contributors**: Deniz Yilmazkaya & Lila Acanal
# Table of Contents

* [Introduction](#introduction)
* [Running the Shell](#running-the-shell)
* [1. Basic Commands](#1-basic-commands)
* [2. Commands with Redirection](#2-commands-with-redirection)
* [3. Support for cd](#3-support-for-cd)
* [4. Commands with Pipes](#4-commands-with-pipes)
* [5. Batched Commands](#5-batched-commands)
* [6. PE 1: Subshells](#6-pe-1-subshells)
* [7. PE 2: Nested Subshells](#7-pe-2-nested-subshells)
---
# Introduction
Software Systems assignment s3-shell is a Unix-style shell implemented in C. It supports fundamental shell operations including commands with redirection, pipes, directory navigation, batched commands, and subshells.

## Code Structure

### Files
* **s3main.c** – main control flow and command dispatch logic
* **s3.h** – type definitions, constants, and function declarations
* **s3.c** – implementation of all shell functions

---
# Running the Shell
Build with GCC (on either Linux or macOS):

```bash
# compile
gcc *.c -o s3

#run
./s3
```
---
# 1. Basic Commands

The shell executes commands using `fork()`, `execvp()`, and `wait()` system calls.

### Implementation Details
* **`launch_program()`** – forks a child process to execute commands
* **`child()`** – uses `execvp()` to replace the child process with the target program
* **`reap()`** – waits for child process completion

### Supported Commands
The shell supports standard Unix commands such as:
* `ls`, `pwd`, `whoami`, `cat`, `wc`, `echo`, etc.
* `exit` – terminates the shell

### Example Usage
```bash
[/home/user s3]$ ls -la
[/home/user s3]$ whoami
[/home/user s3]$ exit
```

---

# 2. Commands with Redirection

The shell supports I/O redirection operators for file input and output.

### Supported Redirection Types
* **Output redirection** (`>`) – overwrites file
* **Append output** (`>>`) – appends to file
* **Input redirection** (`<`) – reads from file

### Implementation Details
* **`parse_redirection()`** – detects and parses redirection operators
* **`launch_program_with_redirection()`** – forks and executes with redirected I/O
* **`child_with_output_redirected()`** – uses `dup2()` and `open()` for output redirection
* **`child_with_input_redirected()`** – uses `dup2()` and `open()` for input redirection

### Example Usage
```bash
[/home/user s3]$ sort txt/phrases.txt > txt/phrases_sorted.txt
[/home/user s3]$ grep June < txt/calendar.txt
[/home/user s3]$ head -n 5 txt/phrases.txt >> txt/phrases_sorted.txt
```

---

# 3. Support for cd

The `cd` command is implemented as a shell builtin to change the working directory.

### Implementation Details
* **`is_cd()`** – detects cd command
* **`run_cd()`** – uses `chdir()` to change directory
* **`init_lwd()`** – initializes last working directory tracking

### Supported cd Operations
* `cd` – navigate to HOME directory
* `cd -` – return to previous directory (prints the directory path)
* `cd <path>` – navigate to specified path

### Prompt Display
The shell prompt displays the current working directory:
```bash
[/home/user s3]$ cd /tmp
[/home/user/tmp s3]$ cd -
/home/user
[/home/user s3]$
```

The prompt is constructed using `getcwd()` in `construct_shell_prompt()`.

---
# 4. Commands with Pipes

The shell supports pipeline operations using the `|` operator to chain multiple commands.

### Implementation Details
* **`parse_pipes()`** – splits command line by pipe operators
* **`launch_program_with_pipes()`** – creates pipes and manages multiple child processes
* **`pipe()`** – creates pipe file descriptors for inter-process communication
* **`dup2()`** – redirects stdin/stdout between piped commands

### Features
* Supports pipelines of arbitrary length
* Compatible with file redirection within piped commands
* Each command in the pipeline runs in a separate child process

### Example Usage
```bash
[/home/user s3]$ cat txt/phrases.txt | sort > txt/phrases_sorted.txt
[/home/user s3]$ tr a-z A-Z < txt/phrases.txt | grep BURN | sort | wc -l
```
---
