/**
 * @file
 * @brief  Client side of the Chat Application
 *
 * @details  For the client, I have provided code for using the
 *   select() function, and connecting to the server.
 *
 * @author  Kartik S
 */

#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "ChatPacket.h"

/**
 * These are some helper functions to read/write to buffers.
 *
 * The first parameter is a buffer of required size.
 *
 * The second parameter is a byte offset into the buffer. The value
 * is read/written at buffer[offset]. The function
 * automatically increases the offset by the number of bytes you
 * read/write into the buffer.
 *
 * The functions also take care of Host to Network order conversion
 * and vice versa.
 */

/// @brief  Helper function to get the next NULL terminated string in the packet stream
std :: string getNextString ( const char *buffer , int &offset );
/// @brief  Helper function to get the next uint32_t in the packet stream
uint32_t getNextUint32 ( const char *buffer , int &offset );
/// @brief  Helper function to get the next uint16_t in the packet stream
uint16_t getNextUint16 ( const char *buffer , int &offset );

/// @brief  Helper function to put the next NULL terminated string in the packet stream
void putNextString ( char *buffer , int &offset , const std :: string &nextString );
/// @brief  Helper function to put the next uint32_t in the packet stream
void putNextUint32 ( char *buffer , int &offset , uint32_t nextUint32 );
/// @brief  Helper function to put the next uint16_t in the packet stream
void putNextUint16 ( char *buffer , int &offset , uint16_t nextUint16 );

/// @brief  Starting point of the client
int
main ( int argc , char **argv ) {

    /*
     * The std::string data type in C++ is similar to String data
     * type in Java
     *
     * uint16_t is an integer of size 16 bits
     *
     * If you are not familiar with the C++ input/output, please see
     * this simple tutorial --
     * http://www.cplusplus.com/doc/tutorial/basic_io/
     *
     */

    // Get the Server's IP and Port
    std :: string serverIP;
    uint16_t serverPort;
    std :: cout << "=== Welcome to the Chat Client!! === \n";
    std :: cout << "Enter Chat Server IP: ";
    std :: cin >> serverIP;
    std :: cout << "Enter Chat Server Port: ";
    std :: cin >> serverPort;

    // Get User name
    std :: string userName;
    std :: cout << "Enter user name: ";
    std :: cin >> userName;

    // Connect to the server
    // Step 1: Create a socket
    int socketFD;
    if ( ( socketFD = socket ( AF_INET , SOCK_STREAM , 0 ) ) < 0 ) {
        std :: cerr << "Error on socket()\n";
        return -1;
    }

    // Step 2: Set the server IP and Address
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr ( serverIP.c_str() );
    serverAddress.sin_port = htons ( serverPort );

    // Step 3: Connect to the server
    if ( connect ( socketFD , (struct sockaddr*) &serverAddress ,
                   sizeof ( struct sockaddr_in ) ) != 0 ) {
        std :: cerr << "Error on connect(), is the server running?\n";
        return -1;
    }

    // Get Client's IP and Port (assigned by the OS)
    struct sockaddr_in clientAddress;
    socklen_t addressLength = sizeof ( struct sockaddr_in );
    if ( getsockname ( socketFD , (struct sockaddr*) &clientAddress ,
                       &addressLength ) != 0 ) {
        std :: cerr << "Error on getsockname()\n";
        close ( socketFD );
        return -1;
    }

    // Create a buffer we can use to send packets to the server
    char *replyBuffer = new char[ MAX_PACKET_LENGTH ];
    if ( replyBuffer == NULL ) {
        std :: cerr << "Error: Heap over\n";
        close ( socketFD );
        return -1;
    }
    int replyOffset , lengthOffset;

    /*
     * This is an example of how to use the helper functions to
     * create a Login packet (in ChatPacket.h).
     *
     * First, you need to allocate a buffer (shown above).
     * Second, you need to set the offset (replyOffset) to 0 initially.
     * Then, you can write the packet fields one by one as shown.
     * Finally, at the end, the 'replyOffset' would contain the number
     * of bytes you have written into the buffer. So, you can write the
     * 'length' field.
     *
     * Now, you can send the buffer using the send() function.
     *
     * To write another packet, reset the offsets as shown and start again.
     */

    // Send a Login request to the server
    replyOffset = 0 , lengthOffset = LENGTH_FIELD_OFFSET;
    // Request Type
    putNextUint16 ( replyBuffer , replyOffset , REQUEST_LOGIN );
    // Length (we will fill this later on)
    putNextUint16 ( replyBuffer , replyOffset , 0 );
    // Cookie (set as zero on login)
    putNextUint32 ( replyBuffer , replyOffset , 0 );
    // User name
    putNextString ( replyBuffer , replyOffset , userName );
    // Now 'replyOffset' has the number of bytes we put in the buffer, we can now write the length
    putNextUint16 ( replyBuffer , lengthOffset , replyOffset );

    if ( send ( socketFD , replyBuffer , replyOffset , 0 ) < 0 ) {
        std :: cerr << "Error on send()\n";
        close ( socketFD );
        return -1;
    }

    // Wait for a successful response
    // -----------------------------------------------
    // Step 1: First, get the 'type' and 'length' of the packet (first 2 fields are total 4 bytes)
    size_t bufferSize = sizeof ( uint16_t ) + sizeof ( uint16_t );
    char *buffer = new char[ bufferSize ];
    if ( buffer == NULL ) {
        std :: cerr << "Error: Heap Over\n";
        close ( socketFD );
        return -1;
    }
    // The flag MSG_WAITALL ensures that we get the all the 4 bytes in one recv()
    if ( recv ( socketFD , buffer , bufferSize , MSG_WAITALL ) < bufferSize ) {
        std :: cerr << "Error on recv(), did server terminate?\n";
        close ( socketFD );
        return -1;
    }
    // Read the 'type' and 'length' fields from the buffer using helper functions
    int offset = 0;
    uint16_t type , length;
    type = getNextUint16 ( buffer , offset );
    length = getNextUint16 ( buffer , offset );
    delete[] buffer;

    // Step 2: Now that we know the packet length, we can recv() the full packet
    bufferSize = length - ( sizeof ( uint16_t ) + sizeof ( uint16_t ) );
    buffer = new char[ bufferSize ];
    if ( buffer == NULL ) {
        std :: cerr << "Error: Heap Over\n";
        close ( socketFD );
        return -1;
    }
    if ( recv ( socketFD , buffer , bufferSize , MSG_WAITALL ) < bufferSize ) {
        std :: cerr << "Error on recv(), did server terminate?\n";
        close ( socketFD );
        return -1;
    }
    // Put 'offset' as 0, so that the buffer is ready for reading using helper functions
    offset = 0;

    // get status
    uint32_t status = getNextUint32(buffer, offset);
	uint32_t cookie = getNextUint32(buffer, offset);

	// if status != success, print fail message and end process
	if (status != STATUS_SUCCESS)
	{
		if (status == ERROR_USERNAME)
			std::cerr << "user name taken\n";
		std::cerr << "Login failed" << std::endl;
		close (socketFD);
		return 0;
	}

    std :: cout << "Client " << userName << " "
                << inet_ntoa ( clientAddress.sin_addr )
                << ":" << ntohs ( clientAddress.sin_port )
                << " connected to Server running on "
                << serverIP << ":" << serverPort << std :: endl;


    //-----------------------------------------

    // Output the intro screen
    std :: cout << "=== Welcome " << userName << " to CS3103 Chat! ===\n"
                << "1. show : Show all users online\n"
                << "2. talk <user> <message> : Send message to user\n"
                << "3. yell <message> : Send message to all users\n"
                << "4. creategroup <user1> <user2> ... : Create group chat\n"
                << "5. discuss <message> : Send message to users in the group chat\n"
                << "6. leavegroup : Leave group chat\n"
                << "7. help : Display all commands\n"
                << "8. exit : Disconnect from Chat server\n\n";

    /**
     * Some points to remember about C++ input/output -
     * To make the prompt appear on screen, you need to
     *    flush() the output
     */

    // Output the prompt
    std :: cout << "> ";
    std :: cout.flush();
    // Remove the trailing '\n' left by 'cin'
    char trailingLinefeed = std :: cin.get();

    // Infinite loop until user inputs 'exit'
    while ( true ) {

        // We need to get input from the socket as well as from the user's keyboard
        // The select() function can be used for this (See Section 14.5 of Richard Stevens UNIX Textbook)
        /*
         * Note:
         * In Java, there seems to be no select() function (I googled and didn't find one).
         * So, In Java you need to create 2 threads - one for reading socket input, and the other
         * for reading keyboard input.
         */
        fd_set readFDs , exceptionFDs;
        FD_ZERO ( &readFDs );
        FD_ZERO ( &exceptionFDs );
        FD_SET ( STDIN_FILENO , &readFDs );         // For Keyboard input
        FD_SET ( STDIN_FILENO , &exceptionFDs );
        FD_SET ( socketFD , &readFDs );             // For Socket input
        FD_SET ( socketFD , &exceptionFDs );

        // Wait for input using select()
        if ( select ( socketFD + 1 , &readFDs , NULL , &exceptionFDs , NULL ) < 0 ) {
            std :: cerr << "Error on select()\n";
            close ( socketFD );
            return -1;
        }

        // Check if any exception occurred
        if ( FD_ISSET ( socketFD , &exceptionFDs ) || FD_ISSET ( STDIN_FILENO , &exceptionFDs ) ) {
            std :: cerr << "Unexpected error while using select(), did the server terminate?\n";
            close ( socketFD );
            return -1;
        }

        // Check whether we got keyboard input or socket input
        // Keyboard Input from the user
        if ( FD_ISSET ( STDIN_FILENO , &readFDs ) ) {

            /*
             * If you are not comfortable with these C++ I/O functions,
             * then you can use the C versions instead.
             */

            // Now we know that we will not block on using 'getline'
            // Note that we should read ONLY ONE line, otherwise we would block!
            std :: string inputLine;
            // Read one line from the input (this is the C++ version of 'getline')
            std :: getline ( std :: cin , inputLine );

            // Put the string into a string stream (similar to sscanf() in C)
            // Now we can read from 'ss' just like we do with 'cin'
            std :: istringstream ss ( inputLine );

            // Read the user command
            std :: string command;
            ss >> command;         // Just like 'cin >> command;'

            // Check which command the user has input
            /*
             * Note:
             * Here we are not doing any input validation
             */
            // HELP
            if ( command == "help" ) {
                std :: cout << "Commands: show talk yell creategroup discuss leavegroup help exit\n";
            }

            // EXIT
            else if ( command == "exit" ) {
				// Send a Exit request to the server
    			replyOffset = 0 , lengthOffset = LENGTH_FIELD_OFFSET;
    			// Request Type
    			putNextUint16 ( replyBuffer , replyOffset , REQUEST_EXIT );
    			// Length (we will fill this later on)
    			putNextUint16 ( replyBuffer , replyOffset , 0 );
    			// Cookie (Need to use the Cookie assigned by ChatServer)
    			putNextUint32 ( replyBuffer , replyOffset , 0 );
    			// User name
    			putNextString ( replyBuffer , replyOffset , userName );
    			// Now 'replyOffset' has the number of bytes we put in the buffer, we can now write the length
    			putNextUint16 ( replyBuffer , lengthOffset , replyOffset );

    			if ( send ( socketFD , replyBuffer , replyOffset , 0 ) < 0 ) {
        			std :: cerr << "Error on send()\n";
        			close ( socketFD );
        			return -1;
    			}

				continue;
            }

            // SHOW
            else if ( command == "show" ) {
				// Send a SHOW request to the server
    			replyOffset = 0 , lengthOffset = LENGTH_FIELD_OFFSET;
    			// Request Type
    			putNextUint16 ( replyBuffer , replyOffset , REQUEST_SHOW );
    			// Length (we will fill this later on)
    			putNextUint16 ( replyBuffer , replyOffset , 0 );
    			// Cookie
    			putNextUint32 ( replyBuffer , replyOffset , cookie );
    			// Now 'replyOffset' has the number of bytes we put in the buffer, we can now write the length
    			putNextUint16 ( replyBuffer , lengthOffset , replyOffset );

    			if ( send ( socketFD , replyBuffer , replyOffset , 0 ) < 0 ) {
        			std :: cerr << "Error on send()\n";
        			close ( socketFD );
        			return -1;
    			}
				
				continue;
            }

            // TALK
            else if ( command == "talk" ) {
            }

            // YELL
            else if ( command == "yell" ) {
            }

            // DISCUSS
            else if ( command == "discuss" ) {
            }

            // LEAVEGROUP
            else if ( command == "leavegroup" ) {
            }

            // CREATEGROUP
            else if ( command == "creategroup" ) {

                /*
                 * To read names from the input one by one, you
                 * can use the following while loop
                 */

                std :: string groupUserName;
                while ( ss ) {
                    ss >> groupUserName;

                    // To eliminate the classic "last string repeating twice problem"
                    if ( ss )
                        putNextString ( replyBuffer , replyOffset , groupUserName );
                }
                // Terminate with two NULLs (i.e. terminate with an empty string)
                putNextString ( replyBuffer , replyOffset , "" );

                // etc...
            }

            // Wrong command
            else {
                std :: cout << "Incorrect command, type 'help' to see the commands\n";
            }

            // Output the next prompt
            std :: cout << "> ";
            std :: cout.flush();
        }

        // Socket input from the server
        else if ( FD_ISSET ( socketFD , &readFDs ) ) {

            // Now we know that we will not block on using 'recv()'
            // Note that we should read ONLY the length of one packet, otherwise we would block!
            /*
             * Note:
             *
             * If the server connection gets disrupted, recv() will return an error value,
             * so we can detect the connection has broken.
             *
             * In an exceptional case, the server may not be working properly and not send the
             * full packet. In this case, we should use a recv() timeout so that we don't
             * block forever. But for this assignment, it is ok, you are not required to do this.
             */

 			/* All Responses from the Server start with the following Response
 			 * Header:
 			 *
 			 *  |------------------------------------------|
 			 *  |    Response Type    |       Length       |
 			 *  |------------------------------------------|
 			 *  |                  Status                  |
 			 *  |------------------------------------------|
			 */

            // Step 1: First, get the 'type' and 'length' of the packet (first 2 fields are total 4 bytes)
            size_t bufferSize = sizeof ( uint16_t ) + sizeof ( uint16_t );
            char *buffer = new char[ bufferSize ];
            if ( buffer == NULL ) {
                std :: cerr << "Error: Heap Over\n";
                close ( socketFD );
                return -1;
            }
            // The flag MSG_WAITALL ensures that we get the all the 4 bytes in one recv()
            if ( recv ( socketFD , buffer , bufferSize , MSG_WAITALL ) < bufferSize ) {
                std :: cerr << "Error on recv(), did server terminate?\n";
                close ( socketFD );
                return -1;
            }
            // Read the 'type' and 'length' fields from the buffer using helper functions
            int offset = 0;
            uint16_t type , length;
            type = getNextUint16 ( buffer , offset );
            length = getNextUint16 ( buffer , offset );
            delete[] buffer;

            // Step 2: Now that we know the packet length, we can recv() the full packet
            bufferSize = length - ( sizeof ( uint16_t ) + sizeof ( uint16_t ) );
            buffer = new char[ bufferSize ];
            if ( buffer == NULL ) {
                std :: cerr << "Error: Heap Over\n";
                close ( socketFD );
                return -1;
            }
            if ( recv ( socketFD , buffer , bufferSize , MSG_WAITALL ) < bufferSize ) {
                std :: cerr << "Error on recv(), did server terminate?\n";
                close ( socketFD );
                return -1;
            }
            // Put 'offset' as 0, so that the buffer is ready for reading using helper functions
            offset = 0;

            // Process the packet here
            switch ( type ) {

                case RESPONSE_SHOW: {

                    /*
                     * This is an example of how you can read from the
                     * buffer
                     */

                    uint32_t status = getNextUint32 ( buffer , offset );

					if (status == STATUS_SUCCESS)
					{
						int i = 1;
						std::cout << "=== Users Online ===" << std::endl;
						std::string names = getNextString(buffer, offset);
						while (names != "")
						{
							if (names == userName)
								std::cout << i << ". " << names << " (you)" << std::endl;
							else
								std::cout << i << ". " << names << std::endl;
							names = getNextString(buffer, offset);
							++i;
						}
					}

                    // etc...

                    break;
                }

                case RESPONSE_EXIT: {

                    uint32_t status = getNextUint32 ( buffer , offset );

                    // etc...
					if (status == STATUS_SUCCESS)
					{
						std::cout << "You have logged out" << std::endl;

	            		delete[] buffer;
	   				 	close ( socketFD );
	    				delete[] replyBuffer;

						return 0;
	                }
					else
					{
						std::cout << "Exit failed" << std::endl;
						break;
					}
				}
                // etc...

                default: {
                    std :: cerr << "\nError: Unknown packet from server\n";
                    close ( socketFD );
                    return -1;
                }
            }

            // Output next prompt if required
            std :: cout << "> ";
            std :: cout.flush();

            // Deallocate receive buffer
            delete[] buffer;
        }
    }

    // Control should not reach here

    // Deallocate the reply buffer
    close ( socketFD );
    delete[] replyBuffer;

    return 0;
}

std :: string
getNextString ( const char *buffer , int &offset ) {

    char nextString [ MAX_CHAT_LENGTH ];

    // Read the string byte by byte till we reach NULL
    int i = 0;
    while ( ( nextString[i++] = buffer[ offset++ ] ) != '\0' ) {
        if ( i >= MAX_CHAT_LENGTH )
            break;
    }
    nextString[i - 1] = '\0';

    // The value of offset is now ready for the next call to getNextThing()
    return std :: string ( nextString );
}

uint32_t
getNextUint32 ( const char *buffer , int &offset ) {

    // Read the next uint32_t
    uint32_t value;
    value = *( const uint32_t* ) ( buffer + offset );

    // Convert from Network to Host order
    value = ntohl ( value );

    // The value of offset is now ready for the next call to getNextThing()
    offset += sizeof ( uint32_t );
    return value;
}

uint16_t
getNextUint16 ( const char *buffer , int &offset ) {

    // Read the next uint16_t
    uint16_t value;
    value = *( const uint16_t* ) ( buffer + offset );

    // Convert from Network to Host order
    value = ntohs ( value );

    // The value of offset is now ready for the next call to getNextThing()
    offset += sizeof ( uint16_t );
    return value;
}

void
putNextString ( char *buffer , int &offset , const std :: string &nextString ) {

    // Write the string byte by byte till we reach NULL
    int i = 0;
    const char *str = nextString.c_str();
    while ( ( buffer[ offset++ ] = str[i++] ) != '\0' )
        ;

    // The value of offset is now ready for the next call to putNextThing()
}

void
putNextUint32 ( char *buffer , int &offset , uint32_t nextUint32 ) {

    // Convert from Host to Network order
    nextUint32 = htonl ( nextUint32 );

    // Write the next uint32_t
    *( uint32_t* ) ( buffer + offset ) = nextUint32;

    // The value of offset is now ready for the next call to putNextThing()
    offset += sizeof ( uint32_t );
}

void
putNextUint16 ( char *buffer , int &offset , uint16_t nextUint16 ) {

    // Convert from Host to Network order
    nextUint16 = htons ( nextUint16 );

    // Write the next uint16_t
    *( uint16_t* ) ( buffer + offset ) = nextUint16;

    // The value of offset is now ready for the next call to putNextThing()
    offset += sizeof ( uint16_t );
}
