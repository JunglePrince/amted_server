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

	fmt.Printf("\nClient %v initialized\n", lc.ClientId)
	fmt.Printf("Available Commands:\n")
	fmt.Printf("  lock <id>\n")
	fmt.Printf("  unlock <id>\n")
	fmt.Printf("  quit\n\n")

	for {
		fmt.Printf("∆ ")
		inputString, readErr := reader.ReadString('\n')
		if readErr != nil {
			return
		}

		inputString = strings.Trim(inputString, "\n")

		if strings.ToLower(inputString) == "quit" {
			return
		}

		inputs := strings.Split(inputString, " ")

		if len(inputs) != 2 {
			fmt.Printf("Not a valid command.\n")
			continue
		}

		command := strings.ToLower(inputs[0])
		lockId, intErr := strconv.Atoi(inputs[1])

		if intErr != nil {
			fmt.Printf("Bad lockId: %v\n", intErr)
		} else if command == "lock" {
			fmt.Printf("%v\n", lc.Lock(lockId))
		} else if command == "unlock" {
			fmt.Printf("%v\n", lc.Unlock(lockId))
		} else {
			fmt.Printf("Not a valid command.\n")
		}
	}
}
