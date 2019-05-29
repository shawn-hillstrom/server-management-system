/* CS 360 (Systems Programming) -- Final Project
 * 	written by Shawn Hillstrom
 * ---------------------------------------------
 * Server implementation.
 */

#include "mftp.h"

/* Function: checkerr
 * ------------------
 * Checks a given function return value against it's known error value
 *	and prints an error message to stderr if they are the same.
 *
 * fret: function return value.
 * eval: known error value.
 * msg: error message to print.
 *
 * returns: void.
 */
void checkerr(int fret, int eval, char * msg) {
	if (fret == eval) {
		fprintf(stderr, "%s: %s\n", msg, strerror(errno));
		exit(1);
	}
}

/* Function: establishsocket
 * -------------------------
 * Establishes a new passive socket with a specified port number.
 *
 * port: port number for the socket.
 * backlog: backlog for the passive socket.
 *
 * returns: file descriptor for the new socket.
 */
int establishsocket(unsigned short port, int backlog) {

	/* Create socket. */
	int socketfd = socket(AF_INET, SOCK_STREAM, 0);
	checkerr(socketfd, -1, "socket (Server: establishsocket)");

	/* Set the family, port number, and address for the socket. */
	struct sockaddr_in servAddr;
	memset(&servAddr, 0, sizeof(servAddr)); // Zero out servAdr.
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(port);
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	/* Bind the socket. */
	checkerr(bind(socketfd, (struct sockaddr *)&servAddr, (socklen_t)sizeof(servAddr)), -1,
		"bind (Server: establishsocket");

	/* Set the socket as passive (listening). */
	checkerr(listen(socketfd, backlog), -1, "listen (Server: establishsocket)");

	return socketfd;
}

/* Function: acceptconnection
 * --------------------------
 * Accepts incoming connections on a passive socket.
 *
 * listenfd: file descriptor for passive socket.
 *
 * returns: file descriptor for the connection.
 */
int acceptconnection(int listenfd) {

	/* Initialize variables. */
	int connectfd;
	socklen_t addrLen = sizeof(struct sockaddr_in);
	struct sockaddr_in clientAddr;

	/* Wait for connection. */
	connectfd = accept(listenfd, (struct sockaddr *)&clientAddr, &addrLen);
	checkerr(connectfd, -1, "accept (Server: acceptconnection)");

	return connectfd;
}

/* Function: msghandler
 * -------------------------
 * Sends messages to a connection.
 *
 * connectfd: file descriptor for connection.
 * msg: message to send to the connection.
 *
 * returns: void.
 */
void msghandler(int connectfd, char * msg) {
	checkerr(write(connectfd, msg, strlen(msg)), -1, "write (Server: msghandler)");
}

/* Function: readhandler
 * ---------------------
 * Reads from a connection until newline or EOF is encountered or
 *	the read buffer is full.
 *
 * connectfd: file descriptor for connection.
 * buffer: read buffer.
 * buflen: length of read buffer.
 *
 * returns: number of characters read.
 */
int readhandler(int connectfd, char * buffer, int buflen) {

	int index = 0;

	while (1) {
		if (index >= buflen) break;
		int rnum = read(connectfd, buffer + index, 1);
		checkerr(rnum, -1, "read (Client: readhandler)");
		if (!rnum || buffer[index] == '\n') break;
		index++;
	}

	return index;
}

/* Function: executecmd
 * --------------------
 * Executes a shell command using execlp given a command name and 
 *	an argument.
 *
 * name: name of the shell command.
 * arg: argument to pass to the shell command.
 *
 * returns: void.
 */
void executecmd(char * name, char * arg) {
	checkerr(execlp(name, name, arg, NULL), -1, "execlp (Server: executecmd)");
}

/* Function: executels
 * -------------------
 * Executes the command ls -l and pipes the output to a data connection.
 *
 * datafd: file descriptor for data connection.
 *
 * returns: void.
 */
void executels(int datafd) {

	/* Wait for connection. */
	int connectfd = acceptconnection(datafd);

	/* Fork... */
	int pid = fork();
	checkerr(pid, -1, "fork (Server: executels)");

	/* ...and exec. */
	if (!pid) {
		checkerr(dup2(connectfd, STDOUT_FILENO), -1, "dup2 (Server: executels)");
		close(connectfd);
		executecmd("ls", "-l");
	}

	/* Close the connection. */
	close(connectfd);
}

/* Function: openfile
 * ------------------
 * Opens a file with given flags.
 *
 * connectfd: file descriptor for current connection (for error messages).
 * filename: name of file.
 * flags: flags for open.
 *
 * returns: file descriptor for open file or -1 if the file is invalid.
 */
int openfile(int connectfd, char * filename, int flags) {

	/* Initialize variables. */
	char clientmsg[256] = {0};
	struct stat filestat;
	int myfd = open(filename, flags);

	/* Check to see if the file exists and can be opened. */
	if (myfd == -1) {
		if (errno == ENOENT) {
			snprintf(clientmsg, 256, "E%s does not exist\n", filename);
			msghandler(connectfd, clientmsg);
			printf("ERROR: %s does not exist\n", filename);
		} else if (errno == EEXIST) {
			snprintf(clientmsg, 256, "E%s already exists\n", filename);
			msghandler(connectfd, clientmsg);
			printf("ERROR: %s already exists\n", filename);
		} else {
			snprintf(clientmsg, 256, "ECannot open %s\n", filename);
			msghandler(connectfd, clientmsg);
			printf("ERROR: %s cannot open\n", filename);
		}
		return myfd;
	} else {

		/* stat the file. */
		checkerr(fstat(myfd, &filestat), -1, "stat (server: openfile)");

		/* Check to see if the file is regular. */
		if (!S_ISREG(filestat.st_mode) || S_ISDIR(filestat.st_mode)) {
			snprintf(clientmsg, 256, "E%s is not a regular file\n", filename);
			msghandler(connectfd, clientmsg);
			printf("ERROR: %s is not a regular file\n", filename);
			close(myfd);
			return -1;
		}
	}

	/* Acknowledge and return successful result. */
	msghandler(connectfd, "A\n");
	return myfd;
}

/* Function: readwrite
 * -------------------
 * Reads from one file descriptor and writes to another one character at a
 * 	time.
 *
 * readfd: file descriptor for reading.
 * writefd: file descriptor for writing.
 *
 * returns: void.
 */
void readwrite(int readfd, int writefd) {
	char nextc;
	while(read(readfd, &nextc, 1)) {
		write(writefd, &nextc, 1);
	}
}

/* Function: getsocketinfo
 * -----------------------
 * Gets the info associated with a socket file descriptor and returns a structure
 * 	containing said info.
 *
 * socketfd: file descriptor for a socket.
 *
 * returns: structure containing socket info.
 */
struct sockaddr_in getsocketinfo(int socketfd) {
	struct sockaddr_in socketAddr;
	socklen_t socketLen = sizeof(socketAddr);
	checkerr(getsockname(socketfd, (struct sockaddr *)&socketAddr, &socketLen), -1,
		"getsockname (Server: getsocketinfo)");
	return socketAddr;
}

/* Function: serverhandler
 * -----------------
 * Handles all incoming connections to the server.
 *
 * connectfd: file descriptor for connection.
 * hostname: hostname for connection.
 *
 * returns: void.
 */
void serverhandler(int connectfd, char * hostname) {

	int datafd = 0; // File descriptor for data socket.

	while (1) {

		/* Initialize variables. */
		char buffer [512] = {0};
		char clientmsg [256] = {0};

		/* Read the next command from the connection. */
		readhandler(connectfd, buffer, 512);

		/* Handle commands. */
		if (buffer[0] == 'D') {

			datafd = establishsocket(0, 1);
			struct sockaddr_in dataAddr = getsocketinfo(datafd);
			snprintf(clientmsg, 256, "A%hu\n", htons(dataAddr.sin_port)); // convert address from network byte order to host byte order.
			msghandler(connectfd, clientmsg);

		} else if (buffer[0] == 'C') {

			/* Get pathname. */
			char * path = strtok(buffer + 1, "\n");

			/* Try to change current working directory to pathname, sending appropriate errors if chdir fails. */
			if (chdir(path) == -1) {
				snprintf(clientmsg, 256, "EInvalid pathname %s\n", path);
				msghandler(connectfd, clientmsg);
				printf("ERROR: Invalid pathname %s\n", path);
			} else {
				msghandler(connectfd, "A\n");
				printf("%s: Changed curent working directory to %s\n", hostname, path);
			}

		} else if (buffer[0] == 'L') {

			executels(datafd);
			close(datafd);
			msghandler(connectfd, "A\n");

		} else if (buffer[0] == 'G') {

			/* Get the filename. */
			char * file = strtok(buffer + 1, "\n");

			/* Wait for connection with client. */
			int dataconnfd = acceptconnection(datafd);

			/* Open the file for reading, continue if open fails. */
			int myfd = openfile(connectfd, file, O_RDONLY);
			if (myfd == -1) continue;

			/* Read from the file and write to the data connection. */
			readwrite(myfd, dataconnfd);

			/* Close the data connection, the file, and the data socket. */
			close(dataconnfd);
			close(myfd);
			close(datafd);

			/* Print a confirmation server-side. */
			printf("%s: Sent contents of %s\n", hostname, file);

		} else if (buffer[0] == 'P') {

			/* Get the filename. */
			char * file = strtok(buffer + 1, "\n");

			/* Wait for connection with client. */
			int dataconnfd = acceptconnection(datafd);

			/* Open the file for writing, creating if it doesn't exit, failing otherwise. */
			int myfd = openfile(connectfd, file, O_WRONLY | O_CREAT | O_EXCL);
			if (myfd == -1) continue;

			/* Read from the data connection and write to the file. */
			readwrite(dataconnfd, myfd);

			/* Close the data connection, set permissions on the file, close the file, and close the data socket. */
			close(dataconnfd);
			chmod(file, S_IRUSR | S_IWUSR);
			close(myfd);
			close(datafd);

			/* Print a confirmation server-side. */
			printf("%s: Received contents of %s\n", hostname, file);

		} else if (buffer[0] == 'Q') {

			msghandler(connectfd, "A\n");
			printf("%s: Closing connection\n", hostname);
			break;

		} else {

			snprintf(clientmsg, 256, "EInvalid input %s\n", buffer);
			msghandler(connectfd, clientmsg);
			printf("ERROR: Invalid input %s\n", buffer);

		}
	}
}

/* Main Function */
int main() {

	/* Establish passive socket. */
	int port = PORT_NUM;
	int listenfd = establishsocket(port, 4);

	while (1) {

		/* Wait for connection. */
		int connectfd = acceptconnection(listenfd);

		/* Get rid of zombies. */
		while (waitpid(-1, NULL, WNOHANG) > 0);

		/* Fork... */
		pid_t pid = fork();
		checkerr(pid, -1, "fork (Server: main)");

		/* ...and handle the connection in the child. */
		if (!pid) {
			struct sockaddr_in clientAddr = getsocketinfo(connectfd);
			struct hostent * hostEntry;
			if((hostEntry = gethostbyaddr(&(clientAddr.sin_addr), sizeof(struct in_addr), AF_INET)) == NULL) {
				herror("gethostbyaddr (Server: main)");
				exit(1);
			}
			printf("Connection received: %s\n", hostEntry->h_name);
			serverhandler(connectfd, hostEntry->h_name);
			close(connectfd); // Close the connection in the child.
			exit(0);
		}

		/* Close the connection in the parent. */
		close(connectfd);
	}

	return 0;
}
