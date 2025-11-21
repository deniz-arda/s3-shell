#include "s3.h"
char s3_path[MAX_LINE];



int main(int argc, char *argv[]){

    //subshell mode
    if (argc == 3 && strcmp(argv[1], "-c") == 0){

        //argv[2] contains a full command line 
        launch_program_with_batch(argv[2]);
        return 0;
    }

    
    ///Stores the command line input
    char line[MAX_LINE];

    ///The last (previous) working directory 
    char lwd[MAX_PROMPT_LEN-6]; 

    init_lwd(lwd);///Implement this function: initializes lwd with the cwd (using getcwd)

    //Stores pointers to command arguments.
    ///The first element of the array is the command name.
    char *args[MAX_ARGS];

    ///Stores the number of arguments
    int argsc;

    while (1) {

        read_command_line(line, lwd); ///Notice the additional parameter (required for prompt construction)

        if (is_command_with_batch(line)){
            launch_program_with_batch(line);
        }

        else if(is_command_with_pipe(line)) {
            char *commands[MAX_ARGS];
            int command_count = parse_pipes(line, commands);
            launch_program_with_pipes(commands, command_count);
        }

        else if(is_cd(line)){
            parse_command(line, args, &argsc);
            run_cd(args, argsc, lwd);
        }

        else if(is_command_with_redirection(line)){

            // Check if it's a subshell with redirection
            if (is_command_with_subshell(line)) {

                // Make a copy for parsing redirection
                char line_copy[MAX_LINE];
                strcpy(line_copy, line);
        
                RedirInfo info = parse_redirection(line_copy);
        
                // Extract subshell from the original line
                char inner[MAX_LINE];
                extract_subshell(line, inner);
        
                launch_subshell_with_redirection(inner, info, NULL, 0, 0);
                reap();
            } 
            else {
                // Regular command with redirection
                RedirInfo info = parse_redirection(line);
                parse_command(line, args, &argsc);
                launch_program_with_redirection(args, argsc, info, NULL, 0, 0);
                reap();
            }
        }

        else if(is_command_with_subshell(line)){
            char inner[MAX_LINE];
            extract_subshell(line, inner);
            launch_subshell(inner, NULL, 0, 0);
            reap();
        }

       else ///Basic command
       {
           parse_command(line, args, &argsc);
           launch_program(args, argsc, NULL, 0, 0);
           reap();
       }
    }

    return 0;
}
