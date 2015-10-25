package main

import "os"
import "lockservice"

func main() {
	servers := os.Args[1:]
	me := 0

	lockservice.MakeLockService(servers, me)
}
