package lockservice

import "math/rand"

type LockClient struct {
	server   string
	clientId int
}

func MakeLockClient(server string) *LockClient {
	lc := new(LockClient)
	lc.server = server
	lc.clientId = rand.Int()
	return lc
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
