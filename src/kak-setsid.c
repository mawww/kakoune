#include <stdio.h>
#include <unistd.h>

void execute_command_in_arguments(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    if(argc < 2) {
        fprintf(stderr, "Give a command to execute as argument to %s.\n",
                argv[0]);
        return 1;
    }
    
    switch(fork()) {
        case -1:
            fprintf(stderr, "Failed to create child process.\n");
            return 2;
        case 0:
            setsid();
            execute_command_in_arguments(argc, argv);
            return 3;  /* execute_command() should not return. */
        default:
            break;
    }
}

void execute_command_in_arguments(int argc, char *argv[])
{
    int i;
    char *command[argc];
    command[argc-1] = NULL;
    for(i=0; i<(argc-1); i++) {
        command[i] = argv[i+1];
    }
    execvp(command[0], command);
    fprintf(stderr, "The given command could not be executed.\n");
}
