CC=g++
INCL=-lpcap -lpthread
##DIR=/home/tlucero/Data_Networks_Projects/Project4/
DIR=/Users/tobiaslucero/Desktop/project4/

all: project4

project3: project3.cpp project4.h
	$(CC) project4.cpp $(INCL) -o project4.o

clean:
	rm -f *.o

run:
	./project4.o $(DIR)p3.pcap

A:
	./project4.o $(DIR)p3.pcap $(DIR)input/1.txt $(DIR)input/A1.txt

B:
	./project4.o $(DIR)p3.pcap $(DIR)input/2.txt $(DIR)input/B2.txt 

C:
	./project4.o $(DIR)p3.pcap $(DIR)input/3.txt $(DIR)input/C3.txt

D:
	./project4.o $(DIR)p3.pcap $(DIR)input/4.txt $(DIR)input/D4.txt

zip:
	zip project4.zip *.cpp Makefile	
