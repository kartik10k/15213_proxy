#include <stdio.h>
#include <stdlib.h>
#include "csapp.h"
#include "cache.h"

/* Recommended max cache and object sizes */

/* You won't lose style points for including these long lines in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";
static const char *default_http_version = "HTTP/1.0\r\n";

static sem_t mutex;
pthread_rwlock_t rwlock;
static cache_list *cache_inst;

void doit(int fd);
int generate_request(rio_t *rp, char *i_request, char *i_host, char * i_uri, int *i_port);
int parse_reqline(char *new_request, char *reqline, char *host, char *uri, int *port);
int parse_uri(char *uri, char *host, int *port, char *uri_nohost);
void get_key_value(char *header_line, char *key, char *value);
void get_host_port(char *value, char *host, int *port);
void *thread(void* vargp);

/* Customized response func */
void client_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/* Customized r/w func and error handler wrapper */
int iOpen_clientfd_r(int fd, char *hostname, int port);
ssize_t iRio_readnb(int fd, char *hostname, rio_t *rp, void *usrbuf, size_t n);
int iRio_writen(int fd, void *usrbuf, size_t n);
void iClose(int fd);

int main(int argc, char **argv) {
    int listenfd, port, clientlen;
    int *connfdp;
    struct sockaddr_in clientaddr;
    pthread_t tid;

    Sem_init(&mutex, 0, 1);
    pthread_rwlock_init(&rwlock, NULL);

    cache_inst = (cache_list *)Malloc(sizeof(cache_list));
    init_cache_list(cache_inst);

    /* Check command line args number. */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    port = atoi(argv[1]);

    /* Ignore SIGPIPE signal */
    Signal(SIGPIPE, SIG_IGN);

    listenfd = Open_listenfd(port);

    printf("listen port open, with fd: %d\n", listenfd);
    while (1) {
        printf("------In listening loop--------\n");
        clientlen = sizeof(clientaddr);
        connfdp = (int *)Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);
        printf("Connection made\n");
        Pthread_create(&tid, NULL, thread, connfdp);
        printf("Connection broken\n");
    }
    
    return 0;
}

void doit(int fd) {
    rio_t client_rio;
    rio_t server_rio;
    cache_block* cache = NULL;
    unsigned int total = 0;
    int fit_size = 1;

    printf("---------In doit--------\r\n");
    char* uri = (char*)Malloc(MAXLINE * sizeof(char));
    char *request = (char *)Malloc(MAXLINE * sizeof(char));
    char *host = (char *)Malloc(MAXLINE * sizeof(char));
    int port;
    int server_fd;

    Rio_readinitb(&client_rio, fd);

    int is_get = generate_request(&client_rio, request, host, uri, &port);   
    if(!is_get) {
        Free(request);
        Free(host);
        Free(uri);
        return;
    }

          
    printf("request: %s, host: %s, uri: %s. port: %d\n", request, host, uri, port);

    //first: find in cache
    cache = find_cache(cache_inst, uri);
    if (cache != NULL ) //cache hit
    {
        printf("request find in cache! uri: %s\n", uri);
        iRio_writen(fd, cache->content, cache->block_size);
        Free(request);
        Free(host);
        Free(uri);
        printf("out do it\n");
        return;
    }   

    //cache miss
    server_fd = iOpen_clientfd_r(fd, host, port);
    if (server_fd < 0) {   //open connection error
        Free(request);
        Free(host);
        Free(uri);
        return;
    }
        
    Rio_readinitb(&server_rio, server_fd);

    int server_connect = 0;
    server_connect = iRio_writen(server_fd, request, strlen(request));
    if (server_connect < 0) {
        iClose(server_fd);
        Free(request);
        Free(host);
        Free(uri);
        return;
    }

    /*Forward response from the server to the client through connfd*/
    ssize_t nread;
    char buf[MAXLINE];
    char content[MAX_OBJECT_SIZE];

    while ((nread = iRio_readnb(fd, host, &server_rio, buf, MAXLINE)) != 0) {
        if(nread < 0) {
            Free(request);
            Free(host);
            Free(uri);
            return;
        }

        if ((total + nread) < MAX_OBJECT_SIZE){
            memcpy(content + total, buf, sizeof(char) * nread);
            total += nread;
        }else{
            printf("web content object is too lage!\n");
            fit_size = 0;
        }
        
        iRio_writen(fd, buf, nread);
    }

    iClose(server_fd);

    if (fit_size == 1){
        if (strstr(uri, "?") != NULL){
            printf("do not cache this web content uri:%s\n", uri);
        }else{
            printf("cache the web content object uri: %s\n", uri);
            modify_cache(cache_inst, uri, content, total);
        }
    }

    printf("---------Out doit--------\r\n");

    Free(request);
    Free(host);
    Free(uri);
    return;
}

int generate_request(rio_t *rp, char *i_request, char *i_host, char* i_uri, int *i_port) {
    char buf[MAXLINE]; 
    char key[MAXLINE];
    char value[MAXLINE];
    char raw[MAXLINE]; 
    int port = 80;
    int host_in_reqbody = 0; 
    char* request = i_request;             // check the weird pointer/array assignment here!!!!
    char* host = i_host;
    char* uri = i_uri;

    printf("-----------In generate_request--------\n");
    *buf = 0;
    *key = 0;
    *value = 0;
    *raw = 0;
    *request = 0;
    *host = 0;

    if (rio_readlineb(rp, buf, MAXLINE) < 0){
        printf("rio_readlineb error\n");
        return 0;
    }

    strcat(raw, buf);

    int is_get = parse_reqline(request, buf, host, uri, &port);

    if(!is_get)
        return 0;
    strcat(request, user_agent_hdr);
    strcat(request, accept_hdr);
    strcat(request, accept_encoding_hdr);
    strcat(request, connection_hdr);
    strcat(request, proxy_connection_hdr);

    printf("request with required header: %s\n", request);
    printf("After parse_reqline, host: %s, port: %d\n", host, port);    
    
    while (strcmp(buf, "\r\n")) {                     // !!!!!!!! \r\n VS \n !!!!!!!!!!!
        printf("cmp result: %d\n", strcmp(buf, "\r\n"));
        *key = '\0';
        *value = '\0';
        if (rio_readlineb(rp, buf, MAXLINE) < 0){
            printf("rio_readlineb error\n");
            return 0;
        }
        printf("buffer: %s\n", buf);
        strcat(raw, buf);

        if (!strcmp(buf, "\r\n"))
            break;

        /* Extract one key-value pair from one header line */
        get_key_value(buf, key, value);
        printf("key: %s, value: %s\n", key, value);
        if (*key != '\0' && *value!='\0') {
            /* If the request body has Host header itself, use it */
            if (!strcmp(key, "Host")) {
                get_host_port(value, host, &port);
                host_in_reqbody = 1;
            }
            if (strcmp(key, "User-Agent") && 
                    strcmp(key, "Accept") && 
                    strcmp(key, "Accept-Encoding") &&
                    strcmp(key, "Connection") &&
                    strcmp(key, "Proxy-Connection")) {

                char hdrline[MAXLINE];
                sprintf(hdrline, "%s: %s\r\n", key, value);
                strcat(request, hdrline);
            }
        }
    }
    /*
     * If request doesn't have a Host header, 
     * combine the host and port from the 
     * first request line as the Host header
     */
    if (!host_in_reqbody) {
        char host_hdr[MAXLINE];
        if (port != 80)
            sprintf(host_hdr, "Host: %s:%d\r\n", host, port);
        else 
            sprintf(host_hdr, "Host: %s\r\n", host);

        strcat(request, host_hdr);
    }

    *i_port = port;

    strcat(request, "\r\n");
    printf("request finished: %s\n", request);
    printf("raw request: %s\n", raw);
    printf("-----------Out generate_request--------\n");

    return 1;
}

int parse_reqline(char *new_request, char *reqline, char *host, char* uri, int *port) {
    char method[MAXLINE], version[MAXLINE];
    char uri_nohost[MAXLINE];
    char new_req[MAXLINE];
    
    printf("---------In parse_reqline--------\n");
    sscanf(reqline, "%s %s %s", method, uri, version);
    if(strcasecmp(method, "GET"))
        return 0;
    parse_uri(uri, host, port, uri_nohost);

    sprintf(new_req, "%s %s %s", method, uri_nohost, default_http_version);   // !!!!! if not GET
    strcat(new_request, new_req);
    printf("reqline: %s, host: %s, port: %d, new_request: %s\n", reqline, host, *port, new_request);
    printf("---------Out parse_reqline--------\n");
    return 1;
}

int parse_uri(char *uri, char *host, int *port, char *uri_nohost) {
    char *uri_ptr, *first_slash_ptr;
    char host_str[MAXLINE], port_str[MAXLINE];
    printf("---------In parse_uri--------\n");
    *host = 0;
    *port = 80;

    uri_ptr = strstr(uri, "http://");        

    if (uri_ptr == NULL) {
        strcpy(uri_nohost, uri);
        printf("---------Out parse_uri--------\n");
        return 0;
    } else {
        uri_ptr += 7;

        first_slash_ptr = strchr(uri_ptr,'/');
        *first_slash_ptr = 0;

        strcpy(host_str, uri_ptr);

        *first_slash_ptr = '/';
        strcpy(uri_nohost, first_slash_ptr);

        char *port_ptr;
        port_ptr = strstr(host_str, ":");
        if (port_ptr != NULL) {
                *port_ptr = 0;
                strcpy(port_str, port_ptr + 1);
                *port = atoi(port_str);
        }

        strcpy(host, host_str);
        printf("uri: %s, host: %s, port: %d, uri_nohost: %s\n", uri, host, *port, uri_nohost);
        printf("---------Out parse_uri--------\n");
        return 1;
    }
}

void get_key_value(char *header_line, char *key, char *value) {
    char *key_tail, *value_tail;
    printf("---------In get_key_value--------\n");

    /* Split the given header line by ":" */
    key_tail = strstr(header_line, ":");
    if (key_tail != NULL) {
        /* Get the key part */
        *key_tail = 0;
        strcpy(key, header_line);
        printf("key: %s\n", key);
        *key_tail = ':';

        /* Find the "\r\n" and get the value accordingly */
        value_tail = strstr(header_line, "\r");         // !!!!!!! what should be used here !!!!!
        printf("I've find the pointer: %p\n", (void *)value_tail);
        *value_tail = 0; 
        strcpy(value, key_tail + 2);
        *value_tail = '\r'; 
        printf("value: %s\n", value);
    } 
    printf("---------Out get_key_value--------\n");
    return;
}

void get_host_port(char *value, char *host, int *port) {
    char *host_tail;
    printf("---------In get_host_port--------\n");
    *port = 80;

    host_tail = strstr(value, ":");
    if (host_tail != NULL) {
        *host_tail = 0;
        strcpy(host, value);
        *port = atoi(host_tail + 1);
        *host_tail = ':';
    }
    printf("---------Out get_host_port--------\n");
    return;
}

void *thread(void* vargp) {
    int connfd = *((int *)vargp);
    Pthread_detach(Pthread_self());
    Free(vargp);
    doit(connfd);
    iClose(connfd);
    return NULL;
}

/* Customized r/w func and error handler wrapper */
int iRio_writen(int fd, void *usrbuf, size_t n) {
    if (rio_writen(fd, usrbuf, n) != n) {
        if (errno == EPIPE || errno == ECONNRESET) {
            printf("Server closed %s\n", strerror(errno));
            return -1;
        } 
        else
            unix_error("Rio_writen error");
    }

    return 0;
}

int iOpen_clientfd_r(int fd, char *hostname, int port) {
    int rc;

    if ((rc = open_clientfd_r(hostname, port)) < 0)
        client_error(fd, hostname, "404", "Not found",
            "Proxy couldn't connect to this server");

    return rc;
}

ssize_t iRio_readnb(int fd, char *hostname, rio_t *rp, void *usrbuf, size_t n) {
    ssize_t rc;

    if ((rc = rio_readnb(rp, usrbuf, n)) < 0)
        client_error(fd, hostname, "404", "Not found",
            "Proxy couldn't connect to this server");

    return rc;
}

void iClose(int fd){
    if (close(fd) < 0)
        printf("fd close error\n");
}

void client_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXLINE];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Request Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The proxy</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}