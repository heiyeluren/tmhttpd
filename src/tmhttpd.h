/**
 * TieMa(Tiny&Mini) Http Server 
 * Copyright (C) 2008 heiyeluren. All rights reserved.
 *
 * tmhttpd is a mini open-source web server, mainly used in html/js/cs/images, 
 * such as static resources to provide services on the web. 
 * 
 * Use and distribution licensed under the BSD license.  See
 * the LICENSE file for full text.
 *
 * To learn more open-source code, visit: http://heiyeluren.googlecode.com
 * My blog: http://blog.csdn.net/heiyeshuwu
 *
 * $Id: tmhttpd.h 2008-6-12, 2008-6-22 04:02 heiyeluren $
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <netdb.h>
#include <dirent.h>
#include <time.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define _GNU_SOURCE
#include <getopt.h>



/* constant define */
#define SERVER_NAME				"tmhttpd/1.0.0_alpha"
#define VERSION					"1.0.0 alpha"

#define BUFFER_SIZE				8192
#define REQUEST_MAX_SIZE		10240

/* configure constant */
#define IS_DEBUG				0					/* Is open debug mode */
#define IS_DAEMON				0					/* Is daemon running */
#define	IS_BROWSE				1					/* Is allow browse file/dir list */
#define IS_LOG					0					/* Is write access log */
#define IS_CGI					1					/* Is open cgi mode */
#define DEFAULT_HTTP_PORT		80					/* Server listen port */
#define DEFAULT_MAX_CLIENT		100					/* Max connection requests */
#define DEFAULT_DOCUMENT_ROOT	"./"				/* Web document root directory */
#define DEFAULT_DIRECTORY_INDEX	"index.html"		/* Directory default index file name */
#define DEFAULT_LOG_PATH		"/tmp/tmhttpd.log"	/* Access log path */
#define DEFAULT_CGI_PATH		"cgi-bin"	        /* Default cgi-bin path */


/* data struct define */
struct st_request_info {
	char *method;
	char *pathinfo;
	char *query;
	char *protocal;
	char *path;
	char *file;
	char *physical_path;
};

