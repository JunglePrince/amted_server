#include <cstring>
#include <iostream>
#include <thread>
#include <unordered_set>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>

#define MAX_PATH 512
#define MAX_FILE_SIZE 2097152

using namespace std;

bool exit_requested = false;

struct ioResult {
  int socket;
  char* buffer;
  ssize_t size;
};

void sighandler(int param) {
  cout << "Caught signal: " << param << ".  Requesting termination." << endl;
  exit_requested = true;
}

void print_usage() {
  cout << endl << "Must provide an IP address and port to listen on:" << endl;
  cout << "\t550server <IP Address> <Port>" << endl << endl;
}

// Creates and binds a server socket given the ip address and port.
// Returns the socket descriptor as an int or -1 in the event of a failure.
int create_socket(char* ip, char* port) {
  int optval;
  int status;
  int serverSocket;
  struct addrinfo hints;
  struct addrinfo *results;
  struct addrinfo *serverInfo;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;        // IPv4
  hints.ai_socktype = SOCK_STREAM;  // TCP

  status = getaddrinfo(ip, port, &hints, &results);
  if (status != 0) {
    cout << "Failure getting ip and port info: " << gai_strerror(status) << endl;
    return -1;
  }

  // results now points to a linked list of 1 or more struct addrinfos.
  for (serverInfo = results; serverInfo != NULL; serverInfo = serverInfo->ai_next) {
    serverSocket = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
    if (serverSocket == -1) {
      cout << "Failure creating server socket: " << strerror(errno) << endl;
      continue;
    }

    status = bind(serverSocket, serverInfo->ai_addr, serverInfo->ai_addrlen);
    if (status == -1) {
      // cleanup the unsuccessful socket
      close(serverSocket);
      freeaddrinfo(results);
      cout << "Failure binding server socket: " << strerror(errno) << endl;
      return -1;
    }

    // Set option to reuse the socket immediately
    optval = 1;
    status = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (status == -1) {
      // cleanup the unsuccessful socket
      close(serverSocket);
      freeaddrinfo(results);
      cout << "Failure setting server socket reuse options: " << strerror(errno) << endl;
      return -1;
    }

    // the socket was successfully setup
    break;
  }

  if (serverInfo == NULL) {
    // sanity check
    cout << "Failure binding server socket: " << strerror(errno) << endl;;
    return -1;
  }

  freeaddrinfo(results);
  return serverSocket;
}

// Sets the mode for the provided socket to be non-blocking.
// Returns: 0 on success, or -1 in the event of a failure.
static int make_socket_non_blocking(int socket) {
  int flags;
  int status;

  // read the socket config flags
  flags = fcntl(socket, F_GETFL, 0);
  if (flags == -1) {
    return -1;
  }

  // update and set the config flags
  flags |= O_NONBLOCK;
  status = fcntl(socket, F_SETFL, flags);
  if (status == -1) {
    return -1;
  }

  // success
  return 0;
}

// Reads the named file from disk into a buffer. Writes the client socket,
// buffer pointer, and file size to the provided pipe.
// In the event of an error reading the file the pointer written to the buffer
// will be null.
void performDiskIo(int clientSocket, char* fileName, int diskIoPipe) {
  FILE* fp;
  size_t fileSize = 0;
  size_t bytesRead;
  char* fileBuffer;
  ioResult* result;

  fp = fopen(fileName, "rb");
  if (fp == NULL) {
    // error opening file, need to signal error to close the client socket.
    cout << "Failure opening requested file: " << fileName << strerror(errno) << endl;
    // null buffer pointer represents an error reading the file.
    fileBuffer = NULL;
  } else {
    // find the file size
    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);
    rewind(fp);

    fileBuffer = (char*) malloc(sizeof(char) * fileSize);
    if (fileBuffer == NULL) {
      cout << "Failure allocating fileBuffer for requested file: " << fileName << strerror(errno) << endl;
    } else {
      // copy the file into the buffer:
      bytesRead = fread(fileBuffer, 1, fileSize, fp);
      if (bytesRead != fileSize) {
        free(fileBuffer);
        cout << "Failure reading requested file: " << fileName << endl;
        // null buffer pointer represents an error reading the file.
        fileBuffer == NULL;
      }
    }

    // done with file
    fclose(fp);
  }

  // write socket, buffer pointer, and buffer size to the pipe to signal completion
  // A null buffer implies a failure reading the file.
  result = (ioResult*) malloc(sizeof(ioResult));
  result->socket = clientSocket;
  result->buffer = fileBuffer;
  result->size = fileSize;

  write(diskIoPipe, result, sizeof(ioResult));

  free(fileName);
  free(result);

  // close the write end of the pipe
  close(diskIoPipe);
}

void read_request(int sock, int epoll) {
  char* fileName = (char*) malloc(MAX_PATH);
  ssize_t bytesRead = read(sock, fileName, MAX_PATH);
  int status;
  struct epoll_event event;
  if (bytesRead == 0) {
      // other side closed the connection, nothing to read
      cout << "Failure nothing read from client socket" << endl;
      free(fileName);
      close(sock);
      return;
    }
    if (bytesRead == -1) {
      cout << "Failure reading from client socket: " << strerror(errno) << endl;
      free(fileName);
      close(sock);
      return;
    }

    // ensure the file is null terminated
    fileName[bytesRead-1] = '\0';

    // create a pipe to communicate with the other thread
    int diskIoPipes[2];
    status = pipe(diskIoPipes);
    if (status == -1) {
      cout << "Failure creating pipes: " << strerror(errno) << endl;
      free(fileName);
      close(sock);
      return;
    }

    // add this side of the pipe to the event queue
    event.data.fd = diskIoPipes[0];
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    status = epoll_ctl(epoll, EPOLL_CTL_ADD, diskIoPipes[0], &event);
    if (status == -1) {
      cout << "Failure adding pipe to event queue: " << strerror(errno) << endl;
      free(fileName);
      close(sock);
      return;
    }

    // create a thread to handle the read request
    thread diskIoThread(performDiskIo, sock, fileName, diskIoPipes[1]);
    diskIoThread.detach();
}

// Reads results form the provided pipe. Marshals the client socket, buffer
// pointer, and file size. Copies the file to the client socket.
void write_response(int diskIoPipe, int epoll) {
  ioResult result;
  ssize_t bytesWritten;
  ssize_t bytesRead = read(diskIoPipe, &result, sizeof(ioResult));
  if (bytesRead != sizeof(ioResult)) {
    cout << "Failure reading file from pipe." << endl;
  }

  // remove the pipe from the epoll
  struct epoll_event event; // can be null in newer kernel versions
  int status = epoll_ctl(epoll, EPOLL_CTL_DEL, diskIoPipe, &event);
  if (status == -1) {
    cout << "Failure removing pipes from the event queue: " << errno << endl;
  }

  // close the pipe
  close(diskIoPipe);

  // write to the client socket and close it
  bytesWritten = write(result.socket, result.buffer, result.size);
  if (bytesWritten != result.size) {
    cout << "Failure writing to client socket." << endl;
  }

  free(result.buffer);
  close(result.socket);
}

// mainline program
int main(int argc, char* argv[]) {

  if (argc != 3) {
    print_usage();
    exit(EXIT_SUCCESS);
  }

  char* ip = argv[1];
  char* port = argv[2];
  struct epoll_event* events;

  // catch signal to exit
  signal(SIGINT, sighandler);

  cout << "Starting AMTED server..." << endl;
  cout << "Server pid = " << getpid() << endl;

  // TODO port between 1024 and 65535

  int serverSocket = create_socket(ip, port);
  if (serverSocket == -1) {
    cout << "Failure creating a listening socket: " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }

  int status = make_socket_non_blocking(serverSocket);
  if (status == -1) {
    cout << "Failure making the listening socket non-blocking." << endl;
    close(serverSocket);
    exit(EXIT_FAILURE);
  }

  status = listen(serverSocket, SOMAXCONN);
  if (status == -1) {
    cout << "Failure listening to socket: " << errno << endl;
    close(serverSocket);
    exit(EXIT_FAILURE);
  }

  cout << "Success." << endl << "Listening on: " << ip << ":" << port << endl;

  // create event queue that stores the server socket, client sockets, and pipes
  struct epoll_event event;

  int epoll = epoll_create1(0);
  if (epoll == -1) {
    cout << "Failure creating event queue: " << errno << endl;
    close(serverSocket);
    exit(EXIT_FAILURE);
  }

  // set to track the client sockets
  unordered_set<int> clients = {};

  // add the listening socket to the event queue
  event.data.fd = serverSocket;
  event.events = EPOLLIN;
  status = epoll_ctl(epoll, EPOLL_CTL_ADD, serverSocket, &event);
  if (status == -1) {
    cout << "Failure queuing the listening socket: " << errno << endl;
    close(serverSocket);
    exit(EXIT_FAILURE);
  }

  while (!exit_requested) {
    // get each event one at a time
    // TODO do we need to alloc here?
    events = (epoll_event*) calloc(1, sizeof(epoll_event));

    int n = epoll_wait(epoll, events, 1, -1);
    if (n == 0) {
      // nothing returned from poll for some reason
      continue;
    } else if (n == -1) {
      cout << "Failure polling event queue: " << strerror(errno) << endl;
      close(serverSocket);
      free(events);
      exit(EXIT_FAILURE);
    }

    if (serverSocket == events[0].data.fd) {
      // notification on the server socket for an incoming connection
      struct sockaddr* clientAddr = new struct sockaddr;
      socklen_t clientAddrLen = sizeof(struct sockaddr);

      // accept new client connection
      int clientSocket = accept(serverSocket, clientAddr, &clientAddrLen);
      if (clientSocket == -1) {
        cout << "Failure accepting new connection: " << errno << endl;
        continue;
      }

      status = make_socket_non_blocking(clientSocket);
      if (status == -1) {
        cout << "Failure making client socket non-blocking: " << errno << endl;
        close(clientSocket);
        continue;
      }

      // add the client socket to the event queue
      event.data.fd = clientSocket;
      event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
      status = epoll_ctl(epoll, EPOLL_CTL_ADD, clientSocket, &event);
      if (status == -1) {
        cout << "Failure queuing the client socket: " << errno << endl;
        close(serverSocket);
        close(clientSocket);
        close(epoll);
        free(events);
        exit(EXIT_FAILURE);
      }

      // save this active client socket
      clients.insert(clientSocket);

    } else if (clients.count(events[0].data.fd) > 0) {
      // notification on a client socket, ready to read request
      read_request(events[0].data.fd, epoll);
      clients.erase(events[0].data.fd);

      // take no more requests from this client
      int status = epoll_ctl(epoll, EPOLL_CTL_DEL, events[0].data.fd, &event);
      if (status == -1) {
        cout << "Failure removing client socket from event queue: " << strerror(errno) << endl;
      }

    } else {
      // notification on a pipe, means a disk I/O has completed
      write_response(events[0].data.fd, epoll);
    }

    free(events);
  }

  if (events != NULL) {
    free(events);
  }

  close(serverSocket);
}