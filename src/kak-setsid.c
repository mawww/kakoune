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
            perror("kak-setsid: fork");
            return 2;
        case 0:
            setsid();
            execvp(argv[1], argv+1);
            perror("kak-setsid: execvp");  /* execvp() should not return. */
            return 3;
        default:
            break;
    }
}
