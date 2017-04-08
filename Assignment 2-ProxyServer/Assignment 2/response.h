//
//  response.h
//  Assignment2-ProxyServer
//
//  Created by Hassaan on 07/04/2017.
//  Copyright © 2017 HaigaTech. All rights reserved.
//

#ifndef response_h
#define response_h


/* HTTP response and header for a successful request.  */

static char* ok_response =
"HTTP/1.0 200 OK\n"
"Content-Type: text/html\n"
"\n";
//"<html>\n"
//" <body>\n"
//"  <h1>Server</h1>\n"
//"  <p>This is the response from the server.</p>\n"
//" </body>\n"
//"</html>\n";

/* HTTP response, header, and body indicating that the we didn't
 understand the request.  */

static char* bad_request_response =
"HTTP/1.0 400 Bad Request\n"
"Content-type: text/html\n"
"\n"
"<html>\n"
" <body>\n"
"  <h1>Bad Request</h1>\n"
"  <p>This server did not understand your request.</p>\n"
" </body>\n"
"</html>\n";

/* HTTP response, header, and body template indicating that the
 requested document was not found.  */

static char* not_found_response_template =
"HTTP/1.0 404 Not Found\n"
"Content-type: text/html\n"
"\n"
"<html>\n"
" <body>\n"
"  <h1>Not Found</h1>\n"
"  <p>The requested URL %s was not found on this server.</p>\n"
" </body>\n"
"</html>\n";

/* HTTP response, header, and body template indicating that the
 method was not understood.  */

static char* bad_method_response_template =
"HTTP/1.0 501 Method Not Implemented\n"
"Content-type: text/html\n"
"\n"
"<html>\n"
" <body>\n"
"  <h1>Method Not Implemented</h1>\n"
"  <p>The method %s is not implemented by this server.</p>\n"
" </body>\n"
"</html>\n";

#endif /* response_h */
