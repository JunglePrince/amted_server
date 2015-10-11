#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <utility>
#include <vector>

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



/**
 * Splits input into a vector of tokens on the supplied separator.
 */
vector<string> tokenize(string input, string separator) {
  vector<string> tokens;
  int prev = 0;
  int next = 0;
  while (prev != string::npos) {
    int position = input.find(separator, next);
    string token = input.substr(next, position - next);
    if (!token.empty())
      tokens.push_back(token);
    prev = position;
    next = position + 1;
  }
  return tokens;
}

/**
 * Takes a line of shell input and splits it into a vector of vectors containing
 * the command and the command line arguments.
 */
vector<vector<string> > parseCommands(string input) {
  vector<string> rawCommands = tokenize(input, "|");
  vector<vector<string> > output;
  vector<string>::iterator it;
  for (it = rawCommands.begin(); it != rawCommands.end(); it++) {
    // Tokenize the command and arguments
    string rawCommand = *it;
    vector<string> command = tokenize(rawCommand, " ");
    // If there is no command, abort
    if (command.empty()) {
      continue;
    }
    output.push_back(command);
  }
  return output;
}

void runCommands(vector<vector<string>> commands) {
  pid_t pid = fork();
  if (pid == 0) {
    // Child process
    vector<string> command = commands.front();
    // Convert arguments into C string array
    char* const args[commands.size()]();
    int i;
    for (i = 0; i < command.size(); ++i) {
      args[i] = command[i].c_str();
    }

    int err = execvp(command.front().c_str(), args);
    if (err == -1) {
      cout << "execvp() error\n";
    }
  } else {
    // Parent process
    pid_t finished;
    int err = wait(&finished);
    if (err == -1) {
      cout << "wait() error\n";
      return;
    }
  }
}

int main(int argc, char* argv[]) {
  cout << "$ ";
  string input;
  while (getline(cin, input)) {
    vector<vector<string> > commands = parseCommands(input);
    runCommands(commands);
    cout << "$ ";
  }
}
