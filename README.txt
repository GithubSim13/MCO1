CSOPESY Major Output - Process Scheduler and CLI
Group 1 - Section S08

Group Members:
- So, Jazlyn
- Simbillo, Jose Miguel B.
- Arucan, Oliver Aldrin
- Pangan, Vince

GitHub Repository:
https://github.com/GithubSim13/MCO1

Entry Point:
main.cpp (inside the MCO1 project folder)

How to Run:
1. Open MCO1.sln in Visual Studio 2022
2. Make sure config.txt is in the same folder as main.cpp
   (C:\Users\<user>\source\repos\MCO1\MCO1\config.txt)
3. Press F5 or Ctrl+F5 to build and run
4. Alternatively, run the compiled MCO1.exe directly from:
   x64\Debug\MCO1.exe (copy config.txt to the same folder as the .exe)

config.txt Format (space-separated):
   num-cpu <1-128>
   scheduler "fcfs" or "rr"
   quantum-cycles <1-2^32>
   batch-process-freq <1-2^32>
   min-ins <1-2^32>
   max-ins <1-2^32>
   delay-per-exec <0-2^32>

Available Commands (after running initialize):
   initialize        - loads config.txt and starts the scheduler
   exit              - terminates the program
   screen -s <name>  - creates a new process and opens its screen
   screen -r <name>  - reattaches to an existing process screen
   screen -ls        - lists all running and finished processes
   scheduler-start   - begins generating dummy processes
   scheduler-stop    - stops generating dummy processes
   report-util       - prints utilization report and saves to csopesy-log.txt

Inside a Process Screen:
   process-smi       - displays process info and logs
   exit              - returns to main menu
