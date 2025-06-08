#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024
#define HISTORY_SIZE 10
#define MAX_ARGS 64
#define MAX_PIPES 10

// gcc -Wall -O2 -o shell shell.c

char *history[HISTORY_SIZE];
int history_count = 0;

// Signal handler for SIGINT (Ctrl+C)
void handle_signal(int sig) {
    if (sig == SIGINT) {
        printf("\nmyshell> ");
        fflush(stdout);
    }
}

// Function to add a command to history
void add_to_history(char *command) {
    if (history_count < HISTORY_SIZE) {
        history[history_count++] = strdup(command);
    } else {
        free(history[0]);
        for (int i = 1; i < HISTORY_SIZE; i++) {
            history[i - 1] = history[i];
        }
        history[HISTORY_SIZE - 1] = strdup(command);
    }
}

// Function to print command history
void print_history() {
    for (int i = 0; i < history_count; i++) {
        printf("%d %s\n", i + 1, history[i]);
    }
}

// Function to read input
char *read_input() {
    char *input = malloc(BUFFER_SIZE * sizeof(char));
    if (!input) {
        fprintf(stderr, "peri: allocation error\n");
        exit(EXIT_FAILURE);
    }
    fgets(input, BUFFER_SIZE, stdin);
    input[strcspn(input, "\n")] = '\0';  // Remove newline
    return input;
}

// Function to parse input into arguments
char **parse_input(char *input) {
    int bufsize = MAX_ARGS, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "peri: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(input, " \t\r\n\a");
    while (token != NULL) {
        tokens[position++] = token;
        
        if (position >= bufsize) {
            bufsize += MAX_ARGS;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "peri: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        
        token = strtok(NULL, " \t\r\n\a");
    }
    tokens[position] = NULL;
    return tokens;
}

// Function to check if a command exists in PATH
int command_exists(const char *cmd) {
    char *path_env = getenv("PATH");
    if (!path_env) return 0;
    
    char *path = strdup(path_env);
    char *dir = strtok(path, ":");
    
    while (dir != NULL) {
        char full_path[BUFFER_SIZE];
        struct stat st;
        
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
        
        if (stat(full_path, &st) == 0 && (st.st_mode & S_IXUSR)) {
            free(path);
            return 1;
        }
        
        dir = strtok(NULL, ":");
    }
    
    free(path);
    return 0;
}

// Function to parse input for pipes
int parse_pipes(char *input, char *commands[MAX_PIPES]) {
    int i = 0;
    char *token = strtok(input, "|");
    
    while (token != NULL && i < MAX_PIPES) {
        // Trim leading and trailing spaces
        char *trimmed = token;
        while (*trimmed == ' ') trimmed++;
        
        char *end = trimmed + strlen(trimmed) - 1;
        while (end > trimmed && *end == ' ') {
            *end = '\0';
            end--;
        }
        
        commands[i++] = trimmed;
        token = strtok(NULL, "|");
    }
    
    return i;  // Return number of commands
}

// Function to execute built-in 'cd' command
int cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "peri: expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("myshell");
        }
    }
    return 1;
}

// Function to handle input/output redirection
void handle_redirection(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("myshell");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDOUT_FILENO) < 0) {
                perror("peri: dup2 error");
                close(fd);
                exit(EXIT_FAILURE);
            }
            close(fd);
            args[i] = NULL;  // Remove the redirection part from args
        } else if (strcmp(args[i], ">>") == 0) {
            int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) {
                perror("myshell");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDOUT_FILENO) < 0) {
                perror("peri: dup2 error");
                close(fd);
                exit(EXIT_FAILURE);
            }
            close(fd);
            args[i] = NULL;
        } else if (strcmp(args[i], "<") == 0) {
            int fd = open(args[i + 1], O_RDONLY);
            if (fd < 0) {
                perror("myshell");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDIN_FILENO) < 0) {
                perror("peri: dup2 error");
                close(fd);
                exit(EXIT_FAILURE);
            }
            close(fd);
            args[i] = NULL;
        }
    }
}

// Function to execute a single command
void execute_command(char **args, int background) {
    if (args[0] == NULL) return;  // Empty command
    
    // Check if command exists
    if (!command_exists(args[0])) {
        fprintf(stderr, "peri: %s: command not found\n", args[0]);
        return;
    }
    
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        // Child process
        
        // Set up a new process group for the child
        if (setpgid(0, 0) < 0) {
            perror("peri: setpgid failed");
        }
        
        // Reset signal handlers to default for child process
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        
        handle_redirection(args);
        
        if (execvp(args[0], args) == -1) {
            fprintf(stderr, "peri: %s: %s\n", args[0], strerror(errno));
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        perror("peri: fork failed");
    } else {
        // Parent process
        if (!background) {
            // Wait for foreground process
            waitpid(pid, &status, 0);
            
            if (WIFEXITED(status)) {
                int exit_status = WEXITSTATUS(status);
                if (exit_status != 0) {
                    fprintf(stderr, "peri: command exited with status %d\n", exit_status);
                }
            } else if (WIFSIGNALED(status)) {
                fprintf(stderr, "peri: command terminated by signal %d\n", WTERMSIG(status));
            }
        } else {
            printf("[%d] Process running in background\n", pid);
        }
    }
}

// Function to execute a pipeline of commands
void execute_pipeline(char *commands[], int num_commands, int background) {
    if (num_commands == 1) {
        // Single command, no pipes
        char **args = parse_input(commands[0]);
        execute_command(args, background);
        free(args);
        return;
    }
    
    int pipes[MAX_PIPES][2];
    pid_t pids[MAX_PIPES];
    
    // Create all the pipes needed
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("peri: pipe creation failed");
            return;
        }
    }
    
    // Create all child processes
    for (int i = 0; i < num_commands; i++) {
        char **args = parse_input(commands[i]);
        
        pids[i] = fork();
        
        if (pids[i] < 0) {
            perror("peri: fork failed");
            exit(EXIT_FAILURE);
        } else if (pids[i] == 0) {
            // Child process
            
            // Set up a new process group for the child
            if (setpgid(0, 0) < 0) {
                perror("peri: setpgid failed");
            }
            
            // Reset signal handlers to default for child process
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            
            // Set up stdin from previous pipe (if not first command)
            if (i > 0) {
                if (dup2(pipes[i-1][0], STDIN_FILENO) < 0) {
                    perror("peri: dup2 failed");
                    exit(EXIT_FAILURE);
                }
            }
            
            // Set up stdout to next pipe (if not last command)
            if (i < num_commands - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                    perror("peri: dup2 failed");
                    exit(EXIT_FAILURE);
                }
            }
            
            // Close all pipe fds in the child
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            // Handle redirection for the command
            handle_redirection(args);
            
            // Execute the command
            if (execvp(args[0], args) < 0) {
                fprintf(stderr, "peri: %s: %s\n", args[0], strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
        
        free(args);
    }
    
    // Parent process: close all pipe fds
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    
    // Wait for all children to finish if not background
    if (!background) {
        for (int i = 0; i < num_commands; i++) {
            int status;
            waitpid(pids[i], &status, 0);
            
            if (WIFEXITED(status)) {
                int exit_status = WEXITSTATUS(status);
                if (exit_status != 0 && i == num_commands - 1) {
                    fprintf(stderr, "peri: pipeline exited with status %d\n", exit_status);
                }
            } else if (WIFSIGNALED(status)) {
                fprintf(stderr, "peri: pipeline terminated by signal %d\n", WTERMSIG(status));
            }
        }
    } else {
        printf("[%d] Pipeline running in background\n", pids[0]);
    }
}

// Function to execute the command (main entry point)
void execute(char **args, int background) {
    // Convert args back to a single string for pipe parsing
    char command_str[BUFFER_SIZE] = "";
    for (int i = 0; args[i] != NULL; i++) {
        strcat(command_str, args[i]);
        strcat(command_str, " ");
    }
    
    // Parse for pipes
    char *commands[MAX_PIPES];
    char *command_copy = strdup(command_str);
    int num_commands = parse_pipes(command_copy, commands);
    
    // Execute the pipeline
    execute_pipeline(commands, num_commands, background);
    
    free(command_copy);
}

// Function to check if the command should run in the background
int check_background(char **args) {
    int i = 0;
    while (args[i] != NULL) {
        i++;
    }
    if (i > 0 && strcmp(args[i - 1], "&") == 0) {
        args[i - 1] = NULL;  // Remove '&' from the argument list
        return 1;  // Run in background
    }
    return 0;  // Run in foreground
}

int main() {
    char *input;
    char **args;
    int background;

    // Set up signal handling
    signal(SIGINT, handle_signal);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    while (1) {
        // Get current working directory
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("peri: getcwd failed");
            strcpy(cwd, "unknown");
        }

        // Display the prompt
        char *username = getenv("USER");
        if (!username) username = "user";
        printf("%s@peri:%s> ", username, cwd);
        fflush(stdout);

        // Read input
        input = read_input();
        if (input == NULL) {
            printf("\n");
            break;  // EOF (Ctrl+D)
        }
        
        if (strlen(input) == 0) {
            free(input);
            continue;
        }

        // Add command to history
        add_to_history(input);

        // Parse input
        args = parse_input(input);
        if (args[0] == NULL) {
            free(input);
            free(args);
            continue;
        }

        // Handle "exit" command
        if (strcmp(args[0], "exit") == 0) {
            free(input);
            free(args);
            break;
        }

        // Handle "cd" command
        if (strcmp(args[0], "cd") == 0) {
            cd(args);
        }

        // Handle "history" command
        else if (strcmp(args[0], "history") == 0) {
            print_history();
        }

        // Execute other commands
        else {
            background = check_background(args);
            execute(args, background);
        }

        // Free memory
        free(input);
        free(args);
    }

    // Free history
    for (int i = 0; i < history_count; i++) {
        free(history[i]);
    }

    return 0;
}
