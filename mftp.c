/* CS 360 (Systems Programming) -- Final Project
 * 	written by Shawn Hillstrom
 * ---------------------------------------------
 * Client implementation.
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

/* Function: makeconnection
 * ------------------------
 * Creates a new connection on a specified port number.
 *
 * port: port number for connection.
 * hostname: the hostname for the connection.
 *
 * returns: file descriptor for the new connection.
 */
int makeconnection(unsigned short port, char * hostname) {

	/* Create socket. */
	int socketfd = socket(AF_INET, SOCK_STREAM, 0);
	checkerr(socketfd, -1, "socket (Client: makeconnection)");

	/* Set the family, port number, and hostname for the socket. */
	struct sockaddr_in servAddr;
	struct hostent * hostEntry;
	struct in_addr ** pptr;
	memset(&servAddr, 0, sizeof(servAddr)); // Zero out servAddr.
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(port);
	if ((hostEntry = gethostbyname(hostname)) == NULL) {
		herror("gethostbyname (Client: makeconnection)");
		exit(1);
	}
	pptr = (struct in_addr **)hostEntry->h_addr_list;
	memcpy(&servAddr.sin_addr, *pptr, sizeof(struct in_addr));

	/* Connect to the socket. */
	checkerr(connect(socketfd, (struct sockaddr *)&servAddr, sizeof(servAddr)), -1,
		"connect (Client: makeconnection)");

	return socketfd;
}

/* Function: msghandler
 * -------------------------
 * Sends a message to a connection.
 *
 * connectfd: file descriptor for connection.
 * msg: message to send to the connection.
 *
 * returns: void.
 */
void msghandler(int connectfd, char * msg) {
	checkerr(write(connectfd, msg, strlen(msg)), -1, "write (Client: msghandler)");
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

/* Function: responsehandler
 * -------------------------
 * Handles all responses from the server over the given connection.
 *
 * connectfd: file descriptor for connection.
 * address: (optional) pointer to integer where response address can 
 *	be stored.
 *
 * returns: 0 on server error, 1 otherwise.
 */
int responsehandler(int connectfd, int * address) {

	char response [256] = {0};
	readhandler(connectfd, response, 256);

	if (response[0] == 'A') {
		if (address != NULL) {
			*address = atoi(response + 1);
		}
		return 1;
	} else if (response[0] == 'E') {
		printf("SERVER: %s", response + 1);
		return 0;
	}

	return 0;
}

/* Function: executecmd
 * --------------------
 * Executes a shell command using execlp given a command name and an 
 *	argument.
 *
 * name: name of the shell command.
 * arg: argument to pass to the shell command.
 *
 * returns: void.
 */
void executecmd(char * name, char * arg) {
	checkerr(execlp(name, name, arg, NULL), -1, "execlp (Client: executecmd)");
}

/* Function: executemore
 * ---------------------
 * Executes the command more -20 using the given input pipe.
 *
 * pipefd: file descriptor for input pipe.
 *
 * returns: void.
 */
void executemore(int pipefd) {

	int pid = fork();
	checkerr(pid, -1, "fork (Client: executemore)");

	if (!pid) {
		checkerr(dup2(pipefd, STDIN_FILENO), -1, "dup2 (Client: executemore)");
		close(pipefd);
		executecmd("more", "-20");
	}

	waitpid(pid, NULL, 0);
	close(pipefd);
}

/* Function: executels
 * -------------------
 * Executes the command ls -l and pipes the output to more.
 *
 * returns: void.
 */
void executels() {

	int pipefd[2];
	checkerr(pipe(pipefd), -1, "pipe (Client: executels)");

	pid_t pid = fork();
	checkerr(pid, -1, "fork (Client: executels)");

	if (pid) {
		close(pipefd[1]); // Close write end.
		executemore(pipefd[0]);
	} else {
		close(pipefd[0]); // Close read end.
		checkerr(dup2(pipefd[1], STDOUT_FILENO), -1, "dup2 (Client: executels)");
		close(pipefd[1]);
		executecmd("ls", "-l");
	}
}

/* Function: getaddress
 * ------------------------
 * Gets an address for a data connection given an existing connection.
 *
 * connectfd: file descriptor for connection.
 *
 * returns: address of data connection.
 */
int getaddress(int connectfd) {
	int address;
	msghandler(connectfd, "D\n");
	responsehandler(connectfd, &address);
	return address;
}

/* Function: dataconnect
 * ---------------------
 * Creates a data connection for a given an existing connection and a server
 *	command.
 *
 * connectfd: file descriptor for connection.
 * hostname: hostname for data connection.
 * servermsg: string containing server command.
 *
 * returns: file descriptor for data connection.
 */
int dataconnect(int connectfd, char * hostname, char * servermsg) {
	int address = getaddress(connectfd);
	msghandler(connectfd, servermsg);
	return makeconnection(address, hostname);
}

/* Function: openfile
 * ------------------
 * Opens a file with given flags.
 *
 * filename: name of file.
 * flags: flags for open.
 *
 * returns: file descriptor for open file or 0 if the file is invalid.
 */
int openfile(char * filename, int flags) {

	/* Initialize variables. */
	struct stat filestat;

	int myfd = open(filename, flags);

	/* Check to see if the file exists and can be opened. */
	if (myfd == -1) {
		if (errno == ENOENT) printf("ERROR %s does not exist\n", filename);
		else if (errno == EEXIST) printf("ERROR: %s already exists\n", filename);
		else printf("ERROR: %s does not exist\n", filename);
		return myfd;
	} else {

		/* stat the file. */
		checkerr(stat(filename, &filestat), -1, "stat (Server: openfile)");

		/* Check to see if the file is regular. */
		if (!S_ISREG(filestat.st_mode) || S_ISDIR(filestat.st_mode)) {
			printf("ERROR: %s is not a regular file\n", filename);
			close(myfd);
			return -1;
		}
	}

	/* Return successful result. */
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

/* Function: clienthandler
 * ----------------
 * Handles passing input to a given connection.
 *
 * connectfd: file descriptor for the connection.
 * hostname: hostname for the connection.
 *
 * returns: void.
 */
void clienthandler(int connectfd, char * hostname) {

	while (1) {

		/* Print prompt */
		printf("> ");

		/* Initialize variables. */
		char buffer[1024] = {0};
		char servermsg[512] = {0};

		/* Get next input line. */
		fgets(buffer, 1024, stdin);

		/* Separate the line based on whitespace. */
		char * token = strtok(buffer, " \t\n");

		/* If we got nothing, print an error and help then continue. */
		if (token == NULL) {
			printf("ERROR: No input detected\n");
			continue;
		}

		/* Handle input. */
		if (strcmp(token, "exit") == 0) {

			msghandler(connectfd, "Q\n");
			responsehandler(connectfd, NULL);
			break;

		} else if (strcmp(token, "cd") == 0) {

			token = strtok(NULL, " \t\n");
			if (chdir(token) == -1) printf("ERROR: Invalid pathname (%s)\n", token);

		} else if (strcmp(token, "rcd") == 0) {

			token = strtok(NULL, " \t\n");
			snprintf(servermsg, 512, "C%s\n", token);
			msghandler(connectfd, servermsg);
			responsehandler(connectfd, NULL);

		} else if (strcmp(token, "ls") == 0) {

			executels();

		} else if (strcmp(token, "rls") == 0) {

			int datafd = dataconnect(connectfd, hostname, "L\n");
			responsehandler(connectfd, NULL);
			executemore(datafd);

		} else if (strcmp(token, "get") == 0) {

			/* Get the filename. */
			token = strtok(NULL, " \t\n");

			/* Open the data connection with the server and run get. */
			snprintf(servermsg, 512, "G%s\n", token);
			int datafd = dataconnect(connectfd, hostname, servermsg);

			/* Wait for acknowledgement, or fail if the client receives an error. */
			if (!responsehandler(connectfd, NULL)) continue;

			/* Open the file for writing, creating if it doesn't exist, failing otherwise. */
			int myfd = openfile(token, O_WRONLY | O_CREAT | O_EXCL);
			if (myfd == -1) continue;

			/* Read from the data connection and write to the file. */
			readwrite(datafd, myfd);

			/* Close the data connection, set permissions on the file, and close the file. */
			close(datafd);
			chmod(token, S_IRUSR | S_IWUSR);
			close(myfd);

		} else if (strcmp(token, "show") == 0) {

			/* Get the filename. */
			token = strtok(NULL, " \t\n");

			/* Open the data connection with the server and run get. */
			snprintf(servermsg, 512, "G%s\n", token);
			int datafd = dataconnect(connectfd, hostname, servermsg);

			/* Wait for acknowledgement, or fail if the client receives an error. */
			if (!responsehandler(connectfd, NULL)) continue;

			/* Pipe the data connection to more -20 */
			executemore(datafd);

		} else if (strcmp(token, "put") == 0) {

			/* Get the filename. */
			token = strtok(NULL, " \t\n");

			/* Open the file for reading. */
			int myfd = openfile(token, O_RDONLY);
			if (myfd == -1) continue;

			/* Open the data connection with the server and run put. */
			snprintf(servermsg, 512, "P%s\n", token);
			int datafd = dataconnect(connectfd, hostname, servermsg);

			/* Wait for acknowledgement, or fail if the client receives an error. */
			if (!responsehandler(connectfd, NULL)) continue;

			/* Read from the file and write to the data connection. */
			readwrite(myfd, datafd);

			/* Close the data connection and the file. */
			close(datafd);
			close(myfd);

		} else {

			printf("ERROR: Invalid input (%s)\n", token);

		}
	}
}

/* Main Function */
int main(int argc, char * argv[]) {

	/* Check number of arguments. */
	if (argc != 2) {
		fprintf(stderr, "argv (Client: main): Incorrect number of arguments\n");
		printf("Usage: ./%s <host>\n", argv[0]);
		exit(1);
	}

	/* Create control connection. */
	int port = PORT_NUM;
	int connectfd = makeconnection(port, argv[1]);
	printf("Connection established on port %d\n", port);

	/* Handle input and send to the connection. */
	clienthandler(connectfd, argv[1]);

	/* Close the connection. */
	close(connectfd);

	return 0;
}
