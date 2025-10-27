#include "s3.h"

int main(int argc, char *argv[]){

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

        if(is_cd(line)){///Implement this function
            parse_command(line, args, &argsc);
            run_cd(args, argsc, lwd); ///Implement this function
        }
        else if(command_with_redirection(line)){
            ///Command with redirection
            RedirInfo info = parse_redirection(line);
            parse_command(line, args, &argsc);
            launch_program_with_redirection(args, argsc, info);
            reap();
       }
       else ///Basic command
       {
           parse_command(line, args, &argsc);
           launch_program(args, argsc);
           reap();
       }
    }

    return 0;
}
