package problemset2

const (
	OK          = "OK"
	NotLocked   = "NotLocked"
	NotYourLock = "NotYourLock"
)

type Err string

type LockArgs struct {
	Client int
	Lock   int
}

type LockReply struct {
	Err Err
}

type UnlockArgs struct {
	Client int
	Lock   int
}

type UnlockReply struct {
	Err Err
}
