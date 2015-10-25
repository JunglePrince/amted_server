package main

import "bufio"
import "os"
import "lockservice"
import "fmt"
import "strings"
import "strconv"

func main() {
	if len(os.Args) <= 1 {
		fmt.Printf("Usage: client.go <ServerIP:Port>\n")
		return
	}
	server := os.Args[1]

	lc := lockservice.MakeLockClient(server)
	reader := bufio.NewReader(os.Stdin)

	fmt.Printf("Client %v initialized\n", lc.ClientId)

	for {
		fmt.Printf("∆ ")
		inputString, readErr := reader.ReadString('\n')
		if readErr != nil {
			return
		}

		inputString = strings.Trim(inputString, "\n")
		inputs := strings.Split(inputString, " ")

		if len(inputs) != 2 {
			fmt.Printf("Bad command.\n")
		}

		command := strings.ToLower(inputs[0])
		lockId, intErr := strconv.Atoi(inputs[1])

		if intErr != nil {
			fmt.Printf("Bad lockId: %v\n", intErr)
		} else if command == "lock" {
			fmt.Printf("%v\n", lc.Lock(lockId))
		} else if command == "unlock" {
			fmt.Printf("%v\n", lc.Unlock(lockId))
		} else if command == "quit" {
			return
		} else {
			fmt.Printf("Bad command.\n")
		}
	}
}
