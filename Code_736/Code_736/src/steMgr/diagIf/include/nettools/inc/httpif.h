/*
 *  Copyright 2007 by Texas Instruments Incorporated.
 *  All rights reserved. Property of Texas Instruments Incorporated.
 *  Restricted rights to use, duplicate or disclose this code are
 *  granted through contract.
 *
 *  @(#) TCP/IP_Network_Developers_Kit 1.93.00.09 08-16-2007 (ndk-c09)
 */
//--------------------------------------------------------------------------
// Network Tools Library
//--------------------------------------------------------------------------
// HTTPIF.H
//
// Interface to the tools available in NETTOOL.LIB
//
// Author: Michael Hanrahan
// Copyright 1999, 2000 by Texas Instruments Inc.
//-------------------------------------------------------------------------

#ifndef _HTTPIF_H_
#define _HTTPIF_H_

//
// HTTP SERVER SERVICE
//

//
// Telnet Parameter Structure
//
typedef struct _ntparam_http {
        int     MaxCon;             // Max number of HTTP connections
                                    // (When set to NULL, 4 is used)
        int     Port;               // Port (set to NULL for HTTP default)
        } NTPARAM_HTTP;


//
// httpOpen
//
// Create an instance of the Http Server based on a specific interface
// or IP addres (IPAddr=0 is a wildcard that matches any installed address).
//
// Compatible with NT_MODE_IFIDX and NT_MODE_IPADDR.
//
// Returns a HANDLE to the Http instance
//
_extern HANDLE httpOpen( NTARGS *pNTA, NTPARAM_HTTP *pNTP );

//
// httpClose
//
// Destroy an instance of the Http Server
//
_extern void  httpClose(HANDLE h);

//
// httpClientProcess
//
// This entry point is used to invoke HTTP directly from
// the server daemon.
//
_extern int   httpClientProcess( SOCKET Sock, UINT32 unused );


//
// Public String Definitions
//
/*
char *CONTENT_TYPE_APPLET  = "application/octet-stream ";
char *CONTENT_TYPE_AU      = "audio/au ";
char *CONTENT_TYPE_DOC     = "application/msword ";
char *CONTENT_TYPE_GIF     = "image/gif ";
char *CONTENT_TYPE_HTML    = "text/html ";
char *CONTENT_TYPE_JPG     = "image/jpeg ";
char *CONTENT_TYPE_MPEG    = "video/mpeg ";
char *CONTENT_TYPE_PDF     = "application/pdf ";
char *CONTENT_TYPE_WAV     = "audio/wav ";
char *CONTENT_TYPE_ZIP     = "application/zip ";
char *CONTENT_TYPE         = "Content-type: ";
char *CONTENT_LENGTH       = "Content-length: ";
char *CRLF                 = "\r\n";
char *DEFAULT_NAME         = "index.html";
char *HTTP_VER             = "HTTP/1.0 ";
char *SPACE                = " ";
*/
extern char *CONTENT_TYPE_APPLET;
extern char *CONTENT_TYPE_AU;
extern char *CONTENT_TYPE_DOC;
extern char *CONTENT_TYPE_GIF;
extern char *CONTENT_TYPE_HTML;
extern char *CONTENT_TYPE_JPG;
extern char *CONTENT_TYPE_MPEG;
extern char *CONTENT_TYPE_PDF;
extern char *CONTENT_TYPE_WAV;
extern char *CONTENT_TYPE_ZIP;
extern char *CONTENT_TYPE;
extern char *CONTENT_LENGTH;
extern char *CRLF;
extern char *DEFAULT_NAME;
extern char *HTTP_VER;
extern char *SPACE;

//
// Status codes for httpSendFullResponse() and httpSendStatusLine()
//
enum HTTP_STATUS_CODE
{
    HTTP_OK=200,
    HTTP_NO_CONTENT=204,
    HTTP_BAD_REQUEST=400,
    HTTP_AUTH_REQUIRED=401,
    HTTP_NOT_FOUND=404,
    HTTP_NOT_ALLOWED=405,
    HTTP_NOT_IMPLEMENTED=501,
    HTTP_STATUS_CODE_END
};

#define HTTPPORT           80
#define MAX_RESPONSE_SIZE  200

_extern int  httpVersion(void);

//
// httpSendClientStr
//
// Sends the indicated NULL terminated response string to
// the indicated client socket - i.e.: calls strlen() and send()
//
_extern void httpSendClientStr( SOCKET Sock, char *Response );

//
// httpSendStatusLine
//
// Sends a formatted response message to Sock with the given
// status code and content type. The value of ContentType can
// be NULL if no ContentType is required.
//
_extern void httpSendStatusLine( SOCKET Sock, int StatusCode, char *ContentType );

//
// httpSendFullResponse
//
// Sends a full formatted response message to Sock, including the
// file indicated by the filename in RequestedFile. The status code
// for this call is usually HTTP_OK.
//
_extern void httpSendFullResponse( SOCKET Sock, int StatusCode,
                                   char *RequestedFile );

//
// httpSendEntityLength
//
// Write out the entiry length tag, and terminates the header
// with an additional CRLF.
//
// Since the header is terminated, this must be the last tag
// written. Entity data should follow immediately.
//
_extern void httpSendEntityLength( SOCKET Sock, INT32 EntityLength );


//
// httpSendErrorResponse
//
// Sends a formatted error response message to Sock based on the
// status code.
//
_extern void httpSendErrorResponse( SOCKET Sock, int StatusCode );


//
// httpErrorResponseHook
//
// Pointer to hook error response functions. Once an application
// writes a pointer to httpErrorResponseHook, the function will
// be called to generate all HTML code contained in any error
// response message. This function is optional and should not be
// used unless required.
//
// The installed callback should generate only the "Content Length"
// tag, followed immediately by the HTML data. The content length
// tag can be written using the httpSendEntityLength() function.
// (This function also terminates the HTTP header.)
//
// The installed callback returns:
//   1 - Error was handled and HTML was generated by the callback
//   0 - No HTML was generated (HTTP server should use its default)
//
_extern int (*httpErrorResponseHook)(SOCKET Sock, int StatusCode);


//
// Common error responses
//
#define http400(Sock)  httpSendErrorResponse(Sock, HTTP_BAD_REQUEST)
#define http404(Sock)  httpSendErrorResponse(Sock, HTTP_NOT_FOUND)
#define http405(Sock)  httpSendErrorResponse(Sock, HTTP_NOT_ALLOWED)
#define http501(Sock)  httpSendErrorResponse(Sock, HTTP_NOT_IMPLEMENTED)

#endif
