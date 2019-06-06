#include <stdio.h>
#include <unistd.h>

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
            execvp(argv[1], argv+1);
            /* execvp() should not return. */
            fprintf(stderr, "The given command could not be executed.\n");
            return 3;
        default:
            break;
    }
}
