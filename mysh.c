#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>


#define MAX_TOKENS 256
#define BUF_SIZE   4096

int readline(int fd, char *out, int maxlen) {
    int len = 0;
    char c;

    while (len < maxlen - 1) {
        ssize_t n = read(fd, &c, 1);

        if (n < 0) return -1;  
        if (n == 0) {
            
            if (len == 0) return -1;  
            break;
        }
        if (c == '\n') break;  

        out[len++] = c;
    }

    out[len] = '\0';  
    return len;
}


int tokenize(char *line, char **tokens, int max) {
    char *comment = strchr(line, '#');
    if (comment != NULL) {
        *comment = '\0';
    }

    int count = 0;   
    char *p = line;  

    while (*p) { 


        while (*p == ' ' || *p == '\t') {
            p++;
        }

      
        if (*p == '\0') break;

        if (count >= max - 1) {
            fprintf(stderr, "mysh: too many tokens\n");
            break;
        }
        tokens[count++] = p;
        while (*p && *p != ' ' && *p != '\t') {
            p++;
        }
        if (*p) {
            *p++ = '\0';
        }
    }
    tokens[count] = NULL; 
    return count;
}

int main(int argc, char *argv[]) {
    int fd;
    int interactive;

    if (argc == 2) {
        fd = open(argv[1], O_RDONLY);
        if (fd < 0) {
            perror(argv[1]);
            return 1;
        }
        interactive = 0;
    } else if (argc == 1) {
        
        fd = STDIN_FILENO;
        interactive = isatty(fd);
    } else {
        fprintf(stderr, "Usage: mysh [script]\n");
        return 1;
    }

    if (interactive) {
        printf("Welcome to my shell!\n");
    }

    while (1) {
        if (interactive) {
            printf("$ ");       
            fflush(stdout);     
        }

       
        char line[BUF_SIZE];
        int len = readline(fd, line, sizeof(line));


        if (len < 0) break;

        char *tokens[MAX_TOKENS];
        int ntok = tokenize(line, tokens, MAX_TOKENS);
        if (ntok == 0) continue;
        printf("You typed %d token(s):\n", ntok);
        int i;
        for (i = 0; i < ntok; i++) {
            printf("  token[%d] = \"%s\"\n", i, tokens[i]);
        }
    }

    if (interactive) {
        printf("Exiting my shell.\n");
    }

    if (fd != STDIN_FILENO) close(fd);
    return 0;
}