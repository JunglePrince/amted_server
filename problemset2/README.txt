Eric Zeng 0450860 ericzeng@cs.washington.edu
Nicholas Shahan 1266066 nshahan@cs.washington.edu

Prerequisites:
Install Go - https://golang.org/dl/

How to start a LockService cluster:
  $ cd src/main
  $ go run server.go <IP:port> <my index>

  Each node in a LockService cluster needs to be started in their own process.
  They can be on the same machine or across a network.

  The command line arguments to the LockService server are the IP/port of all
  of the nodes in the cluster. The last argument is the index of the ip and
  port of the local node.

  If servers are running on localhost, it is sufficient to provide just a colon
  and then the port. Example: ':3000'

Example LockService cluster deployments
  All nodes on single machine:
    - Machine 1 -
    $ go run server.go :8000 :8001 :8002 0
    $ go run server.go :8000 :8001 :8002 1
    $ go run server.go :8000 :8001 :8002 2

  Nodes all on different machines:
    - Machine 1 -
    $ go run server.go :8000 172.42.22.1:8001 132.44.199.36:8002 0
    - Machine 2 -
    $ go run server.go 99.244.2.74:8000 :8001 132.44.199.36:8002 1
    - Machine 3 -
    $ go run server.go 99.244.2.74:8000 172.42.22.1:8001 :8002 2


How to start a LockService client:
  $ cd src/main
  $ go run client.go <server IP:port>
