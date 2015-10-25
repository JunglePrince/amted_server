package problemset2

import "net"
import "fmt"
import "net/rpc"
import "os"
import "syscall"
import "encoding/gob"
import "math/rand"
import "strconv"
import "strings"
import "time"

const Debug = 1

func DPrintf(format string, a ...interface{}) (n int, err error) {
	if Debug > 0 {
		fmt.Printf(format, a...)
	}
	return
}

type LockService struct {
	locks    map[int]int // map lock id -> client id (or Unlocked)
	px       *Paxos
	max      int // The highest operation committed locally.
	requests chan Request

	l          net.Listener
	me         int
	dead       bool // for testing
	unreliable bool // for testing
}

type Request struct {
	Op       Op
	Response chan Err
}

const Requeue = "Requeue" // Response in Request struct to block on a lock

// Op Types
const (
	Lock   = "Lock"
	Unlock = "Unlock"
)

type OpType string

type Op struct {
	OpType OpType
	Client int
	Lock   int
}

// Represents an unlocked lock.
const Unlocked = -1

func (ls *LockService) Lock(args *LockArgs, reply *LockReply) error {
	op := Op{Lock, args.Client, args.Lock}
	reply.Err = enqueueRequest(op)
}

func (ls *LockService) Unlock(args *UnlockArgs, reply *UnlockReply) error {
	op := Op{Unlock, args.Client, args.Lock}
	reply.Err = enqueueRequest(op)
}

func (ls *LockService) enqueueRequest(op Op) Err {
	response := make(chan Err)
	request = Request{op, response}
	ls.requests <- request
	return <-response
}

func (ls *LockService) dequeueRequests() {
	for !ls.dead {
		request := <-requests
		err := getAgreement(request.Op)
		if err == Requeue {
			requests <- request
		} else {
			request.Response <- err
		}
	}
}

// Gets Paxos to agree to the given operation. If other nodes have already
// agreed on operations that have not yet been committed, then those are
// committed before the given operation.
// Precondition: ls.mu is locked.
// Returns the Err string, and the previous value if it was a Put operation
// on a key that already existed, or the value for the key if it was a Get
// operation.
func (ls *LockService) getAgreement(myOp Op) Err {
	// Keep trying to propose a paxos instance until it succeeds.
	for {
		instance := ls.max + 1
		ls.px.Start(instance, myOp)

		to := 10 * time.Millisecond
		// Check the Paxos status until it is decided.
		decided, value := ls.px.Status(instance)
		for !decided {
			time.Sleep(to)
			if to < 10*time.Second {
				to *= 2
			}
			decided, value = ls.px.Status(instance)
		}
		op := value.(Op)

		// Operation needs to be done regardless of whether our operation was
		// chosen.
		err := ls.commitOperation(instance, op)

		// Finish when myOp was the one committed.
		if op == myOp {
			return err
		}
	}
	// Should never reach here
	panic("ls.getAgreement(): Unreachable code!")
}

func (ls *LockService) commitOperation(instance int, op Op) Err {
	if instance != ls.max+1 {
		panic(fmt.Sprintf("Committing out of order! Expected: %v, Actual: %v\n", kv.max+1, instance))
	}

	ls.max++

	// Initialize lock if it doesn't exist
	if _, exists := ls.locks[op.Lock]; !exists {
		ls.locks[op.Lock] = Unlocked
	}

	if op.OpType == Lock {
		if ls.locks[op.Lock] != Unlocked {
			return Requeue
		}

		ls.locks[op.Lock] = op.Client
		return OK

	} else if op.OpType == Unlock {
		if ls.locks[op.Lock] == Unlocked {
			return NotLocked
		}

		if ls.locks[op.Lock] != op.Client {
			return NotYourLock
		}

		ls.locks[op.Lock] = Unlocked
		return OK
	}
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

func MakeLockService(servers []string, me int) *LockService {
	ls := new(LockService)
	ls.me = me
	ls.max = -1
	ls.locks = make(map[int]int)
	ls.requests = make(chan Request, 256)

	go dequeueRequests()

	rpc.Register(ls)
	ls.px = MakePaxos(servers, me)
	rpc.HandleHTTP()
	listener, err := net.Listen("tcp", servers[me])
	go http.Serve(listener, nil)

	return ls
}

func main() {
	servers := os.Args[1:]
	me := 0

	LockService.Make(servers, me)
}
