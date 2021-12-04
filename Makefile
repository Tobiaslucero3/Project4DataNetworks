CC=g++
INCL=-lpcap -lpthread
DIR=/home/tlucero/Data_Networks_Projects/Project4/input/
##DIR=/Users/tobiaslucero/Desktop/project4/input/

all: project4

project4: project4.cpp project4.h
	$(CC) project4.cpp $(INCL) -o project4.o

clean:
	rm -f *.o

run:
	./project4.o $(DIR)p3.pcap

A:
	./project4.o $(DIR)p3.pcap $(DIR)1.txt

B:
	./project4.o $(DIR)p3.pcap $(DIR)2.txt 

C:
	./project4.o $(DIR)p3.pcap $(DIR)3.txt 

D:
	./project4.o $(DIR)p3.pcap $(DIR)4.txt 

zip:
	zip project4.zip *.cpp *.h Makefile	
