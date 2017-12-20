/*cs438 spring2016 http_client.c*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_SIZE 1024
#define MAXDATASIZE 5000 // max number of bytes we can get at once 
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char **argv){
	int sockfd, numbytes;
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	
	if (argc != 2) {
		fprintf(stderr,"usage: client hostname\n");
		exit(1);
	}
	//parse input
	//find http
	char * tok = NULL;
	tok = strtok(argv[1], ":");
	tok[strlen(tok)] = '\0';
	printf("protocal:%s\n", tok);
	if (strcmp(tok, "http")!=0){
		printf("not http connection\n");
		exit(1);
	}
	//find host
	tok = strtok(NULL, "/");
	printf("%s\n", tok);
	char host [MAX_SIZE];
	sprintf(host, "%s", tok);
	printf("host: %s\n",host);
	//path
	char path[MAX_SIZE];
	tok = strtok(NULL, "\0");
	// strcpy(path, tok);
	if(tok!=NULL)
		sprintf(path, "%s", tok);
	else
		sprintf(path, "index.html");
	printf("path:%s\n", path);

	//find port
	tok = strtok(host, ":");
	tok = strtok(NULL, "\0");
	char port[MAX_SIZE];
	if (tok == NULL)
		sprintf(port, "80");
	else
		sprintf(port,"%s",tok);
	printf("port: %s\n", port);

	//finish parsing input 

	
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	
	if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}
		break;
	}
	
	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}
	
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);
	freeaddrinfo(servinfo); // all done with this structure

	//send request
	sprintf(buf, "GET /%s HTTP/1.0\r\n", path);
    sprintf(buf + strlen(buf), "User-Agent: Wget/1.15 (linux-gnu)\r\n");
    sprintf(buf + strlen(buf), "Accept: */*\r\n");
    sprintf(buf + strlen(buf), "Host: %s:%s\r\n", host, port);
    sprintf(buf + strlen(buf), "Connection: Keep-Alive\r\n\r\n");
    if ((numbytes = send(sockfd, buf, strlen(buf), 0)) == -1) {
            perror("send");
            exit(1);
    }

    //finish send 

    //start receive
    FILE* fd_write;
    fd_write = fopen("output", "wb");
    int header_flag = 0;
    do{	
    	memset(buf, '\0', MAXDATASIZE);
		if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
			perror("recv");
			exit(1);
		}
		printf("RECEIVED: %d bytes\n",numbytes);
		//received bad request 
		if(strstr(buf, "HTTP/1.0 400 Bad Request\r\n\r\n")!= NULL){
			fwrite("HTTP/1.0 400 Bad Request", sizeof(char), strlen("HTTP/1.0 400 Bad Request"), fd_write);
			break;
		}
		//received not found 
		if(strstr(buf, "HTTP/1.0 404 Not Found\r\n\r\n")!= NULL){
			fwrite("HTTP/1.0 404 Not Found", sizeof(char), strlen("HTTP/1.0 404 Not Found"), fd_write);
			break;
		}
		if (header_flag == 0){
			tok = strstr(buf,"\r\n\r\n")+4;
			fwrite(tok, sizeof(char), numbytes+buf-tok, fd_write);
			header_flag = 1;
		}
		else
			fwrite(buf, sizeof(char), numbytes, fd_write);
	} while (numbytes !=0);
	fclose(fd_write);
	close(sockfd);
	return 0;
}
