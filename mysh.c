#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

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

        if (count >= max - 1) {
            fprintf(stderr, "mysh: too many tokens\n");
            break;
        }

       
        if (*p == '|' || *p == '<' || *p == '>') {
            tokens[count++] = p;   
            p++;
            *(p-1+1) = '\0';     
            p--;   
            tokens[count-1] = p;   
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


static const char *search_dirs[] = { "/usr/local/bin", "/usr/bin", "/bin", NULL };
static char found_path[BUF_SIZE];

const char *find_program(const char *name) {
    for (int i = 0; search_dirs[i]; i++) {
        snprintf(found_path, sizeof(found_path), "%s/%s", search_dirs[i], name);
        if (access(found_path, X_OK) == 0) return found_path;
    }
    return NULL;
}


int is_builtin(const char *name) {
    return strcmp(name, "cd")    == 0 ||
           strcmp(name, "pwd")   == 0 ||
           strcmp(name, "which") == 0 ||
           strcmp(name, "exit")  == 0;
}


int run_builtin(char **argv, int fd_out) {
    if (strcmp(argv[0], "exit") == 0) {
        return -999; /* tell main loop to quit */
    }

    if (strcmp(argv[0], "pwd") == 0) {
        /* print working directory */
        char cwd[BUF_SIZE];
        if (!getcwd(cwd, sizeof(cwd))) { perror("pwd"); return 1; }
        dprintf(fd_out, "%s\n", cwd);
        return 0;
    }

    if (strcmp(argv[0], "cd") == 0) {
        const char *dest;
        if (argv[1] == NULL) {
            /* no arg: go to HOME */
            dest = getenv("HOME");
            if (!dest) { fprintf(stderr, "cd: HOME not set\n"); return 1; }
        } else if (argv[2] != NULL) {
            fprintf(stderr, "cd: too many arguments\n"); return 1;
        } else {
            dest = argv[1];
        }
        if (chdir(dest) != 0) { perror("cd"); return 1; }
        return 0;
    }

    if (strcmp(argv[0], "which") == 0) {
        if (argv[1] == NULL || argv[2] != NULL) {
            fprintf(stderr, "which: wrong number of arguments\n"); return 1;
        }
        if (is_builtin(argv[1])) return 1; /* built-ins not found by which */
        const char *p = find_program(argv[1]);
        if (!p) return 1;
        dprintf(fd_out, "%s\n", p);
        return 0;
    }

    return 1;
}


typedef struct {
    char *argv[MAX_TOKENS];
    char *in_file;   /* < filename */
    char *out_file;  /* > filename */
} Cmd;


int parse_tokens(char **tokens, int ntok, Cmd *cmds, int max_cmds) {
    int ncmds = 0;
    Cmd *cur = &cmds[ncmds++];
    cur->argv[0] = NULL;
    cur->in_file  = NULL;
    cur->out_file = NULL;
    int argc = 0;

    for (int i = 0; i < ntok; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            /* end current command, start a new one */
            cur->argv[argc] = NULL;
            if (ncmds >= max_cmds) { fprintf(stderr, "mysh: too many pipes\n"); return -1; }
            cur = &cmds[ncmds++];
            cur->argv[0] = NULL;
            cur->in_file  = NULL;
            cur->out_file = NULL;
            argc = 0;

        } else if (strcmp(tokens[i], "<") == 0) {
            /* input redirection */
            if (i + 1 >= ntok || strcmp(tokens[i+1], "<") == 0 ||
                                  strcmp(tokens[i+1], ">") == 0) {
                fprintf(stderr, "mysh: syntax error near <\n"); return -1;
            }
            cur->in_file = tokens[++i];

        } else if (strcmp(tokens[i], ">") == 0) {
            /* output redirection */
            if (i + 1 >= ntok || strcmp(tokens[i+1], "<") == 0 ||
                                  strcmp(tokens[i+1], ">") == 0) {
                fprintf(stderr, "mysh: syntax error near >\n"); return -1;
            }
            cur->out_file = tokens[++i];

        } else {
            /* regular argument */
            if (argc < MAX_TOKENS - 1) cur->argv[argc++] = tokens[i];
        }
    }
    cur->argv[argc] = NULL;
    return ncmds;
}


const char *resolve_path(const char *name) {
    if (strchr(name, '/')) return name;   /*  Pathnames */
    return find_program(name);            /* Bare names */
}


void print_prompt(void) {
    char cwd[BUF_SIZE];
    if (!getcwd(cwd, sizeof(cwd))) { printf("$ "); fflush(stdout); return; }
    const char *home = getenv("HOME");
    if (home && strncmp(cwd, home, strlen(home)) == 0)
        printf("~%s$ ", cwd + strlen(home));  /* replace home with ~ */
    else
        printf("%s$ ", cwd);
    fflush(stdout);
}

void print_status(int status) {
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
        printf("Exited with status %d\n", WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        psignal(WTERMSIG(status), "Terminated by signal");
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
        interactive = isatty(fd); /* §1: isatty() decides mode */
    } else {
        fprintf(stderr, "Usage: mysh [script]\n");
        return EXIT_FAILURE;
    }

    if (interactive) printf("Welcome to my shell!\n");

    int last_status = 0;
    int first = 1;

    while (1) {
        if (interactive) {
            if (!first) print_status(last_status); /* §1: status before prompt */
            print_prompt();                         /* §1: cwd prompt */
        }
        first = 0;

        char line[BUF_SIZE];
        if (readline_fd(fd, line, sizeof(line)) < 0) break; /* EOF */

        char *tokens[MAX_TOKENS];
        int ntok = tokenize(line, tokens, MAX_TOKENS);
        if (ntok == 0) { last_status = 0; continue; } /* empty/comment line */

        Cmd cmds[MAX_TOKENS];
        int ncmds = parse_tokens(tokens, ntok, cmds, MAX_TOKENS);
        if (ncmds < 0) { last_status = 1; continue; } /* syntax error */

        /* check if any segment is "exit" */
        int has_exit = 0;
        for (int i = 0; i < ncmds; i++)
            if (cmds[i].argv[0] && strcmp(cmds[i].argv[0], "exit") == 0)
                has_exit = 1;

        int result = execute(cmds, ncmds, interactive, fd);
        if (result == -999 || has_exit) break; /* §2.2: exit command */

        last_status = result;
    }

    if (interactive) printf("Exiting my shell.\n"); /* §1: goodbye */
    if (fd != STDIN_FILENO) close(fd);
    return EXIT_SUCCESS; /* §2.1: mysh always exits EXIT_SUCCESS */
}