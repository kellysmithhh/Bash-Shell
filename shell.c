#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define BUFFSIZE 4096
/** Retrieve the hostname and make sure that this program is not being run on the main odin server.
 * It must be run on one of the vcf cluster nodes (vcf0 - vcf3).
 */
void check()
{
        char hostname[10];
        gethostname(hostname, 9);
        hostname[9] = '\0';
        if (strcmp(hostname, "csci-odin") == 0) {
                fprintf(stderr, "WARNING: TO MINIMIZE THE RISK OF FORK BOMBING THE ODIN SERVER,\nYOU MUST RUN THIS PROGRAM ON ONE OF THE VCF CLUSTER NODES!!!\n");
                exit(EXIT_FAILURE);
        } // if
} // check

/** This function interprets "~" as the home directory in the same way that the shell does. This
 * supports the "cd ~/mydir" command in the project requirements.
 */
char* expand_tilde(char* path) {
    if (path[0] == '~') {
		// Retrieve the user's home directory
        const char* homeDir = getenv("HOME");
        if (homeDir != NULL) {
			// Allocate memory for the expanded path
            size_t len = strlen(homeDir) + strlen(path) - 1;
            char* expanded = malloc(len);
            if (expanded != NULL) {
				// Construct the expanded path by appending the home directory and the remaining path
                strcpy(expanded, homeDir);
                strcat(expanded, path + 1);
                return expanded;
            }
        }
    }
    return path;
}

/**
 * This method is a simple shell program that behaves similarly to the bash shell. This program will
 * continually prompt the user to enter a shell command with its options and arguments. The parent
 * process then forks a new child process to execute the command. The parent process then waits for
 * the child process to terminate before prompting the user to enter the next command. When the
 * command is exit, the program will terminate.
 */

int main()
{
	// Check if the program is running on the correct server
    check();
    setbuf(stdout, NULL);

    // Sets the current working directory to the user home directory upon initial launch of the shell.
    // Uses getenv("HOME") to retrieve the user home directory.
    chdir(getenv("HOME"));

    int n;
    char cmd[BUFFSIZE];

    // inifite loop that repeatedly prompts the user to enter a command
    while (1)
    {
        // Get the current working directory
        char cwd[BUFFSIZE];
        getcwd(cwd, sizeof(cwd));

        // Check if the current working directory contains the user's home directory
        char *home = getenv("HOME");
        if (home != NULL && strstr(cwd, home) == cwd) {
            // Replace the home directory path with ~
            memmove(cwd, "~", 1);
            memmove(cwd + 1, cwd + strlen(home), strlen(cwd) - strlen(home) + 1);
        }

		// Display the shell prompt with the current working directory
        printf("1730sh:%s$ ", cwd);

        n = read(STDIN_FILENO, cmd, BUFFSIZE);

        // if user enters a non-empty command
        if (n > 1)
        {
            cmd[n - 1] = '\0'; // replaces the final '\n' character with '\0' to make a proper C string

            // Parses/tokenizes cmd by space to prepare the
            // command line argument array that is required by execvp().
            // For example, if cmd is "head -n 1 file.txt", then the
            // command line argument array needs to be
            // ["head", "-n", "1", "file.txt", NULL].
            char *args[BUFFSIZE];
            char *token = strtok(cmd, " ");
            int i = 0;

            while (token != NULL)
            {
                args[i++] = expand_tilde(token);
                token = strtok(NULL, " ");
            }

            args[i] = NULL; // Null-terminate the argument array

            // if the command is "exit", quit the program
            if (strcmp(args[0], "exit") == 0)
            {
                exit(EXIT_SUCCESS);
            }

            // Project 3 TODO: else if the command is "cd", then use chdir(2) to
            // to support change directory functionalities
            if (strcmp(args[0], "cd") == 0)
            {
                if (args[1] == NULL)
                {
                    // If no directory is provided, change to the home directory
                    chdir(getenv("HOME"));
                }
                else
                {
                    // Change to the specified directory
                    if (chdir(args[1]) == -1)
                    {
                        perror("chdir");
                    }
                }
                continue; // Skip the fork and execvp for the cd command
            }

            // for all other commands, fork a child process and let
            // the child process execute user-specified command with its options/arguments.
            pid_t pid = fork();

            if (pid == -1)
            {
                perror("fork");
                exit(EXIT_FAILURE);
            }

            if (pid == 0)
            {
                int in_fd = -1;
                int out_fd = -1;
                int append_mode = 0;

                // if the command contains input/output direction operators
                // such as "head -n 1 < input.txt > output.txt", then the command
                // line argument array required by execvp() is
                // ["head", "-n", "1", NULL], while the "< input.txt > output.txt" portion
                // is parsed properly to be used with dup2(2) inside the child process
                for (int j = 0; args[j] != NULL; ++j)
                {
                    if (strcmp(args[j], "<") == 0)
                    {
                        args[j] = NULL; // Null-terminate the argument array before input redirection
                        in_fd = open(args[j + 1], O_RDONLY);
                        // inside the child process, dup2(2) is used to redirect
                        // standard input and output as specified by the user command
                        if (in_fd == -1)
                        {
                            perror("open in_fd");
                            exit(EXIT_FAILURE);
                        }
                        dup2(in_fd, STDIN_FILENO);
                        close(in_fd);
                    }
                    else if (strcmp(args[j], ">") == 0)
                    {
                        args[j] = NULL; // Null-terminate the argument array before output redirection
                        out_fd = open(args[j + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                        // inside the child process, dup2(2) is used to redirect
                        // standard input and output as specified by the user command
                        if (out_fd == -1)
                        {
                            perror("open out_fd");
                            exit(EXIT_FAILURE);
                        }
                        dup2(out_fd, STDOUT_FILENO);
                        close(out_fd);
                    }
                    else if (strcmp(args[j], ">>") == 0)
                    {
                        args[j] = NULL; // Null-terminate the argument array before output redirection (append mode)
                        out_fd = open(args[j + 1], O_WRONLY | O_CREAT | O_APPEND, 0666);
                        // inside the child process, dup2(2) is used to redirect
                        // standard input and output as specified by the user command
                        if (out_fd == -1)
                        {
                            perror("open out_fd");
                            exit(EXIT_FAILURE);
                        }
                        dup2(out_fd, STDOUT_FILENO);
                        close(out_fd);
                        append_mode = 1;
                    }
                }

                if (!in_fd) // If input redirection is not specified, redirect from /dev/null
                {
                    in_fd = open("/dev/null", O_RDONLY);
                    if (in_fd == -1)
                    {
                        perror("open /dev/null");
                        exit(EXIT_FAILURE);
                    }
                    dup2(in_fd, STDIN_FILENO);
                    close(in_fd);
                }

                if (!out_fd && !append_mode) // If output redirection is not specified, redirect to /dev/null
                {
                    out_fd = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    // inside the child process, dup2(2) is used to redirect
                    // standard input and output as specified by the user command
                    if (out_fd == -1)
                    {
                        perror("open /dev/null");
                        exit(EXIT_FAILURE);
                    }
                    dup2(out_fd, STDOUT_FILENO);
                    close(out_fd);
                }

                // inside the child process, invoke execvp().
                // if execvp() returns -1, be sure to use exit(EXIT_FAILURE);
                // to terminate the child process
                if (execvp(args[0], args) == -1)
                {
                    perror("execvp");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                // inside the parent process, wait for the child process to terminate
                if (wait(NULL) == -1)
                {
                    perror("wait");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    return 0;
}
