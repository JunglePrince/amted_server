#include <cstring>
#include <iostream>
#include <thread>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#define NUM_THREADS 16
#define MAX_PATH 512
using namespace std;

bool exit_requested = false;

void print_usage() {
  cout << endl << "Must provide an IP address and port to listen on:" << endl;
  cout << "\t550server <IP Address> <Port>" << endl << endl;
}

void cleanup_and_exit() {
  exit_requested = true;
  // TODO join threads and free memory
  exit(EXIT_FAILURE);
}

void exit_with_error(const char* errorMsg) {
  cerr << endl << errorMsg << endl;
  cleanup_and_exit();
  exit(EXIT_FAILURE);
}

int create_socket(char* ip, char* port) {
  int status;
  struct addrinfo hints;
  struct addrinfo *results;
  struct addrinfo *serverInfo;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;        // IPv4
  hints.ai_socktype = SOCK_STREAM;  // TCP

  status = getaddrinfo(ip, port, &hints, &results);
  if (status != 0) {
    // TODO cleanup this error handling
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }

  // results now points to a linked list of 1 or more struct addrinfos.
  int serverSocket;
  for (serverInfo = results; serverInfo != NULL; serverInfo = serverInfo->ai_next) {
    serverSocket = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
    if (serverSocket == -1) {
      cout << "socket failure: " << errno;
      continue;
    }

    status = bind(serverSocket, serverInfo->ai_addr, serverInfo->ai_addrlen);
      if (status == 0) {
        // successful bind

        // Set option to reuse the socket immediately
        int optval = 1;
        status = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        if (status != 0) {

          // TODO error handling
          cout << "socket reuse options failed" << endl;
          exit(1);
        }
        break;
      } else {
        cout << "whats going on?? " << status << " " << errno << endl;
          exit(1);
      }


    // cleanup the unsuccessful socket
    close (serverSocket);
  }

  if (serverInfo == NULL) {
    // TODO
    // fprintf (stderr, "Could not bind\n");
    // return -1;
    cout << "server socket failure" << endl;
    exit(1);
  }

  // freeaddrinfo(result);

  return serverSocket;
}

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

  return 0;
}

void write_response(int sock) {
  char writeBuf[1024];
  memset(&writeBuf, 'X', 1024);
  write(sock, writeBuf, 1024);
  close(sock);
}

void read_request(int sock, int epoll) {
  char readbuf[MAX_PATH];
  ssize_t status = read(sock, readbuf, MAX_PATH);
  if (status == 0) {
      // other side closed the connection, nothing to read
      close(sock);
      return;
    }
    if ( res == -1 ) {
      // TODO
      cout << "read failure" << endl;
      exit(1);
    }
    // Log the data we just read and then send it back to the other side.
    readbuf[res-1] = '\0';
    cout << readbuf << endl;
    // write(sock, readbuf, strlen(readbuf));

    // take no more requests from this client
    struct epoll_event event; // can be null in newer kernel versions
    int status = epoll_ctl(epoll, EPOLL_CTL_DEL, sock, &event);
    if (status !=0){}
  // }

    write_response(sock);

  // close(sock);
}

int main(int argc, char* argv[]) {

  if (argc != 3) {
    print_usage();
    exit(EXIT_SUCCESS);
  }

  char* ip = argv[1];
  char* port = argv[2];

  cout << "Starting AMTED server..." << endl;
  cout << "Server pid = " << getpid() << endl;

  // TODO port between 1024 and 65535

  int serverSocket = create_socket(ip, port);
  if (serverSocket == -1) {
    cout << "Failure creating a listening socket: " << errno << endl;
    cleanup_and_exit();
  }

  int status = make_socket_non_blocking(serverSocket);
  if (status == -1) {
    cout << "Failure making the listening socket non-blocking." << endl;
    cleanup_and_exit();
  }

  status = listen(serverSocket, SOMAXCONN);
  if (status == -1) {
    cout << "Failure listening to socket: " << errno << endl;
    cleanup_and_exit();
  }

  cout << "Success." << endl << "Listening on: " << ip << ":" << port << endl;

  // create event queue that stores the server socket, client sockets, and pipes
  struct epoll_event event;

  int epoll = epoll_create1(0);
  if (epoll == -1) {
    cout << "Failure creating event queue: " << errno << endl;
    cleanup_and_exit();
  }

  // add the listening socket to the event queue
  event.data.fd = serverSocket;
  event.events = EPOLLIN;
  status = epoll_ctl(epoll, EPOLL_CTL_ADD, serverSocket, &event);
  if (status == -1) {
    cout << "Failure queuing the listening socket: " << errno << endl;
    cleanup_and_exit();
  }

  while (!exit_requested) {
    // get each event one at a time
    struct epoll_event* events = (epoll_event*) calloc(1, sizeof(epoll_event));
    int n = epoll_wait(epoll, events, 1, -1);
    if (n == 0) {
      // nothing returned from poll for some reason
      continue;
    } else if (n == -1) {
      cout << "Failure polling event queue: " << errno << endl;
      cleanup_and_exit();
    }

    if (serverSocket == events[0].data.fd) {
      // notification on the server socket for an incoming connection
      struct sockaddr* clientAddr = new struct sockaddr;
      socklen_t clientAddrLen = sizeof(struct sockaddr);
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
      event.events = EPOLLIN;
      status = epoll_ctl(epoll, EPOLL_CTL_ADD, clientSocket, &event);
      if (status == -1) {
        cout << "Failure queuing the client socket: " << errno << endl;
        cleanup_and_exit();
      }
    } else if (false){
      // notification on a pipe - means a disk I/O has completed
      // TODO
    } else {
      // notification on a client socket
      read_request(events[0].data.fd, epoll);
    }
  }


  // freeaddrinfo(serverInfo);
}