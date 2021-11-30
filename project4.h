#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <pcap.h>
#include <string.h>
#include <time.h>
#include <queue>

using namespace std;

static char my_ipaddr[15];
static int my_port;
static int number_of_neighbors;
static int total_no_hosts;
static struct neighborInfo * neighbors;
static struct routerTableEntry * router_table;

static queue<unsigned char*>* neighbor_queues;
static int* send_next;
static clock_t* sent_time;
static clock_t* ack_recv_time;
static int* send_ack;
static int* acked;

static queue<int>* neighbor_queues_message_length;

static queue<unsigned char*>* frames;

struct neighborInfo
{
    char fakeIP[15];
    struct in_addr ip; 
	  int port;
    int sender_port;
};

struct routerTableEntry
{
  char dst[15];
  char nxt_hop[15];
};

struct frame 
{
  unsigned char data[140];
};

// This function takes two addresses and compares them
// to see if they are the same. Returns 1 if they are the same
// 0 otherwise
int compareAddresses(char * addr1, char * addr2) {
  if(strlen(addr1)!=strlen(addr2)) {
    return 0;
  }
  
  for(int i = 0; i < strlen(addr1); ++i) {
    if(addr1[i]!=addr2[i]) {
      return 0;
    }
  }
  return 1;
}

// This function takes an ip address and returns the index
// of the neighbor in global neighbor array that is the next hop
int route(char * addr) {
  printf("Routing %s by fake addr\n", addr);
  int index = -1;
  for(int i = 0; i < total_no_hosts; ++i) {
    //printf("%d: %s %s\n", i, entry.dst, entry.nxt_hop);
    //printf("i: %d %s %s", i, router_table[i].dst, addr);
    if(compareAddresses(router_table[i].dst, addr) == 1) {
      //return router_table[i].nxt_hop;
      //printf("i: %d\n", i);
      index = i;
    }
  }

  if(index == -1) return -1;

  for(int i = 0; i < number_of_neighbors; ++i) {
    //printf("j: %d %s %s\n", i, router_table[index].nxt_hop, neighbors[i].fakeIP);
    if(compareAddresses(neighbors[i].fakeIP, router_table[index].nxt_hop))
    {
     // printf("Returning %d\n", i);
      return i;
    }
  }

  return -1;
}

int routeBySenderPortNum(int port) {
  printf("Routing by sender port num %d\n", port);

  for(int i = 0; i < number_of_neighbors; ++i) {
    //printf("%d %d\n", neighbors[i].sender_port, port);
    if(neighbors[i].sender_port == port)
    {
      //printf("Returning %d\n", i);
      return i;
    }
  }

  return -1;
}

// This function prints the frame in the project1 style
void printFrame(unsigned char buf[]) 
{

    int64_t mac_timestamp;
    double data_rate;
    short channel_freq;

    int i = 0; // This will be the index of the current byte i'm on

    printf("\nRadiotop: -----Radiotop Header----\n");
    printf("\tRadiotop:\n");
    
    printf("\tRadiotop: Header revision = %d\n", ( (int) buf[i] ));
    
    printf("\tRadiotop: Header pad = %d\n", ( (int) buf[i+=1] ));
    
    printf("\tRadiotop: Header length = %d\n", ( (int) buf[i+=1] ));    
    
    printf("\tRadiotop: Present flags word = %x\n", ( buf[i+=2] )); // Add 2 because header length is 2 bytes
    
    memcpy(&mac_timestamp, &buf[i+=4], sizeof(int64_t)); // Mac timestamp is 64 bit int
    printf("\tRadiotop: Mac Timestamp = %ld\n", mac_timestamp); // Add 3 because present flags word is 3 bytes 
    
    printf("\tRadiotop: Flag = %x\n", ( (int) buf[i+=8])); // Add 8 because timestamp is 8 bytes
    
   // memcpy(&data_rate, &buf[i+=1], 1);
    printf("\tRadiotop: Data Rate = %.1f Mb/s\n", ((float)buf[i+=1])/2);
    
    memcpy(&channel_freq, &buf[i+=1], 2);
    printf("\tRadiotop: Channel frequency = %d\n", channel_freq);
    
    printf("\tRadiotop: Channel flags = %x\n", (unsigned char)buf[i+=2]);

  // 802.11 Header
  
    printf("\nHeader: ----- 802.11 Header -----\n");
    printf("\tHeader:\n");
    
    printf("\tHeader: Frame Control Field = 0x%.2d", buf[i+=2]); 
    printf("%.2d\n", buf[i+=1]); // Have to print this in two lines otherwise some error occurs

    printf("\tHeader: Duration = %d microseconds\n", ( (uint16_t) buf[i+=1] ) ); // Using int16 because 2 bytes 2*8 = 16
    
    // With receiver and transmitter address have to print it in separate lines
    printf("\tHeader: Receiver Address = %.2d:", buf[i+=2] );
    printf("%.2d:", buf[i+=1]);
    printf("%.2d:", buf[i+=1]);
    printf("%.2d:", buf[i+=1]);
    printf("%.2d:", buf[i+=1]);
    printf("%.2d\n", buf[i+=1]);
    
    printf("\tHeader: Transmitter Address = %.2d:", buf[i+=1]);
    printf("%.2d:", buf[i+=1]);
    printf("%.2d:", buf[i+=1]);
    printf("%.2d:", buf[i+=1]);
    printf("%.2d:", buf[i+=1]);
    printf("%.2d\n", buf[i+=1]);

    i+=6; // Skip the 6 bytes for destination address
    
    printf("\tHeader: Fragment number = %d\n", ( (uint16_t) buf[i+=1] ) );
    
    i+=2; // Skip 2 bytes because fragment number is 2 bytes

    // Skip 8 bytes of logical link control
    i+=8;

   // IPv4 Header
    printf("\nIP: -----IP Header -----\n");

    // Header length and IP version are on the same byte. IP version is bits 4-7, header_length is bits 0-3
    // So we must do some bit manipulation to access only those bits for each
    unsigned int k = 1; // First we take unsigned int set to one
    k = ((((((k << 1) + 1) << 1) + 1) << 1) + 1); // This operation turns 0000 0001 -> 0000 1111
    unsigned int hl = k & buf[i]; // Now we AND 0000 1111 & byte therefore only giving us the bits belonging to header length
    k = k << 4; // Now we left-shift 4 so make 0000 1111 -> 1111 0000
    unsigned int v = k & buf[i]; // Take the AND of k and the byte to get the bits belonging to the ip version
    v = v >> 4; // Now we must left-shift the version because we just want those 4 bits not the entire 8.

    printf("\tIP: Version = %d\n", v );
    printf("\tIP: Header length = %d bytes\n", hl * 4);

    int tot_len;
    memcpy(&tot_len, &buf[i+=3], 1);
    printf("\tIP: Total length = %d\n", tot_len);
    
    printf("\tIP: Flags = 0x%x\n", buf[i+=3]);
   
    unsigned int j = 1; // To get the individual bits which represent the flags, will use
   // an unsigned int set to 1, then left-shift it 7 bits so 0000 0001 -> 1000 0000 then
   // we take the AND of the byte with the flags and j and we see whether any of the bits are set
    j = j << 7;

   // Used https://stackoverflow.com/questions/9280654/c-printing-bits  

    printf("\tIP:\t%u... .... .... .... Reserved Bit: %s\n", !!(j & buf[60]), ((j & buf[60]) ? "Set" : "Not set"));
    j = j >> 1;
    printf("\tIP:\t.%u.. .... .... .... Don't fragment: %s\n", !!(j & buf[60]), ((j & buf[60]) ? "Set" : "Not set"));
    j = j >> 1;
    printf("\tIP:\t..%u. .... .... .... More fragments: %s\n", !!(j & buf[60]), ((j & buf[60]) ? "Set" : "Not set"));
    
    printf("\tIP: Fragment offset = %d bytes\n", buf[i+=1]);
    
    printf("\tIP: Time to live = %d seconds/hop\n", buf[i+=1]);
    printf("\tIP: Protocol = %d (ICMP)\n", buf[i+=1]);
    
    printf("\tIP: Header checksum = %x", (unsigned char)buf[i+=1]); // Since it is two bytes
    printf("%x\n", (unsigned char)buf[i+=1]); // Print it in two steps

    struct in_addr ip_src; // Using these for the source and destination addresses
    struct in_addr ip_dst;

    memcpy(&ip_src, &buf[i+=1], sizeof(in_addr));
    printf("\tIP: Source address = %s\n", inet_ntoa(ip_src));

    memcpy(&ip_dst, &buf[i+=4], sizeof(in_addr)); // Add 4 bytes because ip address is 4 bytes long
    printf("\tIP: Destination address = %s\n", inet_ntoa(ip_dst));

    // Packet data

    printf("\nICMP: ----- Packet Data ----\n");

    unsigned char endofline[16];
    int m = 0;
    for(int l = 0; l < 138 - i; l++) {
      if(l % 16 == 0) {
        if(l!=0) {
            for(int m = 0; m < 16; ++m) {
              printf("%c", endofline[m]);

              if(m==7)
                printf(" ");
            }
        }
        printf("\n%04x ", l);
      }
      printf("%02x ", (unsigned char)buf[i + l]);

      if((((int)buf[i+l]) > 33) & (((int)buf[i+l]) < 127) ) {
        endofline[l % 16] = (char) buf[i + l];
      } else {
        endofline[l % 16] = '.';
      }

    }
    printf("\n");   
}

// This function will be executed by the thread which will watch the queue
void* watchQueue(void* args)
{
    // Loop forever
    while(true){
        // If the queue is not empty print the frame at the front and pop
        if(!((*frames).empty()))
        {
            printFrame((*frames).front());
            printf("Freeing\n");
            free((*frames).front());
            (*frames).pop();
        }
    }
}

void parseFiles(FILE *configfile, FILE *switchfile) {
  // This will be our fake IP address
  int i = 0;
  char x;


  // Make sure the content is set to zero
  memset(my_ipaddr,0, 15);
  while(x != '\n') 
  {
      fread(&x, 1, 1, configfile);
      my_ipaddr[i++] = x; 
  }
  my_ipaddr[i-1] = 0;

  // This will be used to copy things from the file
  char temp_string[15];

  fread(&x, 1, 1, configfile);
  
  x='a';
  i=0;

  while(x != '\n') 
  {
      fread(&x, 1, 1, configfile);
      temp_string[i++] = x; 
  }
  temp_string[i-1] = 0;
  my_port = atoi(temp_string);

  fread(&x, 1, 1, configfile);
  memset(temp_string, 0, 6);
  
  x='a';
  i = 0;
  while(x != '\n') 
  {
      fread(&x, 1, 1, configfile);
      temp_string[i++] = x;
  }

  temp_string[i-1] = 0;

  number_of_neighbors = atoi(temp_string);

  neighbors = (neighborInfo*) malloc(sizeof(neighborInfo) * number_of_neighbors );

  unsigned long neighbor_addr;

  for(int k = 0; k < number_of_neighbors; ++k) 
  {
      neighborInfo n_info;
    fread(&x, 1, 1, configfile);
  
    i=0;
    while(x != ' ') 
    {
      fread(&x, 1, 1, configfile);
      n_info.fakeIP[i++] = x; 
    }
    n_info.fakeIP[i-1] = 0;
    
    strcpy(temp_string, "127.0.0.1");

    neighbor_addr = inet_addr(temp_string);
    memcpy(&n_info.ip, &neighbor_addr, sizeof(struct in_addr));

    memset(temp_string,0, 6);
    i=0;

    x='a';
    for(int j = 0; j < 10; ++j)
      fread(&x, 1, 1, configfile);
  
    while(fread(&x, 1, 1, configfile) == 1) {
      if(x == '\n') {
          break;
      }
      temp_string[i++] = x;
    }
    temp_string[i] = 0;
    n_info.port = atoi(temp_string);

    //printf("2%s %d %s\n", n_info.fakeIP, n_info.port, inet_ntoa(n_info.ip));
    memcpy(&neighbors[k], &n_info, sizeof(neighborInfo));

    //printf("1%s %d\n", neighbors[k].fakeIP, neighbors[k].port);
  }

  i = 0;
  while(x != '\n') 
  {
      fread(&x, 1, 1, switchfile);
      temp_string[i++] = x; 
  }
  temp_string[i-1] = 0;

  total_no_hosts = atoi(temp_string);
  total_no_hosts--; // Compensate?

  router_table = (routerTableEntry*) malloc(sizeof(routerTableEntry) * total_no_hosts );

  //routerTableEntry router_table_local[number_of_routes];

  routerTableEntry entry; 
      
  fread(&x, 1, 1, switchfile);

  for(int k = 0; k < total_no_hosts; ++k)
  {
    i=0;
    while(x != ' ') 
    {
        fread(&x, 1, 1, switchfile);
        temp_string[i++] = x; 
    }
    temp_string[i-1] = 0;

    strcpy(entry.dst, temp_string);

    i = 0;
    while(fread(&x, 1, 1, switchfile) == 1) {
      if(x == '\n') {
          break;
      }
      temp_string[i++] = x;
    }
    temp_string[i] = 0;
    
    strcpy(entry.nxt_hop, temp_string);

    memcpy(&router_table[k], &entry, sizeof(routerTableEntry));

  }
  
}