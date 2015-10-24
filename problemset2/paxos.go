package problemset2

//
// Paxos library, to be included in an application.
// Multiple applications will run, each including
// a Paxos peer.
//
// Manages a sequence of agreed-on values.
// The set of peers is fixed.
// Copes with network failures (partition, msg loss, &c).
// Does not store anything persistently, so cannot handle crash+restart.
//
// The application interface:
//
// px = paxos.Make(peers []string, me string)
// px.Start(seq int, v interface{}) -- start agreement on new instance
// px.Status(seq int) (decided bool, v interface{}) -- get info about an instance
// px.Done(seq int) -- ok to forget all instances <= seq
// px.Max() int -- highest instance seq known, or -1
// px.Min() int -- instances before this seq have been forgotten
//

import "net"
import "net/rpc"
import "log"
import "os"
import "syscall"
import "sync"
import "fmt"
import "math/rand"
import "math"

//import "time"

type Paxos struct {
	mu         sync.Mutex
	l          net.Listener
	dead       bool
	unreliable bool
	rpcCount   int
	peers      []string
	me         int // index into peers[]

	// Your data here.
	instances map[int]*InstanceInfo // map instance -> InstanceInfo
	min       map[string]int        // map peer -> known done value
	majority  int                   // Number of nodes required for a quorum
}

// Per-instance state for prepares/accepts.
type InstanceInfo struct {
	HighestPrepare   int         // The highest prepare number seen (n_p).
	HighestAccept    int         // The highest accept number seen (n_a).
	HighestAcceptVal interface{} // The value corresponding to the highet accept (v_a).
	Decided          bool        // Whether the highest value is the final value.
}

// RPC argument/reply definitions.
type PrepareArgs struct {
	Instance int
	Proposal int // n
}

type PrepareReply struct {
	Err              string
	HighestAccept    int         // n_a
	HighestAcceptVal interface{} // v_a
	DecidedVal       interface{} // Used if this instance has already been decided
	Done             int         // Piggyback done value
}

type AcceptArgs struct {
	Instance int
	Proposal int         // n
	Value    interface{} // v'
}

type AcceptReply struct {
	Err              string
	AcceptedProposal int
	DecidedVal       interface{} // Used if this instance has already been decided
	Done             int         // Piggyback done value
}

type DecidedArgs struct {
	Instance int
	Value    interface{}
}

type DecidedReply struct {
	Done int // piggyback done value
}

const PrepareOk string = "PrepareOk"
const PrepareReject string = "PrepareReject"
const AcceptOk string = "AcceptOk"
const AcceptReject string = "AcceptReject"
const Decided string = "Decided"

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

//
// the application wants paxos to start agreement on
// instance seq, with proposed value v.
// Start() returns right away; the application will
// call Status() to find out if/when agreement
// is reached.
//
func (px *Paxos) Start(seq int, v interface{}) {
	// fmt.Printf("node%v: Start(%v,%v)\n", px.me, seq, v)
	// Can't start agreement on seq if it's already done.
	if seq < px.Min() {
		return
	}
	// Can't start agreement on a number that's already started.
	_, startedInstance := px.instances[seq]
	if !startedInstance {
		px.instances[seq] = &InstanceInfo{-1, -1, nil, false}
	}

	go px.propose(seq, v) // Start agreement on new thread.
}

//
// the application on this machine is done with
// all instances <= seq.
//
// see the comments for Min() for more explanation.
//
func (px *Paxos) Done(seq int) {
	px.min[px.peers[px.me]] = seq
	px.tryForget()
}

// Returns the instance marked as done for me.
func (px *Paxos) getDone() int {
	return px.min[px.peers[px.me]]
}

//
// the application wants to know the
// highest instance sequence known to
// this peer.
//
func (px *Paxos) Max() int {
	max := -1
	for key, _ := range px.instances {
		if key > max {
			max = key
		}
	}
	return max
}

//
// Min() should return one more than the minimum among z_i,
// where z_i is the highest number ever passed
// to Done() on peer i. A peers z_i is -1 if it has
// never called Done().
//
// Paxos is required to have forgotten all information
// about any instances it knows that are < Min().
// The point is to free up memory in long-running
// Paxos-based servers.
//
// Paxos peers need to exchange their highest Done()
// arguments in order to implement Min(). These
// exchanges can be piggybacked on ordinary Paxos
// agreement protocol messages, so it is OK if one
// peers Min does not reflect another Peers Done()
// until after the next instance is agreed to.
//
// The fact that Min() is defined as a minimum over
// *all* Paxos peers means that Min() cannot increase until
// all peers have been heard from. So if a peer is dead
// or unreachable, other peers Min()s will not increase
// even if all reachable peers call Done. The reason for
// this is that when the unreachable peer comes back to
// life, it will need to catch up on instances that it
// missed -- the other peers therefor cannot forget these
// instances.
//
func (px *Paxos) Min() int {
	minDone := math.MaxInt64
	for _, done := range px.min {
		if done < minDone {
			minDone = done
		}
	}
	return minDone + 1
}

// Forget instances that are < Min().
func (px *Paxos) tryForget() {
	px.mu.Lock()
	min := px.Min()
	for instance, _ := range px.instances {
		if instance < min {
			delete(px.instances, instance)
		}
	}
	px.mu.Unlock()
}

//
// the application wants to know whether this
// peer thinks an instance has been decided,
// and if so what the agreed value is. Status()
// should just inspect the local peer state;
// it should not contact other Paxos peers.
//
func (px *Paxos) Status(seq int) (bool, interface{}) {
	px.mu.Lock()
	if seq < px.Min() {
		px.mu.Unlock()
		return false, nil
	}

	instance, startedInstance := px.instances[seq]
	if !startedInstance {
		px.mu.Unlock()
		return false, nil
	}

	decided := instance.Decided
	val := instance.HighestAcceptVal
	px.mu.Unlock()
	return decided, val
}

//
// tell the peer to shut itself down.
// for testing.
// please do not change this function.
//
func (px *Paxos) Kill() {
	px.dead = true
	if px.l != nil {
		px.l.Close()
	}
}

// Propose that v is the value of instance seq.
func (px *Paxos) propose(seq int, v interface{}) {

	// fmt.Printf("node%v: propose(%v,%v)\n", px.me, seq, v)
	instance := px.instances[seq]
	proposal := px.me
	for instance != nil && !instance.Decided && !px.dead {
		prepareQuorum, acceptVal := px.sendPrepares(seq, proposal)

		if !prepareQuorum {
			proposal += len(px.peers)
			continue
		}

		if acceptVal == nil {
			acceptVal = v
		}

		acceptQuorum := px.sendAccepts(seq, proposal, acceptVal)

		if !acceptQuorum {
			proposal += len(px.peers)
			continue
		}

		px.sendDecides(seq, acceptVal)
	}
	px.tryForget()
}

// Send Prepare RPCs to all peers.
// Parameters:
//   seq int      - The instance to propose for.
//   proposal int - The propsal number.
// Return values:
//   bool - true if a quorum of prepare_oks was reached
//   interface{} - the highest accept value received from prepare_ok replies.
func (px *Paxos) sendPrepares(seq int, proposal int) (bool, interface{}) {
	prepareOks := 0 // Number of OKs.

	// Track v_a of highest n_a from prepare oks.
	highestAccept := -1              // n_a
	var highestAcceptVal interface{} // v_a

	// Loop through the peers and send each one a Prepare RPC.
	for _, peer := range px.peers {
		args := PrepareArgs{seq, proposal}
		var reply PrepareReply
		var success bool

		if peer == px.peers[px.me] {
			// Call method directly for the local acceptor.
			reply = PrepareReply{"", 0, nil, nil, 0}
			px.Prepare(&args, &reply)
			success = true
		} else {
			success = call(peer, "Paxos.Prepare", &args, &reply)
		}

		if !success {
			continue
		}

		px.min[peer] = reply.Done // Record the done value.

		if reply.Err == PrepareOk {
			prepareOks++
			if reply.HighestAccept > highestAccept {
				highestAccept = reply.HighestAccept
				highestAcceptVal = reply.HighestAcceptVal
			}
		} else if reply.Err == Decided {
			// If the instance has been decided then broadcast decides to get
			// back up to date.
			px.sendDecides(seq, reply.DecidedVal)
			break
		}
	}
	return prepareOks >= px.majority, highestAcceptVal
}

// Send Accept RPCs to all peers.
// Parameters:
//   seq int               - The instance to seek Accepts for.
//   proposal int          - The proposal to be accepted.
//   acceptVal interface{} - The value to be accepted.
// Returns true if a quorum of AcceptOks was reached.
func (px *Paxos) sendAccepts(seq int, proposal int, acceptVal interface{}) bool {
	acceptOks := 0 // Number of OKs.

	// Loop through all peers and send an accept RPC.
	for _, peer := range px.peers {
		args := AcceptArgs{seq, proposal, acceptVal}
		var reply AcceptReply
		var success bool

		if peer == px.peers[px.me] {
			// Call method directly for the local acceptor.
			reply = AcceptReply{"", 0, nil, 0}
			px.Accept(&args, &reply)
			success = true
		} else {
			success = call(peer, "Paxos.Accept", &args, &reply)
		}
		if !success {
			continue
		}
		px.min[peer] = reply.Done // Record the done value.

		if reply.Err == AcceptOk && reply.AcceptedProposal == proposal {
			acceptOks++
		} else if reply.Err == Decided {
			// If the instance has been decided then broadcast decides to get
			// back up to date.
			px.sendDecides(seq, reply.DecidedVal)
			break
		}
	}
	return acceptOks >= px.majority
}

// Send Decided RPCs to all peers. (Also decides self node)
// Parameters:
//   seq int         - The instance to decide.
//   val interface{} - The decided value.
func (px *Paxos) sendDecides(seq int, val interface{}) {
	for _, peer := range px.peers {
		args := DecidedArgs{seq, val}
		var reply DecidedReply
		var success bool

		if peer == px.peers[px.me] {
			// Call method directly for local learner.
			reply = DecidedReply{0}
			px.Decided(&args, &reply)
			success = true
		} else {
			success = call(peer, "Paxos.Decided", &args, &reply)
		}

		if success {
			px.min[peer] = reply.Done // Record the done value.
		}
	}
}

func (px *Paxos) Prepare(args *PrepareArgs, reply *PrepareReply) error {
	px.mu.Lock()
	reply.Done = px.getDone() // Piggyback the done value.

	// If we are all done with this instance, reject.
	if args.Instance < px.Min() {
		reply.Err = PrepareReject
		px.mu.Unlock()
		return nil
	}

	// Create an InstanceInfo if we haven't encountered this instance.
	instance, hasInstance := px.instances[args.Instance]
	if !hasInstance {
		instance = &InstanceInfo{-1, -1, nil, false}
		px.instances[args.Instance] = instance
	}

	// If the instance has already been decided, return the decided value.
	if instance.Decided {
		reply.Err = Decided
		reply.DecidedVal = instance.HighestAcceptVal
		px.mu.Unlock()
		return nil
	}

	if args.Proposal <= instance.HighestPrepare {
		// The prepare proposal is less than or equal to one I've seen before.
		reply.Err = PrepareReject
		px.mu.Unlock()
		return nil
	}

	// The prepare proposal is the highest one seen.
	instance.HighestPrepare = args.Proposal
	reply.Err = PrepareOk
	reply.HighestAccept = instance.HighestAccept
	reply.HighestAcceptVal = instance.HighestAcceptVal
	px.mu.Unlock()
	return nil
}

func (px *Paxos) Accept(args *AcceptArgs, reply *AcceptReply) error {
	px.mu.Lock()
	reply.Done = px.getDone() // Piggyback the done value.

	// If we are all done with this instance, reject.
	if args.Instance < px.Min() {
		reply.Err = AcceptReject
		px.mu.Unlock()
		return nil
	}

	// Create an InstanceInfo if we haven't encountered this instance.
	instance, hasInstance := px.instances[args.Instance]
	if !hasInstance {
		instance = &InstanceInfo{-1, -1, nil, false}
		px.instances[args.Instance] = instance
	}

	// If the instance has already been decided, return the decided value.
	if instance.Decided {
		reply.Err = Decided
		reply.DecidedVal = instance.HighestAcceptVal
		px.mu.Unlock()
		return nil
	}

	if args.Proposal < instance.HighestPrepare {
		// The accept proposal is lower than one I've seen before.
		reply.Err = AcceptReject
		px.mu.Unlock()
		return nil
	}

	// The accept proposal is the highest one seen.
	instance.HighestPrepare = args.Proposal
	instance.HighestAccept = args.Proposal
	instance.HighestAcceptVal = args.Value
	reply.Err = AcceptOk
	reply.AcceptedProposal = args.Proposal
	px.mu.Unlock()
	return nil
}

func (px *Paxos) Decided(args *DecidedArgs, reply *DecidedReply) error {
	px.mu.Lock()
	reply.Done = px.getDone()

	// If we're all done with the instance then don't bother with deciding
	if args.Instance < px.Min() {
		px.mu.Unlock()
		return nil
	}

	instance, hasInstance := px.instances[args.Instance]
	if !hasInstance {
		instance = &InstanceInfo{-1, -1, nil, false}
		px.instances[args.Instance] = instance
	}

	if instance.Decided {
		px.mu.Unlock()
		return nil
	}

	instance.Decided = true
	instance.HighestAcceptVal = args.Value
	px.mu.Unlock()
	return nil
}

//
// the application wants to create a paxos peer.
// the ports of all the paxos peers (including this one)
// are in peers[]. this servers port is peers[me].
//
func Make(peers []string, me int, rpcs *rpc.Server) *Paxos {
	px := &Paxos{}
	px.peers = peers
	px.me = me

	// Your initialization code here.
	px.instances = make(map[int]*InstanceInfo)

	px.min = make(map[string]int)
	for _, peer := range px.peers {
		px.min[peer] = -1
	}

	px.majority = len(px.peers)/2 + 1

	if rpcs != nil {
		// caller will create socket &c
		rpcs.Register(px)
	} else {
		rpcs = rpc.NewServer()
		rpcs.Register(px)

		// prepare to receive connections from clients.
		// change "unix" to "tcp" to use over a network.
		os.Remove(peers[me]) // only needed for "unix"
		l, e := net.Listen("unix", peers[me])
		if e != nil {
			log.Fatal("listen error: ", e)
		}
		px.l = l

		// please do not change any of the following code,
		// or do anything to subvert it.

		// create a thread to accept RPC connections
		go func() {
			for px.dead == false {
				conn, err := px.l.Accept()
				if err == nil && px.dead == false {
					if px.unreliable && (rand.Int63()%1000) < 100 {
						// discard the request.
						conn.Close()
					} else if px.unreliable && (rand.Int63()%1000) < 200 {
						// process the request but force discard of reply.
						c1 := conn.(*net.UnixConn)
						f, _ := c1.File()
						err := syscall.Shutdown(int(f.Fd()), syscall.SHUT_WR)
						if err != nil {
							// fmt.Printf("shutdown: %v\n", err)
						}
						px.rpcCount++
						go rpcs.ServeConn(conn)
					} else {
						px.rpcCount++
						go rpcs.ServeConn(conn)
					}
				} else if err == nil {
					conn.Close()
				}
				if err != nil && px.dead == false {
					// fmt.Printf("Paxos(%v) accept: %v\n", me, err.Error())
				}
			}
		}()
	}

	return px
}
