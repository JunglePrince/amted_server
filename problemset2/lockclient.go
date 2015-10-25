package problemset2

import "fmt"
import "net/rpc"
import "math/rand"
import "os"
import "strings"

type LockClient struct {
	server   string
	clientId int
}

func MakeLockClient(server string) *LockClient {
	lc := new(LockClient)
	lc.server = server
	ck.clientId = rand.Int()
	return ck
}

//
// call() sends an RPC to the rpcname handler on server srv
// with arguments args, waits for the reply, and leaves the
// reply in reply. the reply argument should be a pointer
// to a reply structure.
//
// the return value is true if the server responded, and false
// if call() was not able to contact the server. in particular,
// the reply's contents are only valid if call() returned true.
//
// you should assume that call() will time out and return an
// error after a while if it doesn't get a reply from the server.
//
// please use call() to send all RPCs, in client.go and server.go.
// please don't change this function.
//
func call(srv string, rpcname string,
	args interface{}, reply interface{}) bool {
	c, errx := rpc.DialHTTP("tcp", srv)
	if errx != nil {
		return false
	}
	defer c.Close()

	err := c.Call(rpcname, args, reply)
	if err == nil {
		return true
	}

	fmt.Println(err)
	return false
}

func (lc *LockClient) Lock(lockId int) Err {
	args := LockArgs{lc.clientId, lockId}
	var reply LockReply

	ok := call(lc.server, "LockService.Lock", &args, &reply)

	if !ok {
		return ConnectionFailure
	}

	return reply.Err
}

func (lc *LockClient) Unlock(lockId int) Err {
	args := UnlockArgs{lc.clientId, lockId}
	var reply UnlockReply

	ok := call(lc.server, "LockService.Unlock", &args, &reply)

	if !ok {
		return ConnectionFailure
	}

	return reply.Err
}

func main() {
	server := os.Args[1]

	lc := LockClient.MakeLockClient(server)

	var command string

	for command != "quit" {
		scanf("%s %d\n", command, lockId)
		strings.ToLower(command)

		if command == "lock" {
			fmt.Printf(lc.Lock(lockId))
		} else if command == "unlock" {
			fmt.Printf(lc.Unlock(lockId))
		} else {
			fmt.Printf("Bad command.\n")
		}
	}
}
