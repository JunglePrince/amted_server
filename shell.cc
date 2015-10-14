#include <iostream>

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

void printErrno() {
  cout << strerror(errno) << "\n";
}

/**
 * Splits input into a array of tokens on the supplied delimiter.
 */
char** tokenize(char* input, const char* delimiter) {
  size_t argmax = 64;
  char** tokens = (char**) calloc(argmax + 1, sizeof(char*));
  if (tokens == NULL) {
    return NULL;
  }
  char* token = strtok(input, delimiter);
  size_t i = 0;
  while (token != NULL) {
    if (strlen(token) == 0)
      continue;
    tokens[i] = token;
    i++;
    token = strtok(NULL, delimiter);
    if (i == argmax)
      break;
  }
  return tokens;
}

/**
 * Takes a line of shell input and splits it into an array of arrays containing
 * the commands and their command line arguments.
 */
char*** parseCommands(char* input, int* len) {
  char** rawCommands = tokenize(input, "|");
  if (rawCommands == NULL)
    return NULL;

  int count = 0;
  while (rawCommands[count] != NULL)
    count++;

  if (count == 0) {
    free(rawCommands);
    return NULL;
  }

  char*** commands = (char***) calloc(count, sizeof(char**));
  int i = 0;
  while (rawCommands[i] != NULL) {
    char* command = rawCommands[i];
    char** commandArgs = tokenize(command, " ");

    if (commandArgs == NULL)
      continue;
    if (commandArgs[0] == NULL)
      continue;

    commands[i] = commandArgs;
    i++;
  }

  free(rawCommands);
  *len = count;
  return commands;
}

/**
 * Sets up pipes for the preceding command in the array, forks another shell
 * process to run the preceding command, and then execs the command at index.
 */
int runCommands(char*** commands, int index) {
  // If this is the first command, all of the pipes are already set up,
  // and we just need to exec the command.
  if (index == 0) {
    char** argv = commands[index];
    if (execvp(*argv, argv) == -1) {
      printErrno();
      return 0;
    }
  }

  // Create pipes to receive data from the preceding command
  int pipes[2];  // 0 is read, 1 is write
  if (pipe(pipes) == -1) {
    printErrno();
    return -1;
  };

  pid_t pid = fork();

  if (pid == 0) {
    // Child process
    // Set stdout (for the preceding process) to the write end of the pipe.
    dup2(pipes[1], STDOUT_FILENO);
    // Close the read end, since it is not used.
    close(pipes[0]);
    // Run the preceding command
    return runCommands(commands, index - 1);
  } else {
    // Parent process
    // Set stdin to the read end of the pipe. stdout has either been set by
    // the next command in the chain, or is just stdout for the last command.
    dup2(pipes[0], STDIN_FILENO);
    // Close the write end, since it is not used.
    close(pipes[1]);
    char** argv = commands[index];
    if (execvp(*argv, argv) == -1) {
      printErrno();
      return 0;
    }
  }
  return 0;
}

int main(int argc, char* argv[]) {
  cout << "$ ";
  // Read a line from stdin
  char* line = NULL;
  size_t len = 0;
  int read;
  while ((read = getline(&line, &len, stdin)) != -1) {
    line[read - 1] = '\0'; // Remove newline
    int len;
    char*** commands = parseCommands(line, &len);
    if (commands) {
      pid_t pid = fork();
      if (pid == 0) {
        // Child process runs the commands
        if (runCommands(commands, len - 1) == -1)
          return 1;
	else
	  return 0;
   
      } else {
        // Parent process waits for all commands to return
        pid_t finished;
        int err = wait(&finished);

        // Free memory allocated for the commands
        int i;
        for (i = 0; i < len; i++) {
          free(commands[i]);
        }
        free(commands);

        // Exit on error
        if (err == -1) {
          printErrno();
          break;
        }
      }
    }
    cout << "$ ";
  }
  free(line);
}
