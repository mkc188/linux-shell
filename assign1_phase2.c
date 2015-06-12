/*  CENG3150 Assignment 1 Phase 2
    14 Oct 2013  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <glob.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SHELL_NAME "3150 shell"
#define MAX 255

char *types[] = {"", "Built-in Command", "Command Name", "Pipe",
                "Redirect Input", "Redirect Output", "Redirect Output",
                "Input Filename", "Output Filename", "Output Filename",
                "Argument", "Argument"};

enum {EMPTY, BUILT_IN, NAME, ARG, PIPE, R_IN, R_OUT, APPEND};

typedef struct unitcommand {
    int flag_builtin;
    int flag_in;
    int flag_out;
    int flag_append;
    int argc;
    int pid;
    int in;
    int out;
    char in_filename[MAX];
    char out_filename[MAX];
    char *arg[MAX];
    struct unitcommand *next;
} Unitcommand;

typedef struct joblist {
    int jobid;
    char arg[MAX];
    Unitcommand *head;
    struct joblist *next;
} Joblist;

static Joblist *list = NULL;
static int job_number = 0;

void add_job(Unitcommand *current, char tokens[][MAX])
{
    int i;
    Joblist *ptr = list;
    while (ptr->next != NULL)
        ptr = ptr->next;

    ptr->head = current;
    ptr->jobid = ++job_number;

    for (i = 0; tokens[i+1][0] != '\0'; i++)
        sprintf(ptr->arg+strlen(ptr->arg), "%s ", tokens[i]);
    sprintf(ptr->arg+strlen(ptr->arg), "%s", tokens[i]);

    ptr->next = (Joblist *)malloc(sizeof(Joblist));
    ptr = ptr->next;
    *ptr = (Joblist){0, {0}, NULL, NULL};
}

void del_job(int jobid)
{
    int i;
    Joblist *ptr = list;
    Joblist *tmp;
    Joblist *head = (Joblist *)malloc(sizeof(Joblist));
    Joblist *tmp_head = head;
    Unitcommand *tmp_cmd;
    head->next = ptr;
    while (head->next != NULL) {
        if (head->next->jobid == jobid) {
            tmp = head->next;

            if (head->next->next != NULL)
                head->next = head->next->next;
            else
                head->next = NULL;
        }
        head = head->next;
    }

    list = tmp_head->next;

    /* Clean up */
    while (tmp->head != NULL) {
        for (i = 0; i < tmp->head->argc; i++) {
            free(tmp->head->arg[i]);
        }
        tmp_cmd = tmp->head;
        tmp->head = tmp->head->next;
        free(tmp_cmd);
    }
    free(tmp);
}

void signal_handler(int sig) {
    printf("\n");
}

void signal_toggle(int action)
{
    if (action) {
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGQUIT, signal_handler);
        signal(SIGTSTP, signal_handler);
    }
    else {
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
    }
}

int checktype(char token[])
{
    int i = strcspn(token, "\t><|*!`\'\"");
    int j = strcspn(token, "\t><|!`\'\"");
    if (token[0] == '\0')
        return EMPTY;
    else if (strcmp(token, "cd") == 0 || strcmp(token, "exit") == 0 ||
             strcmp(token, "fg") == 0 || strcmp(token, "jobs") == 0)
        return BUILT_IN;
    else if (i == strlen(token))
        return NAME;
    else if (j == strlen(token))
        return ARG;
    else if (strcmp(token, "|") == 0)
        return PIPE;
    else if (strcmp(token, "<") == 0)
        return R_IN;
    else if (strcmp(token, ">") == 0)
        return R_OUT;
    else if (strcmp(token, ">>") == 0)
        return APPEND;
    else
        return -1;
}

void execute_cmd(Unitcommand *queue) {
    int in_fd, out_fd, p;
    p = fork();
    if (p == 0) {
        signal_toggle(0);
        if (queue->flag_in) {
            in_fd = open(queue->in_filename, O_RDONLY);
            if (errno == EACCES) {
                fprintf(stderr, "[%s]: permission denied\n", queue->in_filename);
                exit(-1);
            }
            else if (in_fd == -1) {
                fprintf(stderr, "[%s]: no such file or directory\n", queue->in_filename);
                exit(-1);
            }
            if (dup2(in_fd, 0) == -1) {
                fprintf(stderr, "dup2(): unknown error\n");
            }
            if (close(in_fd) == -1) {
                fprintf(stderr, "close(): unknown error\n");
            }
        }
        if (queue->flag_out || queue->flag_append) {
            if (queue->flag_append)
                out_fd = open(queue->out_filename, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU);
            else
                out_fd = open(queue->out_filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
            if (out_fd == -1) {
                fprintf(stderr, "[%s]: permission denied\n", queue->out_filename);
                exit(-1);
            }
            if (dup2(out_fd, 1) == -1) {
                fprintf(stderr, "dup2(): unknown error\n");
            }
            if (close(out_fd) == -1) {
                fprintf(stderr, "close(): unknown error\n");
            }
        }

        if (queue->in) {
            if (dup2(queue->in, 0) == -1) {
                fprintf(stderr, "dup2(): unknown error\n");
            }
            close(queue->in);
        }
        if (queue->out) {
            if (dup2(queue->out, 1) == -1) {
                fprintf(stderr, "dup2(): unknown error\n");
            }
            close(queue->out);
        }

        /* Check for relative path */
        if (queue->arg[0][0] != '/' && queue->arg[0][0] != '.')
            setenv("PATH", "/bin:/usr/bin", 1);

        if (execvp(queue->arg[0], queue->arg)) {
            if (errno == ENOENT)
                fprintf(stderr, "[%s]: command not found\n", queue->arg[0]);
            else
                fprintf(stderr, "[%s]: unknown error\n", queue->arg[0]);
            exit(-1);
        }
    }
    else if (p == -1) {
        fprintf(stderr, "fork(): unknown error\n");
    }

    /* Close parent's file descriptor */
    if (queue->in) {
        close(queue->in);
    }
    if (queue->out) {
        close(queue->out);
    }

    queue->pid = p;
}

void execute_builtin(Unitcommand *queue)
{
    if (strcmp(queue->arg[0], "cd") == 0) {
        if (queue->argc != 2)
            fprintf(stderr, "cd: wrong number of arguments\n");
        else if (chdir(queue->arg[1]) == -1)
            fprintf(stderr, "[%s]: cannot change directory\n", queue->arg[1]);
    }
    else if (strcmp(queue->arg[0], "exit") == 0) {
        if (queue->argc != 1)
            fprintf(stderr, "exit: too many arguments\n");
        else if (list->next != NULL)
            fprintf(stderr, "There is at least one suspended job\n");
        else
            exit(0);
    }
    else if (strcmp(queue->arg[0], "fg") == 0) {
        int status, found = 0;
        if (queue->argc != 2)
            fprintf(stderr, "fg: wrong number of arguments\n");
        else {
            Joblist *ptr = list;
            if (ptr->next != NULL) {
                do {
                    if (ptr->jobid == atoi(queue->arg[1])) {
                        Unitcommand *cmd_ptr = ptr->head;
                        while (cmd_ptr != NULL) {
                            printf("Job wake up: %s\n", ptr->arg);
                            kill(cmd_ptr->pid, SIGCONT);
                            waitpid(cmd_ptr->pid, &status, WUNTRACED);
                            cmd_ptr = cmd_ptr->next;
                            found = 1;
                        }
                        break;
                    }
                    ptr = ptr->next;
                } while (ptr != NULL);

                if (!found)
                    printf("fg: no such job\n");
                else if (!WIFSTOPPED(status))
                    del_job(ptr->jobid);
                return;
            }

            printf("fg: no such job\n");
        }
    }
    else if (strcmp(queue->arg[0], "jobs") == 0) {
        if (queue->argc != 1) {
            fprintf(stderr, "jobs: too many arguments\n");
        }
        else {
            Joblist *ptr = list;
            if (ptr->next != NULL) {
                do {
                    printf("[%d] %s\n", ptr->jobid, ptr->arg);
                    ptr = ptr->next;
                } while (ptr->next != NULL);
            }
            else {
                printf("No suspended jobs\n");
            }
        }
    }
}

void printresult(char tokens[][MAX], int result[])
{
    int i = 0, j, k, l, width, status, ret, pipe_count = 0, fd[2];
    glob_t *globbuf = (glob_t *)malloc(sizeof(glob_t));
    Unitcommand *queue = (Unitcommand *)malloc(sizeof(Unitcommand));
    *queue = (Unitcommand){0, 0, 0, 0, 0, 0, 0, 0, {0}, {0}, {0}, NULL};
    Unitcommand *head = queue;

    while (1) {
        switch (result[i]) {
            case 0:
            /* Execute command */
            queue = head;
            if (queue->flag_builtin) {
                execute_builtin(queue);
                return;
            }
            else if (pipe_count) {
                for (l = 0; l < pipe_count; l++) {
                    if (pipe(fd) == -1) {
                        fprintf(stderr, "pipe(): unknown error\n");
                    }
                    queue->out = fd[1];
                    execute_cmd(queue);
                    queue = queue->next;
                    if (queue != NULL)
                        queue->in = fd[0];
                }
            }
            execute_cmd(queue);

            queue = head;
            for (l = 0; l <= pipe_count; l++) {
                waitpid(queue->pid, &status, WUNTRACED);
                queue = queue->next;
            }

            if (WIFSTOPPED(status)) {
                add_job(head, tokens);
                return;
            }

            /* Clean up */
            while (head != NULL) {
                for (l = 0; l < head->argc; l++) {
                    free(head->arg[l]);
                }
                queue = head;
                head = head->next;
                free(queue);
            }
            free(globbuf);
            return;
            case 1:
            j = 0;
            do {
                width = strlen(tokens[i]) + 1;
                queue->arg[j] = malloc(width);
                queue->argc++;
                memcpy(queue->arg[j++], tokens[i++], width);
            } while (result[i] == 10);
            queue->arg[j] = NULL;
            queue->flag_builtin = 1;
            break;
            case 2:
            /* Copy command name and arguments */
            j = 0;
            globbuf->gl_offs = 0;
            do {
                if (result[i] == 12) {
                    if (j)
                        ret = glob(tokens[i++], GLOB_DOOFFS|GLOB_APPEND, NULL, globbuf);
                    else
                        ret = glob(tokens[i++], GLOB_DOOFFS, NULL, globbuf);

                    /* Error handling */
                    if (ret == GLOB_NOMATCH) {
                        i--;
                        result[i] = 11;
                    }
                    else {
                        for (k = 0; k < globbuf->gl_pathc; k++) {
                            width = strlen(globbuf->gl_pathv[k]) + 1;
                            queue->arg[j] = malloc(width);
                            queue->argc++;
                            memcpy(queue->arg[j++], globbuf->gl_pathv[k], width);
                        }
                    }
                }
                else {
                    width = strlen(tokens[i]) + 1;
                    queue->arg[j] = malloc(width);
                    queue->argc++;
                    memcpy(queue->arg[j++], tokens[i++], width);
                }
            } while (result[i] == 11 || result[i] == 12);
            queue->arg[j] = NULL;
            break;
            case 3:
            queue->next = (Unitcommand *)malloc(sizeof(Unitcommand));
            *(queue->next) = (Unitcommand){0, 0, 0, 0, 0, 0, 0, 0, {0}, {0}, {0}, NULL};
            queue = queue->next;
            pipe_count++;
            case 4:
            case 5:
            case 6:
            i++;
            break;
            case 7:
            strcpy(queue->in_filename, tokens[i++]);
            queue->flag_in = 1;
            break;
            case 8:
            strcpy(queue->out_filename, tokens[i++]);
            queue->flag_out = 1;
            break;
            case 9:
            strcpy(queue->out_filename, tokens[i++]);
            queue->flag_append = 1;
            break;
        }
    }
}

void checkgrammar(char tokens[][MAX])
{
    int i = 0, state = 0, type;
    int result[MAX] = {0};
    while (1) {
        type = checktype(tokens[i]);
        switch (state) {
            case 0:
            if (type == BUILT_IN)
                state = result[i++] = 1;
            else if (type == NAME)
                state = result[i++] = 2;
            else if (type == EMPTY)
                state = 9;
            else
                state = -1;
            break;
            case 1:
            if (type == NAME || type == ARG)
                state = result[i++] = 10;
            else if (type == EMPTY)
                state = 9;
            else
                state = -1;
            break;
            case 2:
            if (type == NAME)
                state = result[i++] = 11;
            else if (type == ARG)
                state = result[i++] = 12;
            else if (type == PIPE)
                state = result[i++] = 3;
            else if (type == EMPTY)
                state = 9;
            else if (type == R_IN)
                state = result[i++] = 4;
            else if (type == R_OUT)
                state = result[i++] = 5;
            else if (type == APPEND)
                state = result[i++] = 6;
            else
                state = -1;
            break;
            case 3:
            if (type == NAME)
                state = result[i++] = 2;
            else
                state = -1;
            break;
            case 4:
            if (type == NAME || type == BUILT_IN)
                state = result[i++] = 7;
            else
                state = -1;
            break;
            case 5:
            if (type == NAME || type == BUILT_IN)
                state = result[i++] = 8;
            else
                state = -1;
            break;
            case 6:
            if (type == NAME || type == BUILT_IN)
                state = result[i++] = 9;
            else
                state = -1;
            break;
            case 7:
            if (type == EMPTY)
                state = 9;
            else if (type == PIPE)
                state = result[i++] = 3;
            else if (type == R_OUT)
                state = result[i++] = 5;
            else if (type == APPEND)
                state = result[i++] = 6;
            else
                state = -1;
            break;
            case 8:
            if (type == R_IN)
                state = result[i++] = 4;
            else if (type == EMPTY)
                state = 9;
            else
                state = -1;
            break;
            case 9:
            printresult(tokens, result);
            return;
            case 10:
            if (type == NAME || type == ARG)
                state = result[i++] = 10;
            else if (type == EMPTY)
                state = 9;
            else
                state = -1;
            break;
            case 11:
            case 12:
            if (type == NAME)
                state = result[i++] = 11;
            else if (type == ARG)
                state = result[i++] = 12;
            else if (type == PIPE)
                state = result[i++] = 3;
            else if (type == EMPTY)
                state = 9;
            else if (type == R_IN)
                state = result[i++] = 4;
            else if (type == R_OUT)
                state = result[i++] = 5;
            else if (type == APPEND)
                state = result[i++] = 6;
            else
                state = -1;
            break;
            case -1:
            printf("Error: invalid input command line\n");
            return;
        }
    }
}

void tokenize(char tokens[][MAX], char input[])
{
    int count = 0;
    char tmp[MAX], *retval;
    strcpy(tmp, input);
    if ((retval = strtok(tmp, " \n"))) {
        strcpy(tokens[count++], retval);
        while ((retval = strtok(NULL, " \n")))
            strcpy(tokens[count++], retval);
    }

    if (count)
        checkgrammar(tokens);
}

int main(int argc, char **argv)
{
    char input[MAX], my_cwd[MAX], tokens[MAX][MAX];

    /* Initialize job list */
    list = (Joblist *)malloc(sizeof(Joblist));
    *list = (Joblist){0, {0}, NULL, NULL};

    while (1) {
        signal_toggle(1);
        printf("[%s:%s]$ ", SHELL_NAME, getcwd(my_cwd, MAX));
        if (fgets(input, MAX, stdin) == NULL || feof(stdin)) {
            printf("\n");
            exit(0);
        }
        else {
            tokenize(tokens, input);
        }
        memset(tokens, 0, MAX*MAX);
    }
    return 0;
}
