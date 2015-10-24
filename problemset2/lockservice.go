package problemset2

import "net"
import "fmt"
import "net/rpc"
import "sync"
import "os"
import "syscall"
import "encoding/gob"
import "math/rand"
import "strconv"
import "strings"
import "time"

type LockService struct {
	locks map[int]int // map lock id -> client id
	px    *Paxos

	l          net.Listener
	me         int
	dead       bool // for testing
	unreliable bool // for testing
}

type Op struct {
	OpType string
	Lock   int
	Client int
}

//
// call() sends an RPC to the rpcname handler on server srv
// with arguments args, waits for the reply, and leaves the
// reply in reply. the reply argument should be a pointer
// to a reply structure.
//
// the return value is true if the server responded, and false
// if call() was not able to contact the server. in particular,
// the replys contents are only valid if call() returned true.
//
// you should assume that call() will time out and return an
// error after a while if it does not get a reply from the server.
//
// please use call() to send all RPCs, in client.go and server.go.
// please do not change this function.
//
func call(srv string, name string, args interface{}, reply interface{}) bool {
	c, err := rpc.Dial("unix", srv)
	if err != nil {
		err1 := err.(*net.OpError)
		if err1.Err != syscall.ENOENT && err1.Err != syscall.ECONNREFUSED {
			// fmt.Printf("paxos Dial() failed: %v\n", err1)
		}
		return false
	}
	defer c.Close()

	err = c.Call(name, args, reply)
	if err == nil {
		return true
	}

	fmt.Println(err)
	return false
}

func Make(servers []string, me int) *LockService {
	ls := new(LockService)
	ls.me = me

	ls.locks = make(map[int]int)

	rpcs := rpc.NewServer()
	rpcs.Register(kv)

	ls.px = paxos.Make(servers, me, rpcs)

	os.Remove(servers[me])
	l, e := net.Listen("unix", servers[me])
	if e != nil {
		log.Fatal("listen error: ", e)
	}
	ls.l = l

	go func() {
		for ls.dead == false {
			conn, err := ls.l.Accept()
			if err == nil && ls.dead == false {
				if ls.unreliable && (rand.Int63()%1000) < 100 {
					// discard the request.
					conn.Close()
				} else if ls.unreliable && (rand.Int63()%1000) < 200 {
					// process the request but force discard of reply.
					c1 := conn.(*net.UnixConn)
					f, _ := c1.File()
					err := syscall.Shutdown(int(f.Fd()), syscall.SHUT_WR)
					if err != nil {
						fmt.Printf("shutdown: %v\n", err)
					}
					go rpcs.ServeConn(conn)
				} else {
					go rpcs.ServeConn(conn)
				}
			} else if err == nil {
				conn.Close()
			}
			if err != nil && ls.dead == false {
				fmt.Printf("LockService(%v) accept: %v\n", me, err.Error())
				ls.kill()
			}
		}
	}()

	return ls
}
