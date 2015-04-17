DEPS=mipd.h
CFL=-g -Wall -lm

# Default
all: mipd ping rd miptp file

# Cleanup
clear:
	rm *.o -f -v
	rm socket* -f -v

# MIP daemon
mipd: mipd.o mipmac.o mip.o unixs.o messagelist.o comm.o
	gcc -o mipd mipd.o mipmac.o mip.o unixs.o messagelist.o comm.o $(CFL)

mipd.o: mipd.c mip.h unixs.h mipmac.h comm.h
	gcc -c -o mipd.o mipd.c $(CFL)

mipmac.o: mipmac.c $(DEPS)
	gcc -c -o mipmac.o mipmac.c $(CFL)

mip.o: mip.c mipmac.h messagelist.h unixs.h $(DEPS)
	gcc -c -o mip.o mip.c $(CFL)

unixs.o: unixs.c messagelist.h mip.h $(DEPS)
	gcc -c -o unixs.o unixs.c $(CFL)

messagelist.o: messagelist.c mip.c $(DEPS)
	gcc -c -o messagelist.o messagelist.c $(CFL)

comm.o: comm.c mip.h unixs.h $(DEPS)
	gcc -c -o comm.o comm.c $(CFL)

# PING programs
ping: pings pingc

pings: pings.o
	gcc -o pings pings.o $(CFL)

pings.o: pings.c
	gcc -c -o pings.o pings.c $(CFL)

pingc: pingc.o
	gcc -o pingc pingc.o $(CFL)

pingc.o: pingc.c
	gcc -c -o pingc.o pingc.c $(CFL)

# Routing Daemon
rd: rd.o
	gcc -o rd rd.o $(CFL)

rd.o: rd.c
	gcc -c -o rd.o rd.c $(CFL)

# Transport Daemon
miptp: miptp.o applist.o
	gcc -o miptp miptp.o applist.o $(CFL)

miptp.o: miptp.c applist.h
	gcc -c -o miptp.o miptp.c $(CFL)

applist.o: applist.c miptp.h
	gcc -c -o applist.o applist.c $(CFL)

# File server and client
file: files filec

files: files.o
	gcc -o files files.o $(CFL)

files.o: files.c
	gcc -c -o files.o files.c $(CFL)

filec: filec.o
	gcc -o filec filec.c $(CFL)

filec.o: filec.c
	gcc -c -o filec.o filec.c $(CFL)