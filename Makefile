COMP = gcc
FLAGS =
TAGS =
SERV_SRC = mftpserve.c
CLNT_SRC = mftp.c
SERV_OBJ = mftpserve.o
CLNT_OBJ = mftp.o
SERV_OUT = mftpserve
CLNT_OUT = mftp

all: ${SERV_OBJ} ${CLNT_OBJ}
	${COMP} ${FLAGS} -o ${SERV_OUT} ${SERV_OBJ} ${TAGS}
	${COMP} ${FLAGS} -o ${CLNT_OUT} ${CLNT_OBJ} ${TAGS}

${SERV_OBJ}: ${SERV_SRC}
	${COMP} ${FLAGS} -c ${SERV_SRC} ${TAGS}

${CLNT_OBJ}: ${CLNT_SRC}
	${COMP} ${FLAGS} -c ${CLNT_SRC} ${TAGS}

clean:
	rm -f ${SERV_OBJ} ${CLNT_OBJ} ${SERV_OUT} ${CLNT_OUT}

runserver: ${SERV_OUT}
	./${SERV_OUT}

runclient: ${CLNT_OUT}
	./${CLNT_OUT} 'localhost'