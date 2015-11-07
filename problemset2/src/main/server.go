package main

import "fmt"
import "lockservice"
import "os"
import "strconv"

func main() {
	if len(os.Args) <= 2 {
		printUsage()
		return
	}

	servers := os.Args[1 : len(os.Args)-1]
	me, err := strconv.Atoi(os.Args[len(os.Args)-1])

	if me < 0 || me >= len(servers) {
		printUsage()
		fmt.Printf("ERROR: \"me\" must be between 0 and %d.\n", len(servers))
		return
	}

	if err != nil {
		printUsage()
		fmt.Printf("ERROR: Last argument must be an index that identifies this server.\n")
		return
	}

	lockservice.MakeLockService(servers, me)
}

func printUsage() {
	fmt.Printf("Usage: server.go <ServerIP:Port> ... <ServerIP:Port> <Zero based \"me\" index>\n")
}
