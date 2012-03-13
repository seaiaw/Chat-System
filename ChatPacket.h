/**
 * @file
 * @brief    Packet formats for the chat protocol
 *
 * @details  I have provided you with some example packet formats,
 *   for you to use as a guideline.
 *
 * @author  Kartik S
 */

#ifndef __ChatPacket_h
#define __ChatPacket_h

#include <stdint.h>

/*
 * This is a very simple implementation, where the client sends a
 * request to the server, and the server sends back a response.
 *
 * All Requests from the client start with this Request Header:
 * (for the size of each field, see the structures defined below)
 *
 *  |------------------------------------------|
 *  |    Request Type     |       Length       |
 *  |------------------------------------------|
 *  |                  Cookie                  |
 *  |------------------------------------------|
 *
 * Each request type is represented by a unique integer.
 * Length specifies the length of the entire request datagram in
 * bytes.
 * Cookie is a unique value given to the client by the server after
 * login. If the TCP connection between the client and server breaks,
 * the user can use the cookie instead of logging in again.
 *
 * (If you do not want to use Cookies, that is also ok)
 *
 * Format of each Request body is as follows:
 *
 * 1. Login Request:
 *
 *  |------------------------------------------|
 *  |      User Name terminated by NULL        |
 *  |------------------------------------------|
 *
 * Cookie value for the Login request is 0. The User name has a
 * maximum size (defined by MAX_USER_NAME_LENGTH).
 *
 * All Responses from the Server start with the following Response
 * Header:
 *
 *  |------------------------------------------|
 *  |    Response Type    |       Length       |
 *  |------------------------------------------|
 *  |                  Status                  |
 *  |------------------------------------------|
 *
 * Length specifies the length of the Response Datagram in bytes.
 * Status indicates Success or an appropriate Error Code.
 *
 * Note: There is no field to map responses to the original requests.
 * (i.e. how do we know which response is for which request?)
 * However, in this simple chat implementation, it is not required.
 *
 * Format of each Response body is as follows:
 *
 * 1. Login Response:
 *
 *  |------------------------------------------|
 *  |                  Cookie                  |
 *  |------------------------------------------|
 *
 * Cookie is a unique value given to the client for that particular chat
 * session.
 *
 * 2. Show Response:
 *
 *  |------------------------------------------|
 *  |              List of users               |
 *  |------------------------------------------|
 *
 * List of users is a sequence of user names, each terminated by NULL.
 * The last user name is terminated by two NULLs (in other words, the
 * last user name is an empty string). The size of the list
 * should be at least one (since the user who sent the request must be
 * online).
 */

/*
 * You need to think about the other packet formats yourselves. This
 * was just an few examples as guidelines.
 *
 * I have given you some helper functions to read/write packet bytes
 * easily (see the .cpp files)
 */

/*
 * You can either use structures, or read/write the fields individually
 * using the helper functions.
 */

/**
 * @brief  Request Header
 */
struct RequestHeader {
    uint16_t requestType;    ///< Request type
    uint16_t length;         ///< Total size of request (in bytes)
    uint32_t cookie;         ///< Cookie value provided by server
};

/**
 * @brief  Response Header
 */
struct ResponseHeader {
    uint16_t responseType;   ///< Response type
    uint16_t length;         ///< Total size of response (in bytes)
    uint32_t status;         ///< Status (SUCCESS/ERROR CODE)
};

/**
 * @brief  Request Types
 */
enum {
    REQUEST_LOGIN         = 1 ,
    REQUEST_SHOW          = 2 ,
    REQUEST_TALK	  = 3 ,
    REQUEST_YELL	  = 4 ,
    REQUEST_CREATEGROUP	  = 5 ,
    REQUEST_DISCUSS	  = 6 ,
    REQUEST_LEAVEGROUP	  = 7 ,
    REQUEST_HELP	  = 8 ,
    REQUEST_EXIT	  = 9 ,
    // etc...
};

/**
 * @brief  Response Types
 */
enum {
    RESPONSE_LOGIN         = 11 ,
    RESPONSE_SHOW          = 12 ,
    RESPONSE_TALK          = 13 ,
    RESPONSE_YELL          = 14 ,
    RESPONSE_CREATEGROUP   = 15 ,
    RESPONSE_DISCUSS       = 16 ,
    RESPONSE_LEAVEGROUP    = 17 ,
    RESPONSE_HELP          = 18 ,
    RESPONSE_EXIT          = 19 ,
    RESPONSE_EXIT_FWD      = 191, //GC

    // etc ...
};

/**
 * @brief  Response Status values
 */
enum {
    STATUS_SUCCESS          = 0 ,
    ERROR_COOKIE_INVALID    = 1 ,

    // etc ...

    ERROR_UNKNOWN           = 1024
};

/// @brief  Maximum length of the user name (including NULL)
#define MAX_USER_NAME_LENGTH 32
/// @brief  Maximum length of the chat message (including NULL)
#define MAX_CHAT_LENGTH      2048
/// @brief  Maximum length of a packet (this is just some magic number)
#define MAX_PACKET_LENGTH    ( 2 * MAX_CHAT_LENGTH )

/// @brief  Byte Offset of the length field in the packets
#define LENGTH_FIELD_OFFSET  sizeof ( uint16_t )

#endif  // __ChatPacket_h
