#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>   
#include <signal.h>    

#define MAX_TOKENS 256
#define BUF_SIZE   4096

int readline(int fd, char *out, int maxlen) {
    int len = 0;
    char c;

    while (len < maxlen - 1) {
        ssize_t n = read(fd, &c, 1);
        if (n < 0){
            return -1;
        }
        if (n == 0){
            break;
        }
        if (c == '\n') {
            break;
        }

        out[len++] = c;
    }

    out[len] = '\0';
    return len;
}

int tokenize(char *line, char **tokens, int max) {
    char *comment = strchr(line, '#');
    if (comment) *comment = '\0';

    int count = 0;
    char *p = line;

    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        if (count >= max - 1) { fprintf(stderr, "mysh: too many tokens\n"); break; }

        if (*p == '|' || *p == '<' || *p == '>') {
            tokens[count++] = p;
            p++;
            if (*p == ' ' || *p == '\t' || *p == '\0') {
                
            } else {
                tokens[count-1][1] = '\0';
            }
            continue;
        }

        tokens[count++] = p;
        while (*p && *p != ' ' && *p != '\t' &&
               *p != '|' && *p != '<' && *p != '>') {
            p++;
        }
        if (*p) *p++ = '\0';
    }

    tokens[count] = NULL;
    return count;
}

int parse_command(char **tokens, int ntok,
                  char **argv, int max_argv,
                  char **in_file, char **out_file) {
    *in_file  = NULL;
    *out_file = NULL;
    int argc  = 0;
    int i     = 0;

    while (i < ntok) {
        if (strcmp(tokens[i], "<") == 0) {
            if (i + 1 >= ntok) {
                fprintf(stderr, "mysh: syntax error: missing file after <\n");
                return -1;
            }
            *in_file = tokens[i + 1];
            i += 2;
        } else if (strcmp(tokens[i], ">") == 0) {
            if (i + 1 >= ntok) {
                fprintf(stderr, "mysh: syntax error: missing file after >\n");
                return -1;
            }
            *out_file = tokens[i + 1];
            i += 2;
        } else {
            if (argc < max_argv - 1)
                argv[argc++] = tokens[i];
            i++;
        }
    }
    argv[argc] = NULL;
    return 0;
}


void report_status(int raw_status) {
    if (WIFEXITED(raw_status)) {
        int code = WEXITSTATUS(raw_status);
        if (code != 0)
            printf("Exited with status %d\n", code);
    } else if (WIFSIGNALED(raw_status)) {
        int sig = WTERMSIG(raw_status);
       
        printf("Terminated by signal %d: %s\n", sig, strsignal(sig));
    }
}


int execute_command(char **cmd_argv, char *in_file, char *out_file,
                    int interactive) {
    (void)in_file; (void)out_file; (void)interactive;

    printf("[stub] would run: %s\n", cmd_argv[0]);
    for (int i = 1; cmd_argv[i]; i++)
        printf("  arg[%d] = %s\n", i, cmd_argv[i]);

    return 0;   
}


int main(int argc, char *argv[]) {
    int fd;
    int interactive;

    if (argc == 2) {
        fd = open(argv[1], O_RDONLY);
        if (fd < 0) { perror(argv[1]); return EXIT_FAILURE; }
        interactive = 0;
    } else if (argc == 1) {
        fd = STDIN_FILENO;
        interactive = isatty(fd);
    } else {
        fprintf(stderr, "Usage: mysh [script]\n");
        return EXIT_FAILURE;
    }

    if (interactive) printf("Welcome to my shell!\n");
    int last_status = 0;
    int should_exit = 0;

    while (!should_exit) {
        
        if (interactive && last_status != 0)
            report_status(last_status);

        if (interactive) { printf("$ "); fflush(stdout); }

        char line[BUF_SIZE];
        int len = readline_fd(fd, line, sizeof(line));
        if (len < 0) break;

        char *tokens[MAX_TOKENS];
        int ntok = tokenize(line, tokens, MAX_TOKENS);
        if (ntok == 0) continue;

        if (strcmp(tokens[0], "exit") == 0) {
            should_exit = 1;
            break;
        }

        char *cmd_argv[MAX_TOKENS];
        char *in_file, *out_file;

        if (parse_command(tokens, ntok, cmd_argv, MAX_TOKENS,
                          &in_file, &out_file) < 0) {

            last_status = 1;
            continue;
        }

        if (!cmd_argv[0]) continue;
        int ret = execute_command(cmd_argv, in_file, out_file, interactive);
        last_status = (ret & 0xff) << 8;
    }

    if (interactive) printf("Exiting my shell.\n");
    if (fd != STDIN_FILENO) close(fd);
    return EXIT_SUCCESS;   
}