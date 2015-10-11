#include <iostream>
#include <list>
#include <string>
#include <utility>

#include <sys/wait.h>

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
 * Splits input into a list of tokens on the supplied separator.
 */
list<string> tokenize(string input, string separator) {
  list<string> tokens;
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
 * Takes a line of shell input and splits it into a list of commands.
 */
list<pair<string, list<string> > > parseCommands(string input) {
  list<string> commands = tokenize(input, "|");
  list<pair<string, list<string> > > output;
  list<string>::iterator it;
  for (it = commands.begin(); it != commands.end(); it++) {
    // Tokenize the command and arguments
    string commandAndArgs = *it;
    list<string> args = tokenize(commandAndArgs, " ");
    // If there is no command, abort
    if (commandAndArgs.empty()) {
      continue;
    }
    // Remove the command from the arglist
    string command = args.front();
    args.pop_front();
    output.push_back(make_pair(command, args));
  }
  return output;
}

int main(int argc, char* argv[]) {
  cout << "$ ";

  string input;
  while (getline(cin, input)) {
    list<pair<string, list<string> > > commands = parseCommands(input);
    list<pair<string, list<string> > >::iterator it;
    for (it = commands.begin(); it != commands.end(); it++) {
      string command = it->first;
      list<string> args = it->second;

      cout << "Command: " << command << "\n";
      cout << "Arguments: ";
      list<string>::iterator argit;
      for (argit = args.begin(); argit != args.end(); argit++) {
        cout << *argit << " ";
      }
      cout << "\n";
    }
    cout << "$ ";
  }
}
