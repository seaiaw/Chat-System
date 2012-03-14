// ChatClient.cpp

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

using namespace std;

/// @brief  Helper function to get the next NULL terminated string in the packet stream
string getNextString ( const char *buffer , int &offset );
/// @brief  Helper function to get the next uint32_t in the packet stream
uint32_t getNextUint32 ( const char *buffer , int &offset );
/// @brief  Helper function to get the next uint16_t in the packet stream
uint16_t getNextUint16 ( const char *buffer , int &offset );

/// @brief  Helper function to put the next NULL terminated string in the packet stream
void putNextString ( char *buffer , int &offset , const string &nextString );
/// @brief  Helper function to put the next uint32_t in the packet stream
void putNextUint32 ( char *buffer , int &offset , uint32_t nextUint32 );
/// @brief  Helper function to put the next uint16_t in the packet stream
void putNextUint16 ( char *buffer , int &offset , uint16_t nextUint16 );

/// @brief  Starting point of the client
int main ( int argc , char **argv ) {

    // Get the Server's IP and Port
    string serverIP;
    uint16_t serverPort;
    string userName;
    cout << "=== Welcome to the Chat Client!! === \n";
    cout << "Enter Chat Server IP: ";
    cin >> serverIP;
    cout << "Enter Chat Server Port: ";
    cin >> serverPort;
    cout << "Enter user name: ";
    cin >> userName;

    // Connect to the server
    // step 1: socket
    int socketFD;
    if ( ( socketFD = socket ( AF_INET , SOCK_STREAM , 0 ) ) < 0 ) {
        cerr << "Error on socket()\n";
        return -1;
    }

    // step 2: server IP and address
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr ( serverIP.c_str() );
    serverAddress.sin_port = htons ( serverPort );

    // step 3: connect
    if ( connect ( socketFD , (struct sockaddr*) &serverAddress ,
                   sizeof ( struct sockaddr_in ) ) != 0 ) {
        cerr << "Error on connect(), is the server running?\n";
        return -1;
    }

    // client IP and port
    struct sockaddr_in clientAddress;
    socklen_t addressLength = sizeof ( struct sockaddr_in );
    if ( getsockname ( socketFD , (struct sockaddr*) &clientAddress ,
                       &addressLength ) != 0 ) {
        cerr << "Error on getsockname()\n";
        close ( socketFD );
        return -1;
    }

    // Create a buffer we can use to send packets to the server
    char *replyBuffer = new char[ MAX_PACKET_LENGTH ];
    if ( replyBuffer == NULL ) {
        cerr << "Error: Heap over\n";
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
        cerr << "Error on send()\n";
        close ( socketFD );
        return -1;
    }

    // Wait for a successful response
    // -----------------------------------------------
    size_t bufferSize = sizeof ( uint16_t ) + sizeof ( uint16_t );
    char *buffer = new char[ bufferSize ];
    if ( buffer == NULL ) {
        cerr << "Error: Heap Over\n";
        close ( socketFD );
        return -1;
    }
    // The flag MSG_WAITALL ensures that we get the all the 4 bytes in one recv()
    if ( recv ( socketFD , buffer , bufferSize , MSG_WAITALL ) < bufferSize ) {
        cerr << "Error on recv(), did server terminate?\n";
        close ( socketFD );
        return -1;
    }
    // Read the 'type' and 'length' fields from the buffer using helper functions
    int offset = 0;
    uint16_t type , length;
    type = getNextUint16 ( buffer , offset );
    length = getNextUint16 ( buffer , offset );
    delete[] buffer;

    bufferSize = length - ( sizeof ( uint16_t ) + sizeof ( uint16_t ) );
    buffer = new char[ bufferSize ];
    if ( buffer == NULL ) {
        cerr << "Error: Heap Over\n";
        close ( socketFD );
        return -1;
    }
    if ( recv ( socketFD , buffer , bufferSize , MSG_WAITALL ) < bufferSize ) {
        cerr << "Error on recv(), did server terminate?\n";
        close ( socketFD );
        return -1;
    }
    // Put 'offset' as 0, so that the buffer is ready for reading using helper functions
    offset = 0;

    // get status
    uint32_t status = getNextUint32(buffer, offset);
	uint32_t cookie = getNextUint32(buffer, offset);
	cout << cookie << endl;

	// if status != success, print fail message and end process
	if (status != STATUS_SUCCESS)
	{
		if (status == ERROR_USERNAME)
			cerr << "user name taken\n";
		cerr << "Login failed" << endl;
		close (socketFD);
		return 0;
	}

    cout << "Client " << userName << " "
         << inet_ntoa ( clientAddress.sin_addr )
         << ":" << ntohs ( clientAddress.sin_port )
         << " connected to Server running on "
         << serverIP << ":" << serverPort << endl;


    //-----------------------------------------

    // Output the intro screen
    cout << "=== Welcome " << userName << " to CS3103 Chat! ===\n"
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
    cout << "> ";
    cout.flush();
    // Remove the trailing '\n' left by 'cin'
    char trailingLinefeed = cin.get();

    // Infinite loop until user inputs 'exit'
    while ( true ) {

        // We need to get input from the socket as well as from the user's keyboard
        // The select() function can be used for this (See Section 14.5 of Richard Stevens UNIX Textbook)

        fd_set readFDs , exceptionFDs;
        FD_ZERO ( &readFDs );
        FD_ZERO ( &exceptionFDs );
        FD_SET ( STDIN_FILENO , &readFDs );         // For Keyboard input
        FD_SET ( STDIN_FILENO , &exceptionFDs );
        FD_SET ( socketFD , &readFDs );             // For Socket input
        FD_SET ( socketFD , &exceptionFDs );

        // Wait for input using select()
        if ( select ( socketFD + 1 , &readFDs , NULL , &exceptionFDs , NULL ) < 0 ) {
            cerr << "Error on select()\n";
            close ( socketFD );
            return -1;
        }

        // Check if any exception occurred
        if ( FD_ISSET ( socketFD , &exceptionFDs ) || FD_ISSET ( STDIN_FILENO , &exceptionFDs ) ) {
            cerr << "Unexpected error while using select(), did the server terminate?\n";
            close ( socketFD );
            return -1;
        }

        // Check whether we got keyboard input or socket input
        // Keyboard Input from the user
        if ( FD_ISSET ( STDIN_FILENO , &readFDs ) ) {

            // Now we know that we will not block on using 'getline'
            // Note that we should read ONLY ONE line, otherwise we would block!
            string inputLine;
            // Read one line from the input (this is the C++ version of 'getline')
            getline ( cin , inputLine );

            // Put the string into a string stream (similar to sscanf() in C)
            // Now we can read from 'ss' just like we do with 'cin'
            istringstream ss ( inputLine );

            // Read the user command
            string command;
            ss >> command;         // Just like 'cin >> command;'

            // Check which command the user has input
            /*
             * Note:
             * Here we are not doing any input validation
             */
            // HELP
            if ( command == "help" ) {
    			cout<< "1. show : Show all users online\n"
        	 		<< "2. talk <user> <message> : Send message to user\n"
         			<< "3. yell <message> : Send message to all users\n"
         			<< "4. creategroup <user1> <user2> ... : Create group chat\n"
         			<< "5. discuss <message> : Send message to users in the group chat\n"
         			<< "6. leavegroup : Leave group chat\n"
         			<< "7. help : Display all commands\n"
         			<< "8. exit : Disconnect from Chat server\n\n";
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
    			putNextUint32 ( replyBuffer , replyOffset , cookie );
    			// User name
    			putNextString ( replyBuffer , replyOffset , userName );
    			// Now 'replyOffset' has the number of bytes we put in the buffer, we can now write the length
    			putNextUint16 ( replyBuffer , lengthOffset , replyOffset );

    			if ( send ( socketFD , replyBuffer , replyOffset , 0 ) < 0 ) {
        			cerr << "Error on send()\n";
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
        			cerr << "Error on send()\n";
        			close ( socketFD );
        			return -1;
    			}
				
				continue;
            }

            // TALK
            else if ( command == "talk" ) {

				// Send a TALK request to the server
    			replyOffset = 0 , lengthOffset = LENGTH_FIELD_OFFSET;
    			// Request Type
    			putNextUint16 ( replyBuffer , replyOffset , REQUEST_TALK );
    			// Length (we will fill this later on)
    			putNextUint16 ( replyBuffer , replyOffset , 0 );
    			// Cookie
    			putNextUint32 ( replyBuffer , replyOffset , cookie );

                string receiverName;
				string message;
                ss >> receiverName;
				// Sender Name
				putNextString ( replyBuffer, replyOffset, userName);
				// Receiver Name
				putNextString ( replyBuffer, replyOffset, receiverName);
                while ( ss ) {
					ss >> message;

                    // To eliminate the classic "last string repeating twice problem"
                    if ( ss )
                        putNextString ( replyBuffer , replyOffset , message );
                }
                // Terminate with two NULLs (i.e. terminate with an empty string)
                putNextString ( replyBuffer , replyOffset , "" );
				
    			// Now 'replyOffset' has the number of bytes we put in the buffer, we can now write the length
    			putNextUint16 ( replyBuffer , lengthOffset , replyOffset );

    			if ( send ( socketFD , replyBuffer , replyOffset , 0 ) < 0 ) {
        			cerr << "Error on send()\n";
        			close ( socketFD );
        			return -1;
    			}
				
				continue;
            }

            // YELL
            else if ( command == "yell" ) {
				// Send a YELL request to the server
    			replyOffset = 0 , lengthOffset = LENGTH_FIELD_OFFSET;
    			// Request Type
    			putNextUint16 ( replyBuffer , replyOffset , REQUEST_YELL );
    			// Length (we will fill this later on)
    			putNextUint16 ( replyBuffer , replyOffset , 0 );
    			// Cookie
    			putNextUint32 ( replyBuffer , replyOffset , cookie );
				
				string message;
                while ( ss ) {
					ss >> message;

                    // To eliminate the classic "last string repeating twice problem"
                    if ( ss )
                        putNextString ( replyBuffer , replyOffset , message );
                }
                // Terminate with two NULLs (i.e. terminate with an empty string)
                putNextString ( replyBuffer , replyOffset , "" );

    			// Now 'replyOffset' has the number of bytes we put in the buffer, we can now write the length
    			putNextUint16 ( replyBuffer , lengthOffset , replyOffset );

    			if ( send ( socketFD , replyBuffer , replyOffset , 0 ) < 0 ) {
        			cerr << "Error on send()\n";
        			close ( socketFD );
        			return -1;
    			}
				
				continue;
            }

            // DISCUSS
            else if ( command == "discuss" ) {
            }

            // LEAVEGROUP
            else if ( command == "leavegroup" ) {
            }

            // CREATEGROUP
            else if ( command == "creategroup" ) {
				
				// Send a YELL request to the server
    			replyOffset = 0 , lengthOffset = LENGTH_FIELD_OFFSET;
    			// Request Type
    			putNextUint16 ( replyBuffer , replyOffset , REQUEST_CREATEGROUP );
    			// Length (we will fill this later on)
    			putNextUint16 ( replyBuffer , replyOffset , 0 );
    			// Cookie
    			putNextUint32 ( replyBuffer , replyOffset , cookie );

                /*
                 * To read names from the input one by one, you
                 * can use the following while loop
                 */
				
                string groupUserName;
                while ( ss ) {
                    ss >> groupUserName;

                    // To eliminate the classic "last string repeating twice problem"
                    if ( ss )
                        putNextString ( replyBuffer , replyOffset , groupUserName );
                }
                // Terminate with two NULLs (i.e. terminate with an empty string)
                putNextString ( replyBuffer , replyOffset , "" );
    			// Now 'replyOffset' has the number of bytes we put in the buffer, we can now write the length
    			putNextUint16 ( replyBuffer , lengthOffset , replyOffset );

    			if ( send ( socketFD , replyBuffer , replyOffset , 0 ) < 0 ) {
        			cerr << "Error on send()\n";
        			close ( socketFD );
        			return -1;
    			}
				
				continue;

                // etc...
            }

            // Wrong command
            else {
                cout << "Incorrect command, type 'help' to see the commands\n";
            }

            // Output the next prompt
            cout << "> ";
            cout.flush();
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

            // Step 1: First, get the 'type' and 'length' of the packet (first 2 fields are total 4 bytes)
            size_t bufferSize = sizeof ( uint16_t ) + sizeof ( uint16_t );
            char *buffer = new char[ bufferSize ];
            if ( buffer == NULL ) {
                cerr << "Error: Heap Over\n";
                close ( socketFD );
                return -1;
            }
            // The flag MSG_WAITALL ensures that we get the all the 4 bytes in one recv()
            if ( recv ( socketFD , buffer , bufferSize , MSG_WAITALL ) < bufferSize ) {
                cerr << "Error on recv(), did server terminate?\n";
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
                cerr << "Error: Heap Over\n";
                close ( socketFD );
                return -1;
            }
            if ( recv ( socketFD , buffer , bufferSize , MSG_WAITALL ) < bufferSize ) {
                cerr << "Error on recv(), did server terminate?\n";
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
						cout << "=== Users Online ===" << endl;
						string names = getNextString(buffer, offset);
						while (names != "")
						{
							if (names == userName)
								cout << i << ". " << names << " (you)" << endl;
							else
								cout << i << ". " << names << endl;
							names = getNextString(buffer, offset);
							++i;
						}
					}

                    // etc...

                    break;
                }
				
                case RESPONSE_YELL: {

                    uint32_t status = getNextUint32 ( buffer , offset );

					if (status == STATUS_SUCCESS)
					{
						;
					}
					else if (status == ERROR_NO_USER_ONLINE)
					{
						cerr<< "There is no other user online" << endl;
					}

                    // etc...

                    break;
                }

                case RESPONSE_YELL_FWD: {

                    uint32_t status = getNextUint32 ( buffer , offset );
					string senderName = getNextString(buffer, offset);
					string message = getNextString(buffer, offset);

					if (status == STATUS_SUCCESS)
					{
						cout << endl << senderName << " says: ";
						while (message != "")
						{
							cout << message << " ";
							message = getNextString (buffer, offset);
						}
						cout << endl;
					}

                    // etc...

                    break;
                }

                case RESPONSE_TALK: {

                    uint32_t status = getNextUint32 ( buffer , offset );

					if (status == STATUS_SUCCESS)
					{
						;
					}
					else if (status == ERROR_USER_NOT_FOUND)
					{
						cerr<< "No such user" << endl;
					}

                    // etc...

                    break;
                }

                case RESPONSE_TALK_FWD: {

                    uint32_t status = getNextUint32 ( buffer , offset );
					string senderName = getNextString(buffer, offset);
					string receiverName = getNextString(buffer, offset);
					string message = getNextString(buffer, offset);

					if (status == STATUS_SUCCESS)
					{
						cout << endl << senderName << " says: ";
						while (message != "")
						{
							cout << message << " ";
							message = getNextString (buffer, offset);
						}
						cout << endl;
					}

                    // etc...

                    break;
                }

                case RESPONSE_EXIT: {

                    uint32_t status = getNextUint32 ( buffer , offset );

                    // etc...
					if (status == STATUS_SUCCESS)
					{
						cout << "You have logged out" << endl;

	            		delete[] buffer;
	   				 	close ( socketFD );
	    				delete[] replyBuffer;

						return 0;
	                }
					else
					{
						cout << "Exit failed" << endl;
						break;
					}
				}

                case RESPONSE_EXIT_FWD: {

                    uint32_t status = getNextUint32 ( buffer , offset );
					string senderName = getNextString (buffer, offset);

                    // etc...
					if (status == STATUS_SUCCESS)
					{
						cout << endl << senderName << " has logged out" << endl;
						break;
	                }
					else
					{
						cout << "Errorneous packet" << endl;
						break;
					}
				}
                // etc...

                default: {
                    cerr << "\nError: Unknown packet from server\n";
                    close ( socketFD );
                    return -1;
                }
            }

            // Output next prompt if required
            cout << "> ";
            cout.flush();

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

string getNextString ( const char *buffer , int &offset ) {

    char nextString [ MAX_CHAT_LENGTH ];

    // Read the string byte by byte till we reach NULL
    int i = 0;
    while ( ( nextString[i++] = buffer[ offset++ ] ) != '\0' ) {
        if ( i >= MAX_CHAT_LENGTH )
            break;
    }
    nextString[i - 1] = '\0';

    // The value of offset is now ready for the next call to getNextThing()
    return string ( nextString );
}

uint32_t getNextUint32 ( const char *buffer , int &offset ) {

    // Read the next uint32_t
    uint32_t value;
    value = *( const uint32_t* ) ( buffer + offset );

    // Convert from Network to Host order
    value = ntohl ( value );

    // The value of offset is now ready for the next call to getNextThing()
    offset += sizeof ( uint32_t );
    return value;
}

uint16_t getNextUint16 ( const char *buffer , int &offset ) {

    // Read the next uint16_t
    uint16_t value;
    value = *( const uint16_t* ) ( buffer + offset );

    // Convert from Network to Host order
    value = ntohs ( value );

    // The value of offset is now ready for the next call to getNextThing()
    offset += sizeof ( uint16_t );
    return value;
}

void putNextString ( char *buffer , int &offset , const string &nextString ) {

    // Write the string byte by byte till we reach NULL
    int i = 0;
    const char *str = nextString.c_str();
    while ( ( buffer[ offset++ ] = str[i++] ) != '\0' )
        ;

    // The value of offset is now ready for the next call to putNextThing()
}

void putNextUint32 ( char *buffer , int &offset , uint32_t nextUint32 ) {

    // Convert from Host to Network order
    nextUint32 = htonl ( nextUint32 );

    // Write the next uint32_t
    *( uint32_t* ) ( buffer + offset ) = nextUint32;

    // The value of offset is now ready for the next call to putNextThing()
    offset += sizeof ( uint32_t );
}

void putNextUint16 ( char *buffer , int &offset , uint16_t nextUint16 ) {

    // Convert from Host to Network order
    nextUint16 = htons ( nextUint16 );

    // Write the next uint16_t
    *( uint16_t* ) ( buffer + offset ) = nextUint16;

    // The value of offset is now ready for the next call to putNextThing()
    offset += sizeof ( uint16_t );
}
