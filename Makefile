CFL=-g -Wall -lm
MIPPRE=./mipdf/
PINGPRE=./ping/
RDPRE=./rdf/
MIPTPPRE=./miptpf/
FILEPRE=./file/
DEPS=$(MIPPRE)mipd.h

# Default
all: mipd ping rd miptp file

# Cleanup
clear:
	rm *.o -fv
	rm socket* -fv

# MIP daemon
mipd: mipd.o mipmac.o mip.o unixs.o messagelist.o comm.o mipdproto.o
	gcc -o mipd mipd.o mipmac.o mip.o unixs.o messagelist.o comm.o mipdproto.o $(CFL)

mipd.o: $(MIPPRE)mipd.c $(MIPPRE)mip.h $(MIPPRE)unixs.h $(MIPPRE)mipmac.h $(MIPPRE)comm.h
	gcc -c -o mipd.o $(MIPPRE)mipd.c $(CFL)

mipmac.o: $(MIPPRE)mipmac.c $(DEPS)
	gcc -c -o mipmac.o $(MIPPRE)mipmac.c $(CFL)

mip.o: $(MIPPRE)mip.c $(MIPPRE)mipmac.h $(MIPPRE)messagelist.h $(MIPPRE)unixs.h $(DEPS)
	gcc -c -o mip.o $(MIPPRE)mip.c $(CFL)

unixs.o: $(MIPPRE)unixs.c $(MIPPRE)messagelist.h $(MIPPRE)mip.h $(DEPS)
	gcc -c -o unixs.o $(MIPPRE)unixs.c $(CFL)

messagelist.o: $(MIPPRE)messagelist.c $(MIPPRE)mip.c $(DEPS)
	gcc -c -o messagelist.o $(MIPPRE)messagelist.c $(CFL)

comm.o: $(MIPPRE)comm.c $(MIPPRE)mip.h $(MIPPRE)unixs.h $(DEPS)
	gcc -c -o comm.o $(MIPPRE)comm.c $(CFL)

mipdproto.o: $(MIPPRE)mipdproto.c $(DEPS)
	gcc -c -o mipdproto.o $(MIPPRE)mipdproto.c $(CFL)

# PING programs
ping: pings pingc

pings: pings.o
	gcc -o pings pings.o $(CFL)

pings.o: $(PINGPRE)pings.c
	gcc -c -o pings.o $(PINGPRE)pings.c $(CFL)

pingc: pingc.o
	gcc -o pingc pingc.o $(CFL)

pingc.o: $(PINGPRE)pingc.c
	gcc -c -o pingc.o $(PINGPRE)pingc.c $(CFL)

# Routing Daemon
rd: rd.o mipdproto.o
	gcc -o rd rd.o mipdproto.o $(CFL)

rd.o: $(RDPRE)rd.c
	gcc -c -o rd.o $(RDPRE)rd.c $(CFL)

# Transport Daemon
miptp: miptp.o applist.o miptpproto.o mipdproto.o packetlist.o gbn.o tpproto.o
	gcc -o miptp miptp.o applist.o miptpproto.o mipdproto.o packetlist.o gbn.o tpproto.o $(CFL)

miptp.o: $(MIPTPPRE)miptp.c $(MIPTPPRE)applist.h $(MIPTPPRE)miptpproto.h
	gcc -c -o miptp.o $(MIPTPPRE)miptp.c $(CFL)

applist.o: $(MIPTPPRE)applist.c $(MIPTPPRE)miptp.h
	gcc -c -o applist.o $(MIPTPPRE)applist.c $(CFL)

miptpproto.o: $(MIPTPPRE)miptpproto.c $(MIPTPPRE)miptp.h
	gcc -c -o miptpproto.o $(MIPTPPRE)miptpproto.c $(CFL)

packetlist.o: $(MIPTPPRE)packetlist.c $(MIPTPPRE)miptp.h
	gcc -c -o packetlist.o $(MIPTPPRE)packetlist.c $(CFL)

gbn.o: $(MIPTPPRE)gbn.c $(MIPTPPRE)miptp.h
	gcc -c -o gbn.o $(MIPTPPRE)gbn.c $(CFL)

tpproto.o: $(MIPTPPRE)tpproto.c $(MIPTPPRE)miptp.h
	gcc -c -o tpproto.o $(MIPTPPRE)tpproto.c

# File server and client
file: files filec

files: files.o miptpproto.o
	gcc -o files files.o miptpproto.o $(CFL)

files.o: $(FILEPRE)files.c
	gcc -c -o files.o $(FILEPRE)files.c $(CFL)

filec: filec.o miptpproto.o
	gcc -o filec filec.o miptpproto.o $(CFL)

filec.o: $(FILEPRE)filec.c
	gcc -c -o filec.o $(FILEPRE)filec.c $(CFL)