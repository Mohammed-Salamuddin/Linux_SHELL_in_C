# Linux_SHELL_in_C
Making My own Linux shell (MR_SHELL) in C.

A shell is special user program which provide an interface to user to use operating system services. 
Shell accept human readable commands from user and convert them into something which kernel can understand. 
It is a command language interpreter that execute commands read from input devices such as keyboards or from files. 
The shell gets started when the user logs in or start the terminal.

We all use the built in terminal window in Linux distributions like Ubuntu, Fedora, etc. But how do they actually work?
All Linux operating systems have a terminal window to write in commands. 
But how are they executed properly after they are entered?
Also, how are extra features like keeping the history of commands and showing help handled? 
All of this will make sence once you start writing your own code,keeping mr_shell.c code as reference.

Some of the things which mr_shell does is
1. Command is entered and if length is non-null, keep it in history.
2. Parsing : Parsing is the breaking up of commands into individual words and strings
3. Checking for special characters like pipes, etc is done
4. Checking if built-in commands are asked for.
5. If pipes are present, handling pipes.
6. Executing system commands and libraries by forking a child and calling execvp.
7. Printing current directory name and asking for next input.etc

Have fun implimenting your own shell :)
