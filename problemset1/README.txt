Eric Zeng 0450860 ericzeng@cs.washington.edu
Nicholas Shahan 1266066 nshahan@cs.washington.edu

Part A

Concurrency architecture inspired by:
http://web.cse.ohio-state.edu/~mamrak/CIS762/pipes_lab_notes.html

Valgrind reports a memory leak on termination, and the stack trace
attributes it to the valgrind malloc implementation or the ld shared
library. We don't think that it originated from our code.


Part B

socket programming inspired by:
Beej's Guide to Network Programming
http://www.beej.us/guide/bgnet/output/html/multipage/index.html

epoll programming inspired by:
How to use epoll?
https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/

A memory leak is shown by valgrind, but it does not originate from our code and
we could not find a solution that would resolve it.
