
This is the Skeleton code for a very simple implementation of the
Chat Client and Server, written in C/C++.

See the code in this order - ChatClient.cpp, ChatPacket.h, ChatServer.cpp

This code is mainly for those who are finding it difficult to do UNIX
programming. I have provided you with most of the code for the
Threading, Socket programming, and the select() function.
You need to only implement the chat protocol functionality.

To be fair to all students, those who are writing their own code
(instead of using the skeleton code) will be given extra credit.

I have provided code for --
1. Creating a threaded server
2. Using the select() function in the client
3. Some helper functions to easily read/write from/to packet buffers
4. Some example packet formats

For those using Java, you can still use the C code
as a guideline for doing your Java code.
I will try to upload an example code for creating a Threaded server in
Java, but it will be a very simple code.

To install C++ compiler on Ubuntu --
$ sudo apt-get install g++

To compile the code --
$ g++ -lpthread -o ChatServer ChatServer.cpp
$ g++ -lpthread -o ChatClient ChatClient.cpp

Note that although you can compile the code, it will not do anything
on executing until you implement the protocol!

Regards,
Kartik
