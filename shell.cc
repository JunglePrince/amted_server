#include <iostream>

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

// int exec_pipes(char* argvv[][512], int pds[][2], int index, int size) {
//   if (index == size - 1) {
//     close(pds[index-1][0]);
//     dup2(pds[index-1][1], 1);
//     execvp(argvv[size-index-1][0], argvv[size-index-1]);
//     return 0;
//   }
//   if (pipe(pds[index]) == -1) {
//     cerr << "pipe err" << endl;
//     exit(EXIT_FAILURE);
//   }
//   pid_t pID;
//   if ((pID = fork()) == -1) {
//     cerr << "fork err" << endl;
//     exit(EXIT_FAILURE);
//   }
//   if (pID == 0) {
//     exec_pipes(argvv, pds, index+1, size);
//   } else {
//     wait(&pID);
//     close(pds[index][1]);
//     dup2(pds[index][0],0);
//     if (index > 0) {
//       close(pds[index-1][0]);
//       dup2(pds[index-1][1],1);
//     }
//     execvp(argvv[size - index - 1][0], argvv[size - index - 1]);
//   }
//   return 0;
// }

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
char*** parseCommands(char* input) {
  char** rawCommands = tokenize(input, "|");
  if (rawCommands == NULL)
    return NULL;

  int count = 0;
  while (rawCommands[count] != NULL)
    count++;

  if (count == 0)
    return NULL;

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
  return commands;
}

void runCommands(char*** commands) {
  pid_t pid = fork();
  if (pid == 0) {
    // Child process
    char** argv = commands[0];
    int err = execvp(*argv, argv);
    if (err == -1) {
      cout << "execvp() error\n";
      printErrno();
    }
  } else {
    // Parent process
    pid_t finished;
    int err = wait(&finished);
    if (err == -1) {
      cout << "wait() error\n";
      printErrno();
      return;
    }
  }
}

int main(int argc, char* argv[]) {
  cout << "$ ";

  // Read a line from stdin
  char* line = NULL;
  size_t len = 0;
  int read;
  while ((read = getline(&line, &len, stdin)) != -1) {
    line[read - 1] = '\0'; // Remove newline
    char*** commands = parseCommands(line);

    if (commands) {
      // Execute commands
      runCommands(commands);

      // Free allocated memory for commands
      int i = 0;
      while (commands[i]) {
	free(commands[i]);
	i++;
      }
      free(commands);
    }
    cout << "$ ";
  }
  free(line);
}
