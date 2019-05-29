# Remote Server Management System for Linux

## About

This project implements a rudimentary remote server management system for Linux. The server can handle up to 5 connections at a time and relies on simple, shell-like commands on the client side to implement basic file management features. 

## Versioning

**VERSION:** 1.0

**RELEASE:** N/A

**LAST UPDATED:** May 4th, 2019

## Resources

**mftp.c:** Source file for client side services.

**mftpserver.c:** Source file for server side services.

**mftp.h:** Header file for both client and server side source files.

**Makefile:** Makefile for building the system.

## How To Use

To build the system:
1. Run `make all` to build all object files and executables. 
2. To remove all object files and executables run `make clean`.

To use the system:
1. Run `make runserver` to start the server.
2. Run `make runclient` to create a client connection on localhost.
3. Otherwise, run `./mftp <HOSTNAME || IPV4>` to create a client connection on the given hostname or ipv4 address.

## Future Development

No future development is planned at this time.