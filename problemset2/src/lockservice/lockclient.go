package lockservice

import "math/rand"
import "time"

type LockClient struct {
	server   string
	ClientId int
}

func MakeLockClient(server string) *LockClient {
	lc := new(LockClient)
	lc.server = server
	rand.Seed(time.Now().UTC().UnixNano())
	lc.ClientId = rand.Int()
	return lc
}

func (lc *LockClient) Lock(lockId int) Err {
	args := LockArgs{lc.ClientId, lockId}
	var reply LockReply

	ok := call(lc.server, "LockService.Lock", &args, &reply)

	if !ok {
		return ConnectionFailure
	}

	return reply.Err
}

func (lc *LockClient) Unlock(lockId int) Err {
	args := UnlockArgs{lc.ClientId, lockId}
	var reply UnlockReply

	ok := call(lc.server, "LockService.Unlock", &args, &reply)

	if !ok {
		return ConnectionFailure
	}

	return reply.Err
}
