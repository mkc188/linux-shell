// CENG3150 Assignment 1 Phase 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SHELL_NAME "3150 shell"
#define MAX 255

char *types[] = {"", "Built-in Command", "Command Name", "Pipe",
                "Redirect Input", "Redirect Output", "Redirect Output",
                "Input Filename", "Output Filename", "", "Argument",
                "Argument"};

enum {EMPTY, BUILT_IN, NAME, ARG, PIPE, R_IN, R_OUT, APPEND};

typedef struct {
    int s;
} State;

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

void changestate(char result[], State *current, int next)
{
    current->s = next;
    strcpy(result, types[next]);
}

void printresult(char tokens[][MAX], char result[][MAX]) {
    int i;
    for (i = 0; tokens[i][0] != '\0'; i++)
        printf("Token %d: \"%s\" (%s)\n", i+1, tokens[i], result[i]);
}

void checkgrammar(char tokens[][MAX])
{
    int i = 0, type;
    char result[MAX][MAX];
    State *current = malloc(sizeof(State));
    current->s = 0;
    while (1) {
        type = checktype(tokens[i]);
        switch (current->s) {
            case 0:
            if (type == BUILT_IN)
                changestate(result[i++], current, 1);
            else if (type == NAME)
                changestate(result[i++], current, 2);
            else if (type == EMPTY)
                current->s = 9;
            else
                current->s = -1;
            break;
            case 1:
            if (type == ARG || type == NAME)
                changestate(result[i++], current, 10);
            else if (type == EMPTY)
                current->s = 9;
            else
                current->s = -1;
            break;
            case 2:
            if (type == ARG || type == NAME)
                changestate(result[i++], current, 11);
            else if (type == PIPE)
                changestate(result[i++], current, 3);
            else if (type == EMPTY)
                current->s = 9;
            else if (type == R_IN)
                changestate(result[i++], current, 4);
            else if (type == R_OUT)
                changestate(result[i++], current, 5);
            else if (type == APPEND)
                changestate(result[i++], current, 6);
            else
                current->s = -1;
            break;
            case 3:
            if (type == NAME)
                changestate(result[i++], current, 2);
            else
                current->s = -1;
            break;
            case 4:
            if (type == NAME || type == BUILT_IN)
                changestate(result[i++], current, 7);
            else
                current->s = -1;
            break;
            case 5:
            if (type == NAME || type == BUILT_IN)
                changestate(result[i++], current, 8);
            else
                current->s = -1;
            break;
            case 6:
            if (type == NAME || type == BUILT_IN)
                changestate(result[i++], current, 8);
            else
                current->s = -1;
            break;
            case 7:
            if (type == EMPTY)
                current->s = 9;
            else if (type == PIPE)
                changestate(result[i++], current, 3);
            else if (type == R_OUT)
                changestate(result[i++], current, 5);
            else if (type == APPEND)
                changestate(result[i++], current, 6);
            else
                current->s = -1;
            break;
            case 8:
            if (type == R_IN)
                changestate(result[i++], current, 4);
            else if (type == EMPTY)
                current->s = 9;
            else
                current->s = -1;
            break;
            case 9:
            printresult(tokens, result);
            free(current);
            return;
            case 10:
            if (type == ARG || type == NAME)
                changestate(result[i++], current, 10);
            else if (type == EMPTY)
                current->s = 9;
            else
                current->s = -1;
            break;
            case 11:
            if (type == ARG || type == NAME)
                changestate(result[i++], current, 11);
            else if (type == PIPE)
                changestate(result[i++], current, 3);
            else if (type == EMPTY)
                current->s = 9;
            else if (type == R_IN)
                changestate(result[i++], current, 4);
            else if (type == R_OUT)
                changestate(result[i++], current, 5);
            else if (type == APPEND)
                changestate(result[i++], current, 6);
            else
                current->s = -1;
            break;
            case -1:
            printf("Error: invalid input command line\n");
            free(current);
            return;
        }
    }
}

int main(int argc, char **argv)
{
    char input[MAX], my_cwd[MAX], tokens[MAX][MAX];
    while (1) {
        printf("[%s:%s]$ ", SHELL_NAME, getcwd(my_cwd, MAX));
        if (fgets(input, MAX, stdin) == NULL || feof(stdin)) {
            printf("\n");
            exit(0);
        }
        else {
            tokenize(tokens, input);
            checkgrammar(tokens);
        }
        memset(tokens, 0, MAX*MAX);
    }
    return 0;
}
