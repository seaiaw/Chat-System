// ChatServer.cpp

#include <iostream>
#include <cstdlib>
#include <string>
#include <map>
#include <vector>
#include <stdint.h>
#include <time.h>
#include <limits.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "ChatPacket.h"

using namespace std;

/*
 * Here, you can define a data structure to store information
 * about the users, as shown in the 'User' structure below.
 *
 * For info on vector (and other useful data structures in C++), see -
 * http://www.cplusplus.com/reference/stl/vector/
 */

/**
 * @brief  Group chat status
 */
enum {
    GROUPCHAT_EMPTY     = 0 ,   ///< User not in group chat
    GROUPCHAT_ACCEPTED  = 1 ,   ///< User currently in group chat
    GROUPCHAT_PENDING   = 2     ///< User has yet to answer invitation
};

/**
 * @brief  Data type representing a list of users
 */
typedef vector <string> UserList;

/**
 * @brief  Information about a User
 */
struct User {
    string userName;          ///< User Name
    uint32_t      cookie;            ///< Cookie value
    int           socketFD;          ///< TCP Socket Descriptor
    int           groupChatStatus;   ///< Group chat status
    UserList*     groupChatUsers;    ///< Users in Group Chat (including this user)
};

typedef vector <User> List;

List userList;

/**
 * @brief  Read-write lock used on the above data structures
 *
 * If you do not want to use locks, thats also fine, since in this
 * assignment the program will most likely still work without locks.
 */
pthread_rwlock_t userDataLock;

/// @brief  Thread handling one particular client
void* clientThread ( void *args );

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

/// @brief  Starting point of the server
int
main ( int argc , char **argv ) {

    // Step 1: Initialise the server

    // Get the service port to use
    uint16_t servicePort;
    cout << "=== Welcome to the Chat Server!! ===\n";
    cout << "Enter Service Port: ";
    cin >> servicePort;

    // Create a socket to listen for client connections
    int socketFD;
    if ( ( socketFD = socket ( AF_INET , SOCK_STREAM , 0 ) ) < 0 ) {
        cerr << "Error creating Server socket\n";
        return -1;
    }

    // Bind to all IP Addresses of this machine, on the input service port
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons ( servicePort );
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    if ( bind ( socketFD , (const struct sockaddr*) &serverAddress ,
                sizeof ( struct sockaddr_in ) ) < 0 ) {
        cerr << "Error on bind()\n";
        close ( socketFD );
        return -1;
    }

    // Tell the OS that the server wants to listen on this socket
    if ( listen ( socketFD , 10 ) < 0 ) {
        cerr << "Error on listen()\n";
        close ( socketFD );
        return -1;
    }

    // Initialise the Read-write lock
    if ( pthread_rwlock_init ( &userDataLock , NULL ) != 0 ) {
        cerr << "Error on pthread_rwlock_init()\n";
        close ( socketFD );
        return -1;
    }

    // Step 2: Wait for connections
    cout << "Chat Server Running on 127.0.0.1:" << servicePort << endl;
    int newSocketFD;
    while ( true ) {
        if ( ( newSocketFD = accept ( socketFD , NULL , NULL ) ) < 0 ) {
            cerr << "Error on accept()\n";
            close ( socketFD );
            return -1;
        }

        // Step 3: On a new connection, create a new thread
        pthread_t threadID;
        int *userSocketFD = new int;
        *userSocketFD = newSocketFD;
        if ( userSocketFD == NULL ) {
            cerr << "Out of heap memory\n";
            close ( socketFD );
            return -1;
        }

        // Also, tell the thread the socket FD it should use for this user
        if ( pthread_create ( &threadID , NULL , clientThread , userSocketFD ) != 0 ) {
            cerr << "Error on pthread_create()\n";
            close ( socketFD );
            return -1;
        }
    }

    // Control should not reach here
    pthread_rwlock_destroy ( &userDataLock );

    return 0;
}

void* clientThread ( void *args ) {
	uint32_t status = STATUS_SUCCESS; 

	User currentUser;
	UserList groupList;

    // Get the socketFD this thread has been assigned to
    int *userSocketFD = (int*) args;
    int socketFD = *userSocketFD;
    delete userSocketFD;

    // Get the Client's IP and Port
    struct sockaddr_in clientAddress;
    socklen_t addressLength = sizeof ( struct sockaddr_in );
    if ( getpeername ( socketFD , (struct sockaddr*) &clientAddress ,
                       &addressLength ) != 0 ) {
        cerr << "Error on getpeername()\n";
        close ( socketFD );
        return NULL;
    }


    // Receive packets on this socket
    while ( true ) {

        // Step 1: First, get the 'type' and 'length' of the packet (first 2 fields are total 4 bytes)
        size_t bufferSize = sizeof ( uint16_t ) + sizeof ( uint16_t );
        char *buffer = new char[ bufferSize ];
        if ( buffer == NULL ) {
            cerr << "Error: Heap Over\n";
            close ( socketFD );
            return NULL;
        }
        // The flag MSG_WAITALL ensures that we get the all the 4 bytes in one recv()
        if ( recv ( socketFD , buffer , bufferSize , MSG_WAITALL ) < bufferSize )
            break;
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
            return NULL;
        }
        if ( recv ( socketFD , buffer , bufferSize , MSG_WAITALL ) < bufferSize )
            break;
        // Put 'offset' as 0, so that the buffer is ready for reading using helper functions
        offset = 0;

        // Keep a reply buffer ready for sending a reply back
        char *replyBuffer = new char[ MAX_PACKET_LENGTH ];
        if ( replyBuffer == NULL ) {
            cerr << "Error: Heap Over\n";
            close ( socketFD );
            return NULL;
        }
        int replyOffset = 0 , lengthOffset = LENGTH_FIELD_OFFSET;

        // Step 3: Check what is the type of packet received
        switch ( type ) {
            /*
             * Event:
             * Login Request
             *

             * Action:
             * 1. Set Cookie value
             * 2. Send LOGIN_RESPONSE
             */
	    	case REQUEST_LOGIN: {

                // Get the cookie value from the packet
                uint32_t cookie;
                cookie = getNextUint32 ( buffer , offset );
				string userName;
				userName = getNextString (buffer, offset);
				for (int i = 0; i < userList.size(); i++)
				{
					if (userList.at(i).userName == userName)
						status = ERROR_USERNAME;
				}
				if (status == STATUS_SUCCESS)
				{
					currentUser.userName = userName;
					currentUser.socketFD = socketFD;
					currentUser.cookie = ntohs ( clientAddress.sin_port );
					currentUser.groupChatStatus = GROUPCHAT_EMPTY;
					currentUser.groupChatUsers = &groupList; 

					userList.push_back(currentUser);

					// Client bob connected from 127.0.0.1:58101
    				cout << "Client " << currentUser.userName << " connected from "
                		 << inet_ntoa ( clientAddress.sin_addr )
                		 << ":" << ntohs ( clientAddress.sin_port )
                		 << endl;
				}				


                /*
                 * If you are modifying the data structures, then
                 * Write lock.
                 */
                // Read Lock the Data structure
                pthread_rwlock_rdlock ( &userDataLock );

                // Do any processing here...
				//////////////////////////////
				// Login Response packet to the Client
    			replyOffset = 0 , lengthOffset = LENGTH_FIELD_OFFSET;
    			// Request Type
    			putNextUint16 ( replyBuffer , replyOffset , RESPONSE_LOGIN );
    			// Length (we will fill this later on)
    			putNextUint16 ( replyBuffer , replyOffset , 0 );
    			// Status
    			putNextUint32 ( replyBuffer , replyOffset , status );
    			// Cookie (need to assign Cookie)
    			putNextUint32 ( replyBuffer , replyOffset , currentUser.cookie);
    			// Now 'replyOffset' has the number of bytes we put in the buffer, we can now write the length
    			putNextUint16 ( replyBuffer , lengthOffset , replyOffset );
				///////////////////////////////

                // Unlock the Data structure
                pthread_rwlock_unlock ( &userDataLock );

                // Send response here...
    			if ( send ( socketFD , replyBuffer , replyOffset , 0 ) < 0 ) {
        			cerr << "Error on send()\n";
       			 	close ( socketFD );
    			}

                break;
            }

			/*
			 * Event:
			 * Talk Request
			 * 
			 * Action:
			 * 1. Check if the cookie value is OK
			 * 2. Reply a message to sender
			 * 3. Forward message to the receiver
			 */
			case REQUEST_TALK: {
				// Get the cookie value from the packet
				uint32_t cookie;
				int receiverSocketFD = -1;
				cookie = getNextUint32(buffer, offset);
				string senderName = getNextString(buffer, offset);
				string receiverName = getNextString(buffer, offset);			

				for (int i = 0; i < userList.size(); i++)
				{
					if (userList.at(i).userName == receiverName)
					{
						receiverSocketFD = userList.at(i).socketFD;
						break;
					}
				}
				if (receiverSocketFD == -1)
					status = ERROR_USER_NOT_FOUND;
				
				if (status == STATUS_SUCCESS)
				{
	                // Read Lock the Data structure
	                pthread_rwlock_rdlock ( &userDataLock );

                	// Do any processing here...
					//////////////////////////////
					// Login Response packet to the Client
    				replyOffset = 0 , lengthOffset = LENGTH_FIELD_OFFSET;
    				// Request Type
    				putNextUint16 ( replyBuffer , replyOffset , RESPONSE_TALK_FWD );
    				// Length (we will fill this later on)
    				putNextUint16 ( replyBuffer , replyOffset , 0 );
    				// Status
    				putNextUint32 ( replyBuffer , replyOffset , status );
    				// Sender Name
    				putNextString ( replyBuffer , replyOffset , senderName );
					// Receiver Name
    				putNextString ( replyBuffer , replyOffset , receiverName );
					// Packet Message
					string message = getNextString (buffer, offset);
					while (message != "")
					{
    					putNextString ( replyBuffer , replyOffset , message );
						message = getNextString (buffer, offset);
					}
                	// Terminate with two NULLs (i.e. terminate with an empty string)
                	putNextString ( replyBuffer , replyOffset , "" );
    				// Now 'replyOffset' has the number of bytes we put in the buffer, we can now write the length
    				putNextUint16 ( replyBuffer , lengthOffset , replyOffset );
					///////////////////////////////

                	// Unlock the Data structure
                	pthread_rwlock_unlock ( &userDataLock );

    				if ( send ( receiverSocketFD , replyBuffer , replyOffset , 0 ) < 0 ) {
        				cerr << "Error on send()\n";
       				 	close ( receiverSocketFD );
    				}
				}
				
				delete[] replyBuffer;

				char *replyBuffer = new char[ MAX_PACKET_LENGTH ];
        		if ( replyBuffer == NULL ) {
            		cerr << "Error: Heap Over\n";
            		close ( socketFD );
            		return NULL;
        		}

	            // Read Lock the Data structure
	            pthread_rwlock_rdlock ( &userDataLock );

               	// Do any processing here...
				//////////////////////////////
				// Login Response packet to the Client
   				replyOffset = 0 , lengthOffset = LENGTH_FIELD_OFFSET;
   				// Request Type
   				putNextUint16 ( replyBuffer , replyOffset , RESPONSE_TALK );
   				// Length (we will fill this later on)
   				putNextUint16 ( replyBuffer , replyOffset , 0 );
   				// Status
   				putNextUint32 ( replyBuffer , replyOffset , status );
   				// Now 'replyOffset' has the number of bytes we put in the buffer, we can now write the length
   				putNextUint16 ( replyBuffer , lengthOffset , replyOffset );
				///////////////////////////////

               	// Unlock the Data structure
               	pthread_rwlock_unlock ( &userDataLock );

    			if ( send ( socketFD , replyBuffer , replyOffset , 0 ) < 0 ) {
        			cerr << "Error on send()\n";
       			 	close ( socketFD );
    			}
				
				// not a serious error, reset status to STATUS_SUCCESS after report error to sender
				status = STATUS_SUCCESS;

				break;
			}

			/*
			 * Event:

			 * Yell Request
			 * 
			 * Action:
			 * 1. Check if the cookie value is OK
			 * 2. Reply a message to sender
			 * 3. Forward message to the all other online users
			 */
			case REQUEST_YELL: {
				// Get the cookie value from the packet
				uint32_t cookie;
				cookie = getNextUint32(buffer, offset);	
				string userName;	
				int receiverSocketFD;

				for (int i = 0; i < userList.size(); i++)
				{
					if (userList.at(i).cookie == cookie)
					{
						userName = userList.at(i).userName;
						break;
					}
				}

				if (userList.size() == 1)
					status = ERROR_NO_USER_ONLINE;
				
				if (status == STATUS_SUCCESS)
				{
	                // Read Lock the Data structure
	                pthread_rwlock_rdlock ( &userDataLock );

					//////////////////////////////
					// Login Response packet to the Client
    				replyOffset = 0 , lengthOffset = LENGTH_FIELD_OFFSET;
    				// Response Type
    				putNextUint16 ( replyBuffer , replyOffset , RESPONSE_YELL_FWD );
    				// Length (we will fill this later on)
    				putNextUint16 ( replyBuffer , replyOffset , 0 );
    				// Status
    				putNextUint32 ( replyBuffer , replyOffset , status );
    				// Sender Name
    				putNextString ( replyBuffer , replyOffset , userName );
					// Packet Message
					string message = getNextString (buffer, offset);
					while (message != "")
					{
    					putNextString ( replyBuffer , replyOffset , message );
						message = getNextString (buffer, offset);
					}
                	// Terminate with two NULLs (i.e. terminate with an empty string)
                	putNextString ( replyBuffer , replyOffset , "" );
    				// Now 'replyOffset' has the number of bytes we put in the buffer, we can now write the length
    				putNextUint16 ( replyBuffer , lengthOffset , replyOffset );
					///////////////////////////////

                	// Unlock the Data structure
                	pthread_rwlock_unlock ( &userDataLock );
					
					// send to all online users
					for (int i = 0; i < userList.size(); i++)
					{
						if (userList.at(i).cookie != cookie)
						{
							receiverSocketFD = userList.at(i).socketFD;
    						if ( send ( receiverSocketFD , replyBuffer , replyOffset , 0 ) < 0 )
							{
        						cerr << "Error on send()\n";
       				 			close ( receiverSocketFD );
    						}
						}
					}
				}
				
				delete[] replyBuffer;

				char *replyBuffer = new char[ MAX_PACKET_LENGTH ];
        		if ( replyBuffer == NULL ) {
            		cerr << "Error: Heap Over\n";
            		close ( socketFD );
            		return NULL;
        		}

	            // Read Lock the Data structure
	            pthread_rwlock_rdlock ( &userDataLock );

               	// Do any processing here...
				//////////////////////////////
				// Login Response packet to the Client
   				replyOffset = 0 , lengthOffset = LENGTH_FIELD_OFFSET;
   				// Request Type
   				putNextUint16 ( replyBuffer , replyOffset , RESPONSE_YELL );
   				// Length (we will fill this later on)
   				putNextUint16 ( replyBuffer , replyOffset , 0 );
   				// Status
   				putNextUint32 ( replyBuffer , replyOffset , status );
   				// Now 'replyOffset' has the number of bytes we put in the buffer, we can now write the length
   				putNextUint16 ( replyBuffer , lengthOffset , replyOffset );
				///////////////////////////////

               	// Unlock the Data structure
               	pthread_rwlock_unlock ( &userDataLock );

    			if ( send ( socketFD , replyBuffer , replyOffset , 0 ) < 0 ) {
        			cerr << "Error on send()\n";
       			 	close ( socketFD );
    			}
				
				// not a serious error, reset status to STATUS_SUCCESS after report error to sender
				status = STATUS_SUCCESS;

				break;
			}

            /*
             * Event:
             * Show Request
             *
             * Action:
             * 1. Check if the cookie value is OK
             * 2. Send back the list of users
             */
            case REQUEST_SHOW: {

                // Get the cookie value from the packet
                uint32_t cookie;
                cookie = getNextUint32 ( buffer , offset );

                // Read Lock the Data structure
                pthread_rwlock_rdlock ( &userDataLock );

                // Do any processing here...
				//////////////////////////////
				// Login Response packet to the Client
   				replyOffset = 0 , lengthOffset = LENGTH_FIELD_OFFSET;
   				// Request Type
   				putNextUint16 ( replyBuffer , replyOffset , RESPONSE_SHOW );
   				// Length (we will fill this later on)
   				putNextUint16 ( replyBuffer , replyOffset , 0 );
   				// Status
   				putNextUint32 ( replyBuffer , replyOffset , status );
				// Names
				for (int i = 0; i < userList.size(); i++)
					putNextString(replyBuffer, replyOffset, userList.at(i).userName);
				// Terminate with two NULLs (i.e. terminate with an empty string)
                putNextString ( replyBuffer , replyOffset , "" );
   				// Now 'replyOffset' has the number of bytes we put in the buffer, we can now write the length
   				putNextUint16 ( replyBuffer , lengthOffset , replyOffset );
				///////////////////////////////

                // Unlock the Data structure
                pthread_rwlock_unlock ( &userDataLock );

                // Send response here...
    			if ( send ( socketFD , replyBuffer , replyOffset , 0 ) < 0 ) {
        			cerr << "Error on send()\n";
       			 	close ( socketFD );
    			}

                break;
            }

			/*
			 * Event:
			 * CreateGroup Request
			 * 
			 * Action:
			 * 1. Check if the cookie value is OK
			 * 2. Reply a message to sender
			 * 3. Forward invitation to the invited users
			 */
			case REQUEST_CREATEGROUP: {
				// Get the cookie value from the packet
				uint32_t cookie;
				cookie = getNextUint32(buffer, offset);		
				int receiverSocketFD;

				// gather names of invited users
				// including the creator of the group
				(currentUser.groupChatUsers)->push_back(currentUser.userName);
				string message = getNextString (buffer, offset);
				while (message != "")
				{
//					for (int i = 0; i < userList.size(); i++)
//					{
//						if (userList.at(i).userName == message)
//						{
//							userName = userList.at(i).userName;
//							break;
//						}
//					}
					(currentUser.groupChatUsers)->push_back(message);
					message = getNextString (buffer, offset);
				}

				currentUser.groupChatStatus = GROUPCHAT_PENDING;

//					currentUser.userName = userName;
//					currentUser.socketFD = socketFD;
//					currentUser.cookie = ntohs ( clientAddress.sin_port );
//					currentUser.groupChatStatus = GROUPCHAT_EMPTY;
//					currentUser.groupChatUsers = &groupList; 
				
				if (status == STATUS_SUCCESS)
				{
	                // Read Lock the Data structure
	                pthread_rwlock_rdlock ( &userDataLock );

					//////////////////////////////
					// Login Response packet to the Client
    				replyOffset = 0 , lengthOffset = LENGTH_FIELD_OFFSET;
    				// Response Type
    				putNextUint16 ( replyBuffer , replyOffset , RESPONSE_CREATEGROUP_FWD);
    				// Length (we will fill this later on)
    				putNextUint16 ( replyBuffer , replyOffset , 0 );
    				// Status
    				putNextUint32 ( replyBuffer , replyOffset , status );
    				// Sender Name
    				putNextString ( replyBuffer , replyOffset , currentUser.userName );
					// invited namelist
					for (int j = 0; j < (currentUser.groupChatUsers)->size(); j++)
					{
						putNextString(replyBuffer, replyOffset, (currentUser.groupChatUsers)->at(j));
					}
    				// Now 'replyOffset' has the number of bytes we put in the buffer, we can now write the length
    				putNextUint16 ( replyBuffer , lengthOffset , replyOffset );
					///////////////////////////////

                	// Unlock the Data structure
                	pthread_rwlock_unlock ( &userDataLock );
					
					// send to invited users
					for (int j = 0; j < (currentUser.groupChatUsers)->size(); j++)		
					{
						if ((currentUser.groupChatUsers)->at(j) == currentUser.userName)
							continue;

						for (int i = 0; i < userList.size(); i++)
						{
							if (userList.at(i).userName == (currentUser.groupChatUsers)->at(j))
							{
								userList.at(i).groupChatStatus = GROUPCHAT_PENDING;
								receiverSocketFD = userList.at(i).socketFD;
	    						if ( send ( receiverSocketFD , replyBuffer , replyOffset , 0 ) < 0 )
								{
	        						cerr << "Error on send()\n";
	       				 			close ( receiverSocketFD );
	    						}
								break;
							}
						}
					}
				}
				
				delete[] replyBuffer;

				char *replyBuffer = new char[ MAX_PACKET_LENGTH ];
        		if ( replyBuffer == NULL ) {
            		cerr << "Error: Heap Over\n";
            		close ( socketFD );
            		return NULL;
        		}

	            // Read Lock the Data structure
	            pthread_rwlock_rdlock ( &userDataLock );

               	// Do any processing here...
				//////////////////////////////
				// Login Response packet to the Client
   				replyOffset = 0 , lengthOffset = LENGTH_FIELD_OFFSET;
   				// Request Type
   				putNextUint16 ( replyBuffer , replyOffset , RESPONSE_CREATEGROUP );
   				// Length (we will fill this later on)
   				putNextUint16 ( replyBuffer , replyOffset , 0 );
   				// Status
   				putNextUint32 ( replyBuffer , replyOffset , status );
   				// Now 'replyOffset' has the number of bytes we put in the buffer, we can now write the length
   				putNextUint16 ( replyBuffer , lengthOffset , replyOffset );
				///////////////////////////////

               	// Unlock the Data structure
               	pthread_rwlock_unlock ( &userDataLock );

    			if ( send ( socketFD , replyBuffer , replyOffset , 0 ) < 0 ) {
        			cerr << "Error on send()\n";
       			 	close ( socketFD );
    			}
				
				// not a serious error, reset status to STATUS_SUCCESS after report error to sender
				status = STATUS_SUCCESS;

				break;
			}

			/*
			 * Event:
			 * LeaveGroup Request
			 * 
			 * Action:
			 * 1. Check if the cookie value is OK
			 * 2. Reply a message to sender
			 * 3. Forward notification to the group members
			 */
			case REQUEST_LEAVEGROUP: {

				// !!! INCOMPLETE !!!

				// set groupChatStatus back to empty
				currentUser.groupChatStatus = GROUPCHAT_EMPTY;
				// erase all the groupChatUsers record
				currentUser.groupChatUsers = &groupList; 
				
				break;
			}

            /*
             * Event:
             * Exit Request
             *

             * Action:
             * 1. Check if the cookie value is OK
             * 2. Send RESPONSE_EXIT
	     	 * 3. Send RESPONSE_EXIT_FWD
             */
            case REQUEST_EXIT: {

                // Get the cookie value from the packet
                uint32_t cookie;
                cookie = getNextUint32 ( buffer , offset );
				string userName;
				userName = getNextString (buffer, offset);
				for (int i = 0; i < userList.size(); i++)
				{
					if (userList.at(i).userName == userName)
					{
						userList.erase(userList.begin()+i);
					}
				}
				// Client bob exited from 127.0.0.1:58101
    			cout << "Client " << userName << " exited from "
                	 << inet_ntoa ( clientAddress.sin_addr )
                	 << ":" << ntohs ( clientAddress.sin_port )
                	 << endl;	

                /*
                 * If you are modifying the data structures, then
                 * Write lock.

                 */
                // Read Lock the Data structure
                pthread_rwlock_rdlock ( &userDataLock );

                // Do any processing here...
				//////////////////////////////
				// Send a Login request to the sender
    			replyOffset = 0 , lengthOffset = LENGTH_FIELD_OFFSET;
    			// Request Type
    			putNextUint16 ( replyBuffer , replyOffset , RESPONSE_EXIT );
    			// Length (we will fill this later on)
    			putNextUint16 ( replyBuffer , replyOffset , 0 );
    			// Status
    			putNextUint32 ( replyBuffer , replyOffset , status);
    			// Now 'replyOffset' has the number of bytes we put in the buffer, we can now write the length
    			putNextUint16 ( replyBuffer , lengthOffset , replyOffset );
				///////////////////////////////

                // Unlock the Data structure
                pthread_rwlock_unlock ( &userDataLock );

                // Send response here...
    			if ( send ( socketFD , replyBuffer , replyOffset , 0 ) < 0 ) {
        			cerr << "Error on send()\n";
       			 	close ( socketFD );
    			}

				delete[] replyBuffer;
       			
				char *replyBuffer = new char[ MAX_PACKET_LENGTH ];
        		if ( replyBuffer == NULL ) {
            		cerr << "Error: Heap Over\n";
            		close ( socketFD );
            		return NULL;
        		}

                // Read Lock the Data structure
                pthread_rwlock_rdlock ( &userDataLock );

				//////////////////////////////
				// Send a Login request to the other clients
    			replyOffset = 0 , lengthOffset = LENGTH_FIELD_OFFSET;
    			// Request Type
    			putNextUint16 ( replyBuffer , replyOffset , RESPONSE_EXIT_FWD );
    			// Length (we will fill this later on)
    			putNextUint16 ( replyBuffer , replyOffset , 0 );
    			// Status
    			putNextUint32 ( replyBuffer , replyOffset , status );
    			// User name
    			putNextString ( replyBuffer , replyOffset , userName );
    			// Now 'replyOffset' has the number of bytes we put in the buffer, we can now write the length
    			putNextUint16 ( replyBuffer , lengthOffset , replyOffset );
				///////////////////////////////

                // Unlock the Data structure
                pthread_rwlock_unlock ( &userDataLock );

				int everySocketFD = 0;
				
				for (int i = 0; i < userList.size(); i++)
				{
					if (userList.at(i).userName != userName)
					{
						everySocketFD = userList.at(i).socketFD;
						if ( send ( everySocketFD , replyBuffer , replyOffset , 0 ) < 0 ) {
    	    				cerr << "Error on send()\n";
    	   			 		close ( socketFD );
    					}
					}
				}

        		// Buffer should be deallocated
        		delete[] buffer;
        		delete[] replyBuffer;

                // break;
				close (socketFD);
				return NULL;
				
            }


            // etc..

            default: {
                break;
            }
        }

        // Buffer should be deallocated
        delete[] buffer;
        delete[] replyBuffer;

		if (status != STATUS_SUCCESS)
		{
			cerr << "Error occurred" << endl;
			close (socketFD);
			return NULL;
		}
    }

    // If we broke out of the loop, then the connection was closed or
    // there was an error
    cerr << "Client closed connection unexpectedly\n";
    close ( socketFD );

    // Exit the thread
    return NULL;
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
