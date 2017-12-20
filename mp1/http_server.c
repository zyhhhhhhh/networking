/*cs438 spring2016 http_server.c*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#define MAX_SIZE 1024
#define MAXDATASIZE 5000 // max number of bytes we can get at once 
#define BACKLOG 10

void sigchld_handler(int s)
{
	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;
	
	while(waitpid(-1, NULL, WNOHANG) > 0);
	
	errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int main(int argc, char **argv){
	char buf[MAXDATASIZE];
	int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP
	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}
		break;
	}
	
	freeaddrinfo(servinfo); // all done with this structure
	
	if (p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}
	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}
	
	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
	
	printf("server: waiting for connections...\n");
	
	while(1) { // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}
		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
		printf("server: got connection from %s\n", s);
		
		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
			//start receive
			int numbytes;
			if ((numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1) {
				perror("recv");
				exit(1);
			}
			//end receive in the buf

			//parse the request
			char * tok = NULL;
			tok = strstr(buf, "GET /");
			//check GET
			if (tok != buf){
				sprintf(buf, "HTTP/1.0 400 Bad Request\r\n\r\n");
				if (send(new_fd, buf, strlen(buf), 0) == -1){
					perror("send");
					exit(1);
				}
				printf("Wrong format of GET\n");
				close(new_fd);
				exit(1);
			}

			//check http
			// tok = strstr(buf, "HTTP/1.0");
			// if (tok == NULL){
			// 	sprintf(buf, "HTTP/1.0 400 Bad Request\r\n\r\n");
			// 	if (send(new_fd, buf, strlen(buf), 0) == -1){
			// 		perror("send");
			// 		exit(1);
			// 	}
			// 	printf("Wrong HTTP\n");
			// 	close(new_fd);
			// 	exit(1);
			// }
			if (strstr(buf,"HTTP/") == NULL || strstr(buf,"\r\n\r\n" )== NULL){
				sprintf(buf, "HTTP/1.0 400 Bad Request\r\n\r\n");
				if (send(new_fd, buf, strlen(buf), 0) == -1){
					perror("send");
					exit(1);
				}
				printf("Wrong termination\n");
				close(new_fd);
				exit(1);
			}
			//get the file path
			// char path [MAXDATASIZE];
			// printf("%s\n", buf);
			char * tok1 = NULL;
			tok = strstr(buf, "GET /")+5;
			tok1 = strstr(buf, "HTTP/")-1;
			int pathlen = tok1-tok;
			char path [pathlen+1];
			// printf("len = %d\n",pathlen);
			strncpy(path, tok, pathlen);
			path[pathlen] = '\0';
			printf("path:%s\n", path);
			//check file path
			FILE* fd_read;
			fd_read = fopen(path, "rb");
			if (fd_read == 0){
				sprintf(buf, "HTTP/1.0 404 Not Found\r\n\r\n");
				if (send(new_fd, buf, strlen(buf), 0) == -1){
					perror("send");
					exit(1);
				}
				printf("Wrong file path\n");
				close(new_fd);
				exit(1);
			}

			//end parsing the request

			//start transmit
			int header_flag = 0;
			int bytes_read =0;
		    do{
		    	memset(buf, '\0', MAXDATASIZE);
		    	if (header_flag == 0){
					sprintf(buf, "HTTP/1.0 200 OK\r\n\r\n");
					bytes_read = fread(buf+strlen(buf), sizeof(char), MAXDATASIZE-1-strlen("HTTP/1.0 200 OK\r\n\r\n"), fd_read);
					header_flag = 1;
					if (send(new_fd, buf, bytes_read+strlen("HTTP/1.0 200 OK\r\n\r\n"), 0) == -1){
						perror("send");
						exit(1);
					}
				}
				else{
					bytes_read = fread(buf, sizeof(char), MAXDATASIZE-1, fd_read);
					if (bytes_read ==0){
						break;
					}
					if (send(new_fd, buf, bytes_read, 0) == -1){
						perror("send");
						exit(1);
					}
				}
				

				
			} while (bytes_read != 0);
			fclose(fd_read);
			exit(0);
		}
		close(new_fd); //parent doesn't need this
		
	}
	return 0;
}
