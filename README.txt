Description: basic custom shell written in C using multiple threads

Three commands: exit, cd, and status.
The shell also supports commenting by '#'.
General syntax of command line is:

command [arg1 arg2 ...] [< input_file] [> output_file] [&] (square brackets optional)


Compile with:

gcc -o smallsh smallsh.c 

