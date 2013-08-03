/**
 * TieMa Mini Http Server 
 * Copyright (C) 2008 heiyeluren <heiyeluren#gmail.com>
 *
 * tmhttpd is a mini open-source web server, mainly used in html/js/cs/images, 
 * such as static resources to provide services on the web. 
 * 
 * Use and distribution licensed under the BSD license.  See
 * the LICENSE file for full text.
 *
 * To learn more open-source, visit: http://heiyeluren.googlecode.com
 * My blog: http://blog.csdn.net/heiyeshuwu
 *
 * $Id: tmhttpd.c 2008-6-12, 2008-6-22 04:00 heiyeluren $
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


/*
#ifndef SIZE_T_MAX
#define SIZE_T_MAX				2147483647L
#endif

#define READ_TIMEOUT			60
#define WRITE_TIMEOUT			300
#define DEFAULT_CHARSET			"iso-8859-1"

#define METHOD_UNKNOWN			0
#define METHOD_GET				1
#define METHOD_HEAD				2
#define METHOD_POST				3

#ifndef STDIN_FILENO
#define STDIN_FILENO			0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO			1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO			2
#endif


#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
*/

/* configure constant */
#define IS_DEBUG				0					/* Is open debug mode */
#define IS_DAEMON				0					/* Is daemon running */
#define	IS_BROWSE				0					/* Is allow browse file/dir list */
#define IS_LOG					0					/* Is write access log */
#define DEFAULT_HTTP_PORT		80					/* Server listen port */
#define DEFAULT_MAX_CLIENT		100					/* Max connection requests */
#define DEFAULT_DOCUMENT_ROOT	"./"				/* Web document root directory */
#define DEFAULT_DIRECTORY_INDEX	"index.html"		/* Directory default index file name */
#define DEFAULT_LOG_PATH		"/tmp/tmhttpd.log"	/* Access log path */


/* data struct define */
struct st_request_info {
/*	int method_i; */
	char *method;
	char *pathinfo;
	char *query;
	char *protocal;
	char *path;
	char *file;
	char *physical_path;
};

/*
struct st_response_info {
	int status;
	char *date;
	char *server;
	char *cache_control;
	char *expires;
	char *last_modified;
	char *content_type;
	int content_length;
	char *contents;
};
*/

/* default configure */
/* static char *index_names[] = { "index.html", "index.htm", "index.xhtml", "default.html", "default.htm","home.html","home.htm" }; */
static unsigned short g_is_debug	= IS_DEBUG;
static unsigned short g_is_daemon	= IS_DAEMON;
static unsigned short g_is_browse	= IS_BROWSE;
static unsigned short g_is_log		= IS_LOG;
static unsigned int g_port			= DEFAULT_HTTP_PORT;
static unsigned int g_max_client	= DEFAULT_MAX_CLIENT;
static char g_dir_index[32]			= DEFAULT_DIRECTORY_INDEX;
static char g_doc_root[256]			= DEFAULT_DOCUMENT_ROOT; 
static char g_log_path[256]			= DEFAULT_LOG_PATH;

/* Global variable */
static int g_log_fd					= 0;



/********************************
 *
 *   Http Server Basic Function
 *
 ********************************/

/**
 * Die alert message
 *
 */
void die(char *mess){
    perror(mess); 
    exit(1); 
}

/**
 * substr - Sub string from pos to length
 *
 */
static char *substr( const char *s, int start_pos, int length, char *ret ){
    char buf[length+1];
    int i, j, end_pos;
	int str_len = strlen(s);

    if (str_len <= 0 || length < 0){
		return "";        
	}
	if (length == 0){
		length = str_len - start_pos;
	}
	start_pos = ( start_pos < 0 ? (str_len + start_pos) : ( start_pos==0 ? start_pos : start_pos-- ) );
	end_pos = start_pos + length;

    for(i=start_pos, j=0; i<end_pos && j<=length; i++, j++){
		buf[j] = s[i];        
	}
    buf[length] = '\0';
    strcpy(ret, buf);

    return(ret);
}

/**
 * explode -  separate string by separator
 *
 * @param string from - need separator 
 * @param char delim - separator
 * @param pointarray to - save return separate result
 * @param int item_num - return sub string total
 * 
 * @include stdlib.h 
 * @include string.h
 *
 * @example
 * char *s, **r;
 * int num;
 * explode(s, '\n', &r, &num);
 * for(i=0; i<num; i++){
 *     printf("%s\n", r[i]);
 * }
 * 
 */
static void explode(char *from, char delim, char ***to, int *item_num){
    int i, j, k, n, temp_len;
    int max_len = strlen(from) + 1;
    char buf[max_len], **ret;
       
    for(i=0, n=1; from[i]!='\0'; i++){
        if (from[i] == delim) n++;
    }
    
    ret = (char **)malloc(n*sizeof(char *));
    for (i=0, k=0; k<n; k++){
        memset(buf, 0, max_len);     
        for(j=0; from[i]!='\0' && from[i]!=delim; i++, j++) buf[j] = from[i];
        i++;
        temp_len = strlen(buf)+1;
        ret[k] = malloc(temp_len);
        memcpy(ret[k], buf, temp_len);
    } 
    *to = ret;
    *item_num = n;
}

/**
 * strtolower - string to lowner
 *
 */
static char *strtolower( char *s ){
	int i, len = sizeof(s);
	for( i = 0; i < len; i++ ){
		s[i] = ( s[i] >= 'A' && s[i] <= 'Z' ? s[i] + 'a' - 'A' : s[i] );
	}
	return(s);
}

/**
 * strtoupper - string to upper
 *
 */
static char *strtoupper( char *s ){
	int i, len = sizeof(s);
	for( i = 0; i < len; i++ ){
		s[i] = ( s[i] >= 'a' && s[i] <= 'z' ? s[i] + 'A' - 'a' : s[i] );
	}
	return(s);
}

/**
 * strpos - find char at string position
 *
 */
static int strpos (const char *s, char c){
	int i, len;
	if (!s || !c) return -1;
	len = strlen(s);
	for (i=0; i<len; i++){
		if (s[i] == c) return i;
	}
	return -1;	
}

/**
 * strrpos - find char at string last position
 *
 */
static int strrpos (const char *s, char c){
	int i, len;
	if (!s || !c) return -1;
	len = strlen(s);
	for (i=len; i>=0; i--){
		if (s[i] == c) return i;
	}
	return -1;
}




/**
 * trim - strip left&right space char
 *
 */
static char *trim( char *s ){   
    int l;   
    for( l=strlen(s); l>0 && isspace((u_char)s[l-1]); l-- ){
		s[l-1] = '\0';   
	}
    return(s);   
}   


/**
 * ltrim - strip left space char
 *
 */
static char *ltrim( char *s ){   
    char *p;   
    for(p=s; isspace((u_char)*p); p++ );   
    if( p!=s ) strcpy(s, p);   
    return(s);   
}   


/**
 *  filesize - get file size
 */
static long filesize(const char *filename){
    struct stat buf;
    if (!stat(filename, &buf)){
        return buf.st_size;
    }
    return 0;
}

/**
 * file_exists - check file is exist
 */
static int file_exists(const char *filename){
    struct stat buf;
    if (stat(filename, &buf) < 0){
        if (errno == ENOENT){
            return 0;
        }
    }
    return 1;
}

/**
 * file_get_contents - read file contents
 *
 */
static int file_get_contents( const char *filename, size_t filesize, char *ret, off_t length ){
	if ( !file_exists(filename) || access(filename, R_OK)!=0 )	return -1;

	int fd;
	char buf[filesize];

	if ( (fd = open(filename, O_RDONLY)) == -1) return -1;
	length = ( length > 0 ? length : filesize);
	read(fd, buf, length);
	strcpy(ret, buf);
	close(fd);

	return 0;
}

/**
 * is_dir - check file is directory
 * 
 */
static int is_dir(const char *filename){
	struct stat buf;
	if ( stat(filename, &buf) < 0 ){
		return -1;
	}
	if (S_ISDIR(buf.st_mode)){
		return 1;
	}
	return 0;
}

/**
 * is_file - check file is regular file
 * 
 */
static int is_file(const char *filename){
	struct stat buf;
	if ( stat(filename, &buf) < 0 ){
		return -1;
	}
	if (S_ISREG(buf.st_mode)){
		return 1;
	}
	return 0;
}

/**
 * Fetch current date tme
 *
 */
static void getdate(char *s){
	char *wday[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
	time_t timep;
	struct tm *p;

	time(&timep);
	p = localtime(&timep);
	sprintf(s, "%d-%d-%d %d:%d:%d",(1900+p->tm_year), (1+p->tm_mon), p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
} 

/**
 * Get MIME type header
 *
 */
static void mime_content_type( const char *name, char *ret ){
    char *dot, *buf; 

    dot = strrchr(name, '.'); 

	/* Text */
    if ( strcmp(dot, ".txt") == 0 ){
        buf = "text/plain";
    } else if ( strcmp( dot, ".css" ) == 0 ){
        buf = "text/css";
    } else if ( strcmp( dot, ".js" ) == 0 ){
        buf = "text/javascript";
    } else if ( strcmp(dot, ".xml") == 0 || strcmp(dot, ".xsl") == 0 ){
        buf = "text/xml";
    } else if ( strcmp(dot, ".xhtm") == 0 || strcmp(dot, ".xhtml") == 0 || strcmp(dot, ".xht") == 0 ){
        buf = "application/xhtml+xml";
    } else if ( strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0 || strcmp(dot, ".shtml") == 0 || strcmp(dot, ".hts") == 0 ){
        buf = "text/html";

	/* Images */
    } else if ( strcmp( dot, ".gif" ) == 0 ){
        buf = "image/gif";
    } else if ( strcmp( dot, ".png" ) == 0 ){
        buf = "image/png";
    } else if ( strcmp( dot, ".bmp" ) == 0 ){
        buf = "application/x-MS-bmp";
    } else if ( strcmp( dot, ".jpg" ) == 0 || strcmp( dot, ".jpeg" ) == 0 || strcmp( dot, ".jpe" ) == 0 || strcmp( dot, ".jpz" ) == 0 ){
        buf = "image/jpeg";

	/* Audio & Video */
    } else if ( strcmp( dot, ".wav" ) == 0 ){
        buf = "audio/wav";
    } else if ( strcmp( dot, ".wma" ) == 0 ){
        buf = "audio/x-ms-wma";
    } else if ( strcmp( dot, ".wmv" ) == 0 ){
        buf = "audio/x-ms-wmv";
    } else if ( strcmp( dot, ".au" ) == 0 || strcmp( dot, ".snd" ) == 0 ){
        buf = "audio/basic";
    } else if ( strcmp( dot, ".midi" ) == 0 || strcmp( dot, ".mid" ) == 0 ){
        buf = "audio/midi";
    } else if ( strcmp( dot, ".mp3" ) == 0 || strcmp( dot, ".mp2" ) == 0 ){
        buf = "audio/x-mpeg";
	} else if ( strcmp( dot, ".rm" ) == 0  || strcmp( dot, ".rmvb" ) == 0 || strcmp( dot, ".rmm" ) == 0 ){
        buf = "audio/x-pn-realaudio";
    } else if ( strcmp( dot, ".avi" ) == 0 ){
        buf = "video/x-msvideo";
    } else if ( strcmp( dot, ".3gp" ) == 0 ){
        buf = "video/3gpp";
    } else if ( strcmp( dot, ".mov" ) == 0 ){
        buf = "video/quicktime";
    } else if ( strcmp( dot, ".wmx" ) == 0 ){
        buf = "video/x-ms-wmx";
	} else if ( strcmp( dot, ".asf" ) == 0  || strcmp( dot, ".asx" ) == 0 ){
        buf = "video/x-ms-asf";
    } else if ( strcmp( dot, ".mp4" ) == 0 || strcmp( dot, ".mpg4" ) == 0 ){
        buf = "video/mp4";
	} else if ( strcmp( dot, ".mpe" ) == 0  || strcmp( dot, ".mpeg" ) == 0 || strcmp( dot, ".mpg" ) == 0 || strcmp( dot, ".mpga" ) == 0 ){
        buf = "video/mpeg";

	/* Documents */
    } else if ( strcmp( dot, ".pdf" ) == 0 ){
        buf = "application/pdf";
    } else if ( strcmp( dot, ".rtf" ) == 0 ){
        buf = "application/rtf";
	} else if ( strcmp( dot, ".doc" ) == 0  || strcmp( dot, ".dot" ) == 0 ){
        buf = "application/msword";
	} else if ( strcmp( dot, ".xls" ) == 0  || strcmp( dot, ".xla" ) == 0 ){
        buf = "application/msexcel";
	} else if ( strcmp( dot, ".hlp" ) == 0  || strcmp( dot, ".chm" ) == 0 ){
        buf = "application/mshelp";
	} else if ( strcmp( dot, ".swf" ) == 0  || strcmp( dot, ".swfl" ) == 0 || strcmp( dot, ".cab" ) == 0 ){
        buf = "application/x-shockwave-flash";
	} else if ( strcmp( dot, ".ppt" ) == 0  || strcmp( dot, ".ppz" ) == 0 || strcmp( dot, ".pps" ) == 0 || strcmp( dot, ".pot" ) == 0 ){
        buf = "application/mspowerpoint";

	/* Binary & Packages */
    } else if ( strcmp( dot, ".zip" ) == 0 ){
        buf = "application/zip";
    } else if ( strcmp( dot, ".rar" ) == 0 ){
        buf = "application/x-rar-compressed";
    } else if ( strcmp( dot, ".gz" ) == 0 ){
        buf = "application/x-gzip";
    } else if ( strcmp( dot, ".jar" ) == 0 ){
        buf = "application/java-archive";
	} else if ( strcmp( dot, ".tgz" ) == 0  || strcmp( dot, ".tar" ) == 0 ){
        buf = "application/x-tar";
	} else {
		buf = "application/octet-stream";
	}
	strcpy(ret, buf);
}

/**
 * Hex a char
 *
 */
static int _hex( char c )
{
    if ( c >= '0' && c <= '9' )
        return c - '0';
    if ( c >= 'a' && c <= 'f' )
        return c - 'a' + 10;
    if ( c >= 'A' && c <= 'F' )
        return c - 'A' + 10;
    return 0;		/* shouldn't happen, we're guarded by isxdigit() */
}

/**
 * Encode a string
 *
 */
static void StrEncode( char* to, size_t tosize, const char* from )
{
    int tolen;

    for ( tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from ){
        if ( isalnum(*from) || strchr( "/_.-~", *from ) != (char*) 0 ){
            *to = *from;
            ++to;
            ++tolen;
        } else {
            (void) sprintf( to, "%%%02x", (int) *from & 0xff );
            to += 3;
            tolen += 3;
        }
    }
    *to = '\0';
}


/**
 * Decode a string 
 *
 */
static void StrDecode( char* to, char* from ){
    for ( ; *from != '\0'; ++to, ++from ){
        if ( from[0] == '%' && isxdigit( from[1] ) && isxdigit( from[2] ) ){
            *to = _hex( from[1] ) * 16 + _hex( from[2] );
            from += 2;
        } else {
            *to = *from;
        }
    }
    *to = '\0';
}



/*-------------------------------
 *
 *  C library function (not use)
 *
 *-------------------------------*/

/* strcat - string attach */
char *str_cat (char * dst, const char * src){ 
	char * cp = dst; 
	while( *cp ) cp++; 
	while( *cp++ = *src++ ) ;
	return( dst );
}

/* strlen - string length */
size_t str_len(const char *p){
    size_t n = 0;
    while (*p++) n++;
    return n;
}

/* strcmp - string compare */
int str_cmp(const char *s1, const char *s2){
    for (; *s1 && *s1 == *s2; s1++, s2++) ;
    return *s1 - *s2;
}

/* strncmp - string n compare */
int str_ncmp(const char *s1, const char *s2, size_t n){
    for (; --n && *s1 && *s1 == *s2; s1++, s2++) ;
    return *s1 - *s2;
}

/* strcpy - string copy */
char *str_cpy(char *dst, const char *src){
    char *p = dst;
    while (*dst++ = *src++) ;
    return p;
}

/* strncpy - string n copy */
char *str_ncpy(char *dst, const char *src, size_t n){
    char *p = dst;
    while (n && (*dst++ = *src++)) n--;
    if (!n) *dst = '\0';
    return p;
}

/* strstr */
char *str_str( const char * s1, const char * s2 ){
    const char * p1 = s1;
    const char * p2;
    while ( *s1 ){
        p2 = s2;
        while ( *p2 && ( *p1 == *p2 ) ){
            ++p1;
            ++p2;
        }
        if ( ! *p2 ) return (char *) s1;
        ++s1;
        p1 = s1;
    }
    return NULL;
}

/* strspn */
size_t str_spn( const char * s1, const char * s2 ){
    size_t len = 0;
    const char * p;
    while ( s1[ len ] ){
        p = s2;
        while ( *p ){
            if ( s1[len] == *p ) break;
            ++p;
        }
        if ( ! *p ) return len;
        ++len;
    }
    return len;
}

/* strcspn */
size_t str_cspn( const char * s1, const char * s2 ){
    size_t len = 0;
    const char * p;
    while ( s1[len] ){
        p = s2;
        while ( *p )
            if ( s1[len] == *p++ ) return len;
        ++len;
    }
    return len;
}

/* strpbrk */
char *str_pbrk( const char * s1, const char * s2 ){
    const char * p1 = s1;
    const char * p2;
    while ( *p1 ){
        p2 = s2;
        while ( *p2 )
            if ( *p1 == *p2++ ) return (char *) p1;
        ++p1;
    }
    return NULL;
}

/* strchr */
char *str_chr( const char * s, int c ){
    do{
        if ( *s == (char) c ) return (char *) s;
    } while ( *s++ );
    return NULL;
}

/* strrchr */
char *str_rchr( const char * s, int c ){
    size_t i = 0;
    while ( s[i++] );
    do{
        if ( s[--i] == (char) c ) return (char *) s + i;
    } while ( i );
    return NULL;
}

/* memcpy - memory copy */
void *mem_cpy(void *s, const void *s0, size_t n){
    char *s1 = s;
    const char *s2 = s0;
    while (n--) *s1++ = *s2++;
    return s;
}

/* memmove - memory move */
void *mem_move(void *s, const void *s0, size_t n){
    char *s1 = s;
    const char *s2 = s0;
    if (s1 <= s2){
        while (n--) *s1++ = *s2++;
    } else {
        s1 += n, s2 += n;
        while (n--) *--s1 = *--s2;
    }
    return s;
}

/* memset - memory set */
void *mem_set( void * s, int c, size_t n ){
    unsigned char * p = (unsigned char *) s;
    while ( n-- ) *p++ = (unsigned char) c;
    return s;
}

/* memcmp - memory compare */
int mem_cmp( const void * s1, const void * s2, size_t n ){
    const unsigned char * p1 = (const unsigned char *) s1;
    const unsigned char * p2 = (const unsigned char *) s2;
    while ( n-- ){
        if ( *p1 != *p2 ) return *p1 - *p2;
        ++p1;
        ++p2;
    }
    return 0;
}

/* memchr */
void *mem_chr( const void * s, int c, size_t n ){
    const unsigned char * p = (const unsigned char *) s;
    while ( n-- ){
        if ( *p == (unsigned char) c ) return (void *) p;
        ++p;
    }
    return NULL;
}

/* toupper - char to upper */
char to_upper(char c){
    return c >= 'a' && c <= 'z' ? c + 'A' - 'a' : c ;
}

/* tolower - char to lower */
char to_lower(char c){
    return c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c;
}



/********************************
 *
 *   Http Server Core Function
 *
 ********************************/

 
/**
 * Usage message
 *
 */
static void Usage(char *exefile){
	/* Print copyright information */
	fprintf(stderr, "#=======================================\n");
	fprintf(stderr, "# TieMa Mini Http Server\n");
	fprintf(stderr, "# Version %s\n", VERSION);
	fprintf(stderr, "# \n");
	fprintf(stderr, "# heiyeluren <blog.csdn.net/heiyeshuwu>\n");
	fprintf(stderr, "#=======================================\n\n");
    fprintf(stderr, "Usage: %s [OPTION] ... \n", exefile);

	/* Print options information */
	fprintf(stderr, "\nOptions: \n\
  -D, --is-deubg	Is open debug mode, default No\n\
  -d, --is-daemon	Is daemon running, default No\n\
  -p, --port=PORT	Server listen port, default 80\n\
  -m, --max-client=SIZE	Max connection requests, default 100\n\
  -L, --is-log		Is write access log, default No\n\
  -l, --log-path=PATH	Access log path, default /tmp/tmhttpd.log\n\
  -b, --is-browse	Is allow browse file/dir list, default No\n\
  -r, --doc-root=PATH	Web document root directory, default programe current directory ./\n\
  -i, --dir-index=FILE	Directory default index file name, default index.html\n\
  -h, --help		Print help information\n");

	/* Print example information */
	fprintf(stderr, "\nExample: \n\
  %s -d -p 80 -m 128 -L -l /tmp/access.log -b -r /var/www -i index.html\n\
  %s -d -p80 -m128 -L -l/tmp/access.log -b -r/var/www -iindex.html\n\
  %s --is-daemon --port=80 --max-client=128 --is-log --log-path=/tmp/access.log --is-browse --doc-root=/var/www --dir-index=index.html\n\n", exefile, exefile, exefile);

}

/**
 * Output environment and configure information
 *
 */
static void PrintConfig(){
	fprintf(stderr, "===================================\n");
	fprintf(stderr, " tmhttpd Configure information\n");
	fprintf(stderr, "===================================\n");
	fprintf(stderr, "Is-Debug\t = %s\n", g_is_debug ? "Yes" : "No");
	fprintf(stderr, "Is-Daemon\t = %s\n", g_is_daemon ? "Yes" : "No");
	fprintf(stderr, "Port\t\t = %d\n", g_port);
	fprintf(stderr, "Max-Client\t = %d\n", g_max_client);
	fprintf(stderr, "Is-Log\t\t = %s\n", g_is_log ? "Yes" : "No");
	fprintf(stderr, "Log-Path\t = %s\n", g_log_path);
	fprintf(stderr, "Is-Browse\t = %s\n", g_is_browse ? "Yes" : "No");
	fprintf(stderr, "Doc-Root\t = %s\n", g_doc_root);
	fprintf(stderr, "Dir-Index\t = %s\n", g_dir_index);
	fprintf(stderr, "===================================\n\n");
}

/**
 * Log message
 *
 */
static int WriteLog( const char *message ){
	if ( !g_is_log ){
		fprintf(stderr, "%s", message);
		return 0;
	}
	if ( g_log_fd == 0 ){
		if ( (g_log_fd = open(g_log_path, O_RDWR|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1 ){
			perror("open log file error");
			return -1;
		}
	}
	if (write(g_log_fd, message, strlen(message)) == -1){
		perror("write log error");
		return -1;
	}

	return 0;
}

/**
 * Send http header
 *
 */
static int SendHeaders(int client_sock, int status, char *title, char *extra_header, char *mime_type, off_t length, time_t mod ){
    time_t now;
    char timebuf[100], buf[BUFFER_SIZE], buf_all[REQUEST_MAX_SIZE], log[8];

	/* Make http head information */
    memset(buf, 0, strlen(buf));
    sprintf(buf, "%s %d %s\r\n", "HTTP/1.0", status, title);
    strcat(buf_all, buf);

    memset(buf, 0, strlen(buf));
    sprintf(buf, "Server: %s\r\n", SERVER_NAME);
    strcat(buf_all, buf);

    now = time( (time_t*) 0 );
    strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
    memset(buf, 0, strlen(buf));
    sprintf(buf, "Date: %s\r\n", timebuf);
    strcat(buf_all, buf);

   /* if (extra_header != (char*)0){
        memset(buf, 0, strlen(buf));
        sprintf(buf, "%s\r\n", extra_header);
        strcat(buf_all, buf);
    } */
    if (mime_type != (char*)0){
        memset(buf, 0, strlen(buf));
        sprintf(buf, "Content-Type: %s\r\n", mime_type);
        strcat(buf_all, buf);
    }
    if (length >= 0){
        memset(buf, 0, strlen(buf));
        sprintf(buf, "Content-Length: %lld\r\n", (int64_t)length);        
        strcat(buf_all, buf);
    }
    if (mod != (time_t) -1 ){
        memset(buf, 0, strlen(buf));
        strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &mod ) );
        sprintf(buf, "Last-Modified: %s\r\n", timebuf );
        strcat(buf_all, buf);
    }
    memset(buf, 0, strlen(buf));
    sprintf(buf, "Connection: close\r\n\r\n");
    strcat(buf_all, buf);

	/* Debug message */
	if ( g_is_debug ){
		fprintf(stderr, "[ Response ]\n");
		fprintf(stderr, "%s", buf_all);
	}

	/* Write http header to client socket */
    write(client_sock, buf_all, strlen(buf_all));

	return 0;
} 


/**
 * Send http error message 
 * 
 * @param int status
 * @param char* title
 * @param char* extra
 */
static int SendError(int client_sock, int status, char *title, char *extra_header, char *text ){
    char buf[BUFFER_SIZE], buf_all[REQUEST_MAX_SIZE];

	/* Send http header */
    SendHeaders( client_sock, status, title, extra_header, "text/html", -1, -1 );

	/* Send html page */
    memset(buf, 0, strlen(buf));
    sprintf(buf, "<html>\n<head>\n<title>%d %s - %s</title>\n</head>\n<body>\n<h2>%d %s</h2>\n", status, title, SERVER_NAME, status, title);
    strcat(buf_all, buf);

    memset(buf, 0, strlen(buf));
    sprintf(buf, "%s\n", text);
    strcat(buf_all, buf);

    memset(buf, 0, strlen(buf));
    sprintf(buf, "\n<br /><br /><hr />\n<address>%s</address>\n</body>\n</html>\n", SERVER_NAME);
    strcat(buf_all, buf);

	/* Write client socket */
    write(client_sock, buf_all, strlen(buf_all));
	exit(0);
}

/**
 * Send file contents to client
 *
 */
static int SendFile( int client_sock, char *filename, char *pathinfo ){
    char buf[128], contents[8192], mime_type[64];
    int fd;
	size_t file_size;

    /* Get mime type & send http header information */
    mime_content_type(filename, mime_type);
    SendHeaders( client_sock, 200, "OK", "", mime_type, filesize(filename), 0);

    /* Open file */
    if ( (fd = open(filename, O_RDONLY)) == -1 ){
        memset(buf, '\0', sizeof(buf));
        sprintf(buf, "Something unexpected went wrong read file %s.", pathinfo);
        SendError( client_sock, 500, "Internal Server Error", "", buf);
    }

	/* Read static file write to client socket (File size less 8192 bytes) */
	file_size = filesize(filename);
	if ( file_size < 8192){
		read(fd, contents, 8192);
		write( client_sock, contents, file_size );

	} else {
		while ( read(fd, contents, 8192) > 0 ){
			write( client_sock, contents, 8192 );
			memset(contents, '\0', sizeof(contents));
		}
	}

    /* Close file descriptor */
    close( fd );

    /* Debug message */
    if (g_is_debug){
        printf("request filename: %s\n", filename);
        printf("request pathinfo: %s\n", pathinfo);
    }

	return 0;
}

/**
 * Send directory index list to client
 *
 */
static int SendDirectory( int client_sock, char *path, char *pathinfo ){
	size_t file_size;
	char msg[128], buf[10240], tmp[1024], tmp_path[1024];
	DIR *dp;
	struct dirent *node;

	/* Open directory */
	if ( (dp = opendir(path)) == NULL ){
		memset(buf, 0, sizeof(msg));
		sprintf(msg, "Something unexpected went wrong browse directory %s.", pathinfo);
		SendError( client_sock, 500, "Internal Server Error", "", msg);
	}

	/* Check pathinfo is valid */
	memset(tmp_path, 0, sizeof(tmp_path));
	substr(pathinfo, -1, 1, tmp_path);
	if ( strcmp(tmp_path, "/") == 0){
		memset(tmp_path, 0, sizeof(tmp_path));
		strcpy(tmp_path, pathinfo);
	} else {
		memset(tmp_path, 0, sizeof(tmp_path));
		strcpy(tmp_path, pathinfo);	
		strcat(tmp_path, "/");
	}

	/* Fetch directory list */
	sprintf( tmp, "<html>\n<head>\n<title>Browse directory %s - %s</title>\n</head>\n<body><h3>Directory %s</h3>\n<hr />\n<ul>\n", pathinfo, SERVER_NAME, pathinfo );
	strcat(buf, tmp);

	while ( (node = readdir(dp)) != NULL){
		if ( strcmp(node->d_name, ".") != 0){
			memset(tmp, 0, sizeof(tmp));
			sprintf(tmp, "\t<li><a href=\"%s%s\">%s</a></li>\n", tmp_path, node->d_name, node->d_name);
			strcat(buf, tmp);
		}
	}
    memset(tmp, 0, sizeof(tmp));
    sprintf(tmp, "\n</ul>\n<br /><hr />\n<address>%s</address>\n</body>\n</html>\n", SERVER_NAME);
    strcat(buf, tmp);

	file_size = strlen(buf);
	SendHeaders( client_sock, 200, "OK", "", "text/html", file_size, 0 );
	write( client_sock, buf, file_size );

	return 0;
}


/**
 * Process request
 *
 */
static int ProcRequest( int client_sock, struct sockaddr_in client_addr, struct st_request_info request_info ){
	char buf[128];

	/* File is exist or has access permission */
	if ( !file_exists( request_info.physical_path ) ){
		memset(buf, 0, sizeof(buf));
		sprintf(buf, "File %s not found.", request_info.pathinfo);
		SendError( client_sock, 404, "Not Found", "", buf);
	}
	if ( access(request_info.physical_path, R_OK) != 0 ){
		memset(buf, 0, sizeof(buf));
		sprintf(buf, "File %s is protected.", request_info.pathinfo);
		SendError( client_sock, 403, "Forbidden", "", buf);
	}

	/* Check target is regular file or directory */
	if ( is_file(request_info.physical_path) == 1 ){
		SendFile( client_sock,  request_info.physical_path, request_info.pathinfo );

	} else if ( is_dir(request_info.physical_path) == 1 ){ 
		/* Is a directory choose browse dir list */
		if ( g_is_browse ){
			SendDirectory( client_sock, request_info.physical_path, request_info.pathinfo );

		} else {
			memset(buf, 0, sizeof(buf));
			sprintf(buf, "File %s is protected.", request_info.pathinfo);
			SendError( client_sock, 403, "Forbidden", "", buf);
		}

	} else {
		memset(buf, 0, sizeof(buf));
		sprintf(buf, "File %s is protected.", request_info.pathinfo);
		SendError( client_sock, 403, "Forbidden", "", buf);		
	}
	
	/*
		printf("method: %s\n", request_info.method);
		printf("pathinfo: %s\n", request_info.pathinfo);
		printf("query: %s\n", request_info.query);
		printf("protocal: %s\n", request_info.protocal);
		printf("path: %s\n", request_info.path);
		printf("file: %s\n", request_info.file);
		printf("physical_path: %s\n", request_info.physical_path);
	*/

	return 0;
}




/**
 * Parse client request
 *
 */
static int ParseRequest( int client_sock, struct sockaddr_in client_addr, char *req ){
	char **buf, **method_buf, **query_buf, currtime[32], cwd[1024], tmp_path[1536], pathinfo[512], path[256], file[256], log[1024];
	int line_total, method_total, query_total, i;
	struct st_request_info request_info;

	/* Split client request */
	getdate(currtime);
	explode(req, '\n', &buf, &line_total);

	/* Print log message  */
	memset(log, 0, sizeof(log));
	sprintf(log, "[%s] %s %s\n", currtime, inet_ntoa(client_addr.sin_addr), buf[0]);
	WriteLog(log);

	/* Check request is empty */
	if (strcmp(buf[0], "\n") == 0 || strcmp(buf[0], "\r\n") == 0){
		SendError( client_sock, 400, "Bad Request", "", "Can't parse request." );
	}

	/* Check method is implement */
	explode(buf[0], ' ', &method_buf, &method_total);
	if ( strcmp( strtolower(method_buf[0]), "get") != 0 &&  strcmp( strtolower(method_buf[0]), "head") != 0 ){
		SendError( client_sock, 501, "Not Implemented", "", "That method is not implemented." );
	}
	explode(method_buf[1], '?', &query_buf, &query_total);

	/* Make request data */	
	getcwd(cwd, sizeof(cwd));
	strcpy(pathinfo, query_buf[0]);
	substr(query_buf[0], 0, strrpos(pathinfo, '/')+1, path);
	substr(query_buf[0], strrpos(pathinfo, '/')+1, 0, file);
	
	/* Pad request struct */
	memset(&request_info, 0, sizeof(request_info));
	strcat(cwd, pathinfo);

	request_info.method			= method_buf[0];
	request_info.pathinfo		= pathinfo;
	request_info.query			= (query_total == 2 ? query_buf[1] : "");
	request_info.protocal		= strtolower(method_buf[2]);
	request_info.path			= path;
	request_info.file			= file;
	request_info.physical_path	= cwd;

	/* Is a directory pad default index page */
	memset(tmp_path, 0, sizeof(tmp_path));
	strcpy(tmp_path, cwd);
	if ( is_dir(tmp_path) ){
		strcat(tmp_path, g_dir_index);
		if ( file_exists(tmp_path) ){
			request_info.physical_path = tmp_path;
		}
	}
	
	/* Debug message */
	if ( g_is_debug ){
		fprintf(stderr, "[ Request ]\n");
		for(i=0; i<line_total; i++){
			fprintf(stderr, "%s\n", buf[i]);
		}	
		/*
		for(i=0; i<method_total; i++){
			printf("[%s] %s method at %d: %s\n", currtime, inet_ntoa(client_addr.sin_addr), i, method_buf[i]);
		}
		for(i=0; i<query_total; i++){
			printf("[%s] %s query at %d: %s\n", currtime, inet_ntoa(client_addr.sin_addr), i, query_buf[i]);
		}
		printf("[%s] %s parse-method: %s\n", currtime, inet_ntoa(client_addr.sin_addr),  request_info.method);
		printf("[%s] %s parse-pathinfo: %s\n", currtime, inet_ntoa(client_addr.sin_addr),  request_info.pathinfo);
		printf("[%s] %s parse-query: %s\n", currtime, inet_ntoa(client_addr.sin_addr),  request_info.query);
		printf("[%s] %s parse-protocal: %s\n", currtime, inet_ntoa(client_addr.sin_addr),  request_info.protocal);
		printf("[%s] %s parse-path: %s\n", currtime, inet_ntoa(client_addr.sin_addr),  request_info.path);
		printf("[%s] %s parse-file: %s\n", currtime, inet_ntoa(client_addr.sin_addr),  request_info.file);
		printf("[%s] %s parse-physical_path: %s\n", currtime, inet_ntoa(client_addr.sin_addr),  request_info.physical_path);
		*/
	}

	/* Process client request */
	ProcRequest( client_sock, client_addr, request_info );

	return 0;
}



/**
 * Handle a client
 *
 */
static void HandleClient( int client_sock, struct sockaddr_in client_addr ){
    char buf[REQUEST_MAX_SIZE];

	if ( read(client_sock, buf, REQUEST_MAX_SIZE) < 0){
		SendError( client_sock, 500, "Internal Server Error", "", "Client request not success." );
		die("read sock");
	}
	ParseRequest( client_sock, client_addr, buf );

    close(client_sock);
    exit(0);

	/*
	char buf[REQUEST_MAX_SIZE], req[REQUEST_MAX_SIZE];
    int  nbyte, i = 0, r = 1, flag = -1, received = -1;
	//if (fcntl(client_sock, F_SETFL, O_NONBLOCK) == -1){
	//	die("HandleClient");
	//}
    //(void) setsockopt(client_sock, IPPROTO_TCP, TCP_NOPUSH, (void*) &r, sizeof(r) );
	
	while((nbyte = read(client_sock, buf, REQUEST_MAX_SIZE)) > 0){
		if (strcmp(buf, "\n") == 0 || strcmp(buf, "\r\n") == 0)	i++;
		else i = 0;
		if (i == 1)	break;
        strcat(req, buf); 
        memset(buf, 0, strlen(buf));
    } 	
    memset(request, 0, REQUEST_MAX_SIZE);
    memcpy(request, req, strlen(req));
	ParseRequest(client_sock, req, client_addr);
    close(client_sock);
    exit(0);
	*/    


    ////while(1){
    //    /* Receive message */
    //    if ((nbyte = recv(sock, buffer, BUFFER_SIZE, 0)) < 0){
    //        die("Failed to read bytes from client");
    //    }
    //    /* Send bytes and check for more incoming data in loop */
    //    if ((nbyte = send(sock, buffer, nbyte, 0)) < 0){
    //        die("Failed to write bytes to client");            
    //    }
    //}

}


/**
 * Initialize server socket listen
 *
 */
static void InitServerListen( unsigned int port, unsigned int max_client ){
    int serversock, clientsock;
    struct sockaddr_in server_addr, client_addr;
	char currtime[32];

    /* Create the TCP socket */
    if ((serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
        die("Failed to create socket");
    }

    /* Construct the server sockaddr_in structure */
    memset(&server_addr, 0, sizeof(server_addr));       /* Clear struct */
    server_addr.sin_family = AF_INET;                  /* Internet/IP */
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);   /* Incoming addr */
    server_addr.sin_port = htons(port);          /* server port */

    /* Bind the server socket */
    if (bind(serversock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
        die("Failed to bind the server socket");
    }
    /* Listen on the server socket */
    if (listen(serversock, max_client) < 0){
        die("Failed to listen on server socket");
    }

	/* Print listening message */
	getdate(currtime);
	fprintf(stdout, "[%s] Start server listening at port %d ...\n", currtime, port);
	fprintf(stdout, "[%s] Waiting client connection ...\n", currtime);


    /* Run until cancelled */
    while (1){
        unsigned int clientlen = sizeof(client_addr);
		memset(currtime, 0, sizeof(currtime));
		getdate(currtime);

        /* Wait for client connection */
        if ((clientsock = accept(serversock, (struct sockaddr *) &client_addr, &clientlen)) < 0){
            die("Failed to accept client connection");
        }

		/* Debug message */
		/* if ( g_is_debug ){
			fprintf(stdout, "[%s] Client connected: %s\n", currtime, inet_ntoa(client_addr.sin_addr));
		} */

        /* Use child process new connection */
        if ( fork() == 0 ){
			HandleClient(clientsock, client_addr);
		} else {
			wait(NULL); 
		}

		/* Not use close socket connection */
        close(clientsock);
    }

}

/** 
 * Parse cmd options
 *
 */
static int ParseOptions( int argc, char *argv[] ){
	int opt;
	struct option longopts[] = {
		{ "is-debug",	0, NULL, 'D' },
		{ "is-daemon",	0, NULL, 'd' },
		{ "port",		1, NULL, 'p' },
		{ "max-client",	1, NULL, 'm' },
		{ "is-log",		0, NULL, 'L' },
 		{ "log-path",	1, NULL, 'l' },
		{ "is-browse",	0, NULL, 'b' },
		{ "doc-root",	1, NULL, 'r' },
		{ "dir-index",	1, NULL, 'i' },
		{ "help",		0, NULL, 'h' },
		{ 0,			0, 0,	 0   }
	};

	/* Parse every options */
	while ( (opt = getopt_long(argc, argv, ":Ddp:m:Ll:br:i:h", longopts, NULL)) != -1){
		switch (opt){
			case 'h': 
				Usage(argv[0]); 
				return(-1);
				break;
			case 'D': g_is_debug = 1; break;
			case 'd': g_is_daemon = 1; break;
			case 'p':
				g_port = atoi(optarg);
				if ( g_port < 1 || g_port > 65535 ){
					fprintf(stderr, "Options -p,--port error: input port number %s invalid, must between 1 - 65535.\n\n", optarg);
					return(-1);
				}
				break;
			case 'm':
				g_max_client = atoi(optarg);
				if ( !isdigit(g_max_client) ){
					fprintf(stderr, "Options -m,--max-client error: input clients %s invalid, must number, proposal between 32 - 2048.\n\n", optarg);
					return(-1);
				}
				break;
			case 'L': g_is_log = 1; break;
			case 'l':
				strcpy(g_log_path, optarg);
				if ( !file_exists(g_log_path) || !is_dir(g_log_path) ){
					fprintf(stderr, "Options -l,--log-path error: input path %s not exist or not a directory.\n\n", optarg);
					return(-1);					
				}
				break;
			case 'b': g_is_browse = 1; break;
			case 'r':
				strcpy(g_doc_root, optarg);
				if ( !file_exists(g_doc_root) || !is_dir(g_doc_root) ){
					fprintf(stderr, "Options -l,--log-path error: input path %s not exist or not a directory.\n\n", optarg);
					return(-1);					
				}
				break;
			case 'i':
				strcpy(g_dir_index, optarg);
				break;
		}
	}

	return(0);
}



/********************************
 *
 *   Http Server running
 *
 ********************************/


/**
 * Main function 
 *
 */
int main( int argc, char *argv[] ){

	/* Parse cli input options */
	if ( argc > 1 ){
		if ( ParseOptions( argc, argv ) != 0 ){
			//Usage(argv[0]);
			exit(-1);
		}
	}

	/* Set work directory */
	chdir(g_doc_root);
	
	/* Set is daemon mode run */
	if ( g_is_daemon ){
		pid_t pid;
		if ( (pid = fork()) < 0 ){
			die("daemon fork error");
		} else if ( pid != 0){
			exit(1);
		}
	}

	/* Debug mode out configure information */
	if ( g_is_debug ){
		PrintConfig();
	}

	/* Start server listen */
	InitServerListen( g_port, g_max_client );
	

}


/* End of file */
