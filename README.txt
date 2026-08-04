CSOPESY Major Output 2 - Multitasking OS with Memory Management
Group 1 - Section S08

Group Members:
- So, Jazlyn
- Simbillo, Jose Miguel B.
- Arucan, Oliver Aldrin
- Pangan, Vince

GitHub Repository:
https://github.com/GithubSim13/MCO1
- GO TO MCO2 Branch for MCO2 output

Entry Point:
main.cpp (inside the MCO1 project folder)

How to Run:
1. Open MCO1.sln in Visual Studio 2022
2. Make sure config.txt is in the same folder as main.cpp
   (C:\Users\<user>\source\repos\MCO1\MCO1\config.txt)
3. Press F5 or Ctrl+F5 to build and run
4. Alternatively, run the compiled MCO1.exe directly from:
   x64\Debug\MCO1.exe (copy config.txt to the same folder as the .exe)
5. The program has also been verified to build cleanly and run correctly
   under Linux/g++ (g++ -std=c++17 -pthread *.cpp -o mco1), for anyone
   who wants to sanity-check behavior outside Visual Studio.

config.txt Format (space-separated):
   num-cpu           <1-128>
   scheduler         "fcfs" or "rr"
   quantum-cycles    <1-2^32> if scheduler is "rr" (unused, so any value
                     including 0 is accepted, if scheduler is "fcfs")
   batch-process-freq <1-2^32>
   min-ins           <1-2^32>
   max-ins           <1-2^32>
   delay-per-exec    <0-2^32>
   max-overall-mem   total RAM in bytes (must be an exact multiple of mem-per-frame)
   mem-per-frame     bytes per physical frame / page
   min-mem-per-proc  power of 2 (>= 2) - lower bound for scheduler-generated process size
   max-mem-per-proc  power of 2 (>= 2) - upper bound for scheduler-generated process size
                     (NOTE: the [64, 65536] power-of-2 range from the spec applies to
                     manually-typed "screen -s"/"screen -c" sizes, not to this config
                     range - a process this small still gets a correctly-sized symbol
                     table, capped at min(32, memSize/2) variables rather than a fixed 32)
All of the above are validated on "initialize" - a misconfigured config.txt
will print a specific error naming the offending field instead of starting
with silently-wrong behavior.

Available Commands (after running initialize):
   initialize            - loads + validates config.txt, boots the memory
                            allocator and scheduler
   exit                  - terminates the program
   screen -s <name> [mem]            - creates a process and opens its screen.
                                        Memory size is OPTIONAL: give a power-of-2
                                        size in [64, 65536] to set it explicitly
                                        (invalid sizes get rejected with "invalid
                                        memory allocation"), or omit it entirely
                                        and the process gets the maximum allowed
                                        size (65536 bytes) - useful for quickly
                                        testing arbitrary READ/WRITE addresses
                                        without doing the math on a small config.
   screen -c <name> [mem] "<instrs>" - creates a process with a user-supplied,
                                        semicolon-separated instruction list
                                        (1-50 instructions) instead of a
                                        randomly generated one. Memory size is
                                        optional here too, same rule as above.
                                        Example (no size - gets 65536 bytes):
     screen -c faulty_process "DECLARE varA 10; DECLARE varB 5; ADD varA varA varB; WRITE 0x500 varA; READ varC 0x500; PRINT("Result: " + varC)"
                                        Example (explicit size):
     screen -c proc1 4096 "DECLARE varA 10; DECLARE varB 5; ADD varA varA varB; WRITE 0x500 varA; READ varC 0x500; PRINT("Result: " + varC)"
     NOTE ON QUOTES: the outer "..." wraps the whole instruction list; a
     PRINT's own "text" quotes nest inside it as plain double-quotes too
     (no backslash-escaping needed) - the parser finds the FIRST and LAST
     quote in the line to know where the instruction list starts/ends, so
     type it exactly as shown above, not with \" escapes.
   screen -r <name>      - reattaches to a still-running process's screen.
                            If the process was shut down for an out-of-bounds
                            memory access, prints the violation diagnostic
                            instead. Not-found/already-finished processes
                            print "Process <name> not found."
   screen -ls            - lists running / finished / memory-violated processes
   scheduler-start        \
   scheduler-test          > all three start batch generation of dummy
   scheduler_start        /  processes (accepted as synonyms)
   scheduler-stop         - stops batch generation (in-flight processes finish)
   report-util            - writes the screen -ls report to csopesy-log.txt
   vmstat                 - total/used/free memory, idle/active/total CPU
                            ticks, cumulative pages paged in/out
   process-smi            - nvidia-smi-style summary: CPU utilization +
                            overall memory usage/utilization + per-process
                            memory footprint

Inside a Process Screen:
   process-smi       - refreshes the process's own info/logs
   exit              - returns to main menu

Memory model notes (see the MO2 spec for full detail):
   - Demand paging: pages are brought into physical frames only on first
     access ("first touch" = zero-filled); a full RAM triggers a global
     FIFO eviction of the oldest resident page across ALL processes.
   - Evicted (dirty) pages are written to csopesy-backing-store.txt, a
     genuinely human-readable text file (metadata line + hex-encoded data
     line per evicted page) - open it in any editor at any time.
   - Every process's first 64 bytes are its "symbol table": up to 32 uint16
     variables (DECLARE / ADD-SUB destinations / READ destinations). Once
     32 are declared, further NEW variable declarations are silently ignored.
   - READ(var, addr) / WRITE(addr, value) address the process's OWN memory
     block (0 = first byte that process owns). An address outside
     [0, allocated size) is a memory access violation: the process is
     shut down immediately and its memory is released.
