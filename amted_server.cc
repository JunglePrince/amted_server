#include <cstring>
#include <iostream>

#include <unistd.h>

using namespace std;

void printUsage() {
  cout << endl <<"Must provide an IP address and port to listen on:" << endl;
  cout << "\t550server <IP Address> <Port>" << endl << endl;

}

void createSocket(char* ip, char* port) {

}


int main(int argc, char* argv[]) {

  if (argc != 3) {
    printUsage();
    exit(EXIT_SUCCESS);
  }

  char* ip = argv[1];
  char* port = argv[2];

  cout << "Starting AMTED server on: " << ip << ":" << port << endl;
  cout << "Server pid = " << getpid() << endl;

  createSocket(ip, port);

}