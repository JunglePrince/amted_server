#include <string>
#include <iostream>
#include <sys/wait.h>
#include <vector>
#include <boost/algorithm/string.hpp>
using namespace std;
using namespace boost;

int exec_pipes(char* argvv[][512], int pds[][2], int index, int size) {
  if (index == size - 1) {
    close(pds[index-1][0]);
    dup2(pds[index-1][1], 1);
    execvp(argvv[size-index-1][0], argvv[size-index-1]);
    return 0;
  }
  if (pipe(pds[index]) == -1) {
    cerr << "pipe err" << endl;
    exit(EXIT_FAILURE);
  }
  pid_t pID;
  if ((pID = fork()) == -1) {
    cerr << "fork err" << endl;
    exit(EXIT_FAILURE);
  }
  if (pID == 0) {
    exec_pipes(argvv, pds, index+1, size);
  } else {
    wait(&pID);
    close(pds[index][1]);
    dup2(pds[index][0],0);
    if (index > 0) {
      close(pds[index-1][0]);
      dup2(pds[index-1][1],1);
    }
    execvp(argvv[size - index - 1][0], argvv[size - index - 1]);
  }
  return 0;
}

int parse_input() {
  string line;
  while(std::getline(std::cin, line)) {
    vector<string> argv;
    split(argv, line, is_any_of("|"));
    int size = argv.size();
    char* argvv[size][512];
    for (int i = 0; i < size; i++) {
      char* line = strdup(argv[i].c_str());
      int j = 0;
      for (argvv[i][j] = strtok(line," "); argvv[i][j] != NULL; argvv[i][++j] = strtok(NULL, " ")) {
        int pds[size-1][2];
        exec_pipes(argvv, pds, 0, size);
        for (int i = 0; i < size; i++) {
          for (int j = 0; j < 512; j++) {
            free(argvv[i][j]);
          }
          free(argvv[i]);
        }
    free(argvv);
  }
  return 0;
}

int main(int argc, char* argv[]) {
  return parse_input();
}
