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

static int my_index;

static int router_table_size;

struct neighborInfo
{
    char fakeIP[15];
    struct in_addr ip; 
	  int port;
    int sender_port;
    int cost;
    int index;
};

struct routerTableEntry
{
  int dst_index; // 1 = A, 2 = B, ..., 7=G
  int cost;
  int nxt_hop_index;
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


char * getAddrByIndexOnRouterTable(int index) {
  for(int i = 0; i < number_of_neighbors; ++i) {
    if(neighbors[i].index==index) {
      return neighbors[i].fakeIP;
    }
  }
}

int routeBySenderPortNum(int port) {
  printf("Routing by sender port num %d\n", port);

  for(int i = 0; i < number_of_neighbors; ++i) {
    //printf("%d %d\n", neighbors[i].sender_port, port);
    if(neighbors[i].sender_port == port)
    {
      //printf("Returning %d\n", i);
      return neighbors[i].index;
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

// Prints the routing table
void printRoutingTable(routerTableEntry * rtr_table) {
  printf("Destination_Index|Next_Hop_Index|Cost\n");
  for(int i = 0; i < total_no_hosts; ++i) {
    printf("%d|%d|%d\n",rtr_table[i].dst_index,rtr_table[i].nxt_hop_index,rtr_table[i].cost);
  }
  printf("\n");
}

void printNeighborTable() {
  printf("FakeIP|Rcvr_Port|Sndr_Port|Cost|Index_on_routing_table\n");
  for(int i = 0; i < number_of_neighbors; ++i) {
    printf("%s|%d|%d|%d|%d\n",neighbors[i].fakeIP,neighbors[i].port,
      neighbors[i].sender_port,neighbors[i].cost,neighbors[i].index);
  }
  printf("\n");
}

void parseFiles(FILE *configfile) {
  
  // This will be our fake IP address
  int i = 0;
  char x;

    // This will be used to copy things from the file
  char temp_string[15];


  // Get IP addr
  // Make sure the content is set to zero
  memset(my_ipaddr,0, 15);
  while(x != '\n') 
  {
      fread(&x, 1, 1, configfile);
      my_ipaddr[i++] = x; 
  }
  my_ipaddr[i-1] = 0;

  temp_string[0] = my_ipaddr[i-2];
  temp_string[1] = 0;
  my_index=atoi(temp_string);
  my_index--; // Compensate because array 0 indexed vs IP start at 1

  fread(&x, 1, 1, configfile);
  
  // Get port number
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

  // Get total number of hosts
  x='a';
  i=0;
  while(x != '\n') 
  {
      fread(&x, 1, 1, configfile);

      temp_string[i++] = x; 
  }
  temp_string[i-1] = 0;
  total_no_hosts = atoi(temp_string);
  //total_no_hosts--; // Compensate because i am a host

  fread(&x, 1, 1, configfile);
  memset(temp_string, 0, 6);

  // Get number of neighbors
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

  router_table = (routerTableEntry*) malloc(sizeof(routerTableEntry) * total_no_hosts );


  router_table_size = sizeof(routerTableEntry)*total_no_hosts;


  memset(router_table, 0, router_table_size);

  unsigned long neighbor_addr;

  neighborInfo n_info;

  routerTableEntry entry;
  for(int k = 0; k < number_of_neighbors; ++k) 
  {
    fread(&x, 1, 1, configfile);
  
    i=0;
    while(x != ' ') 
    {
      fread(&x, 1, 1, configfile);
      n_info.fakeIP[i++] = x; 
    }
    n_info.fakeIP[i-1] = 0;

    temp_string[0] = n_info.fakeIP[i-2];
    temp_string[1] = 0;

    n_info.index=atoi(temp_string);
    n_info.index--; // Compensate because array 0 indexed vs IP start at 1

    entry.dst_index=n_info.index;
    entry.nxt_hop_index=n_info.index;
    
    strcpy(temp_string, "127.0.0.1");

    neighbor_addr = inet_addr(temp_string);
    memcpy(&n_info.ip, &neighbor_addr, sizeof(struct in_addr));

    memset(temp_string,0, 6);
    i=0;

    x='a';
    for(int j = 0; j < 10; ++j)
      fread(&x, 1, 1, configfile);
  
    while(fread(&x, 1, 1, configfile) == 1) {
      if(x == ' ') {
          break;
      }
      temp_string[i++] = x;
    }
    temp_string[i] = 0;
    n_info.port = atoi(temp_string);

    memset(temp_string,0, 6);
    i=0;

    while(fread(&x, 1, 1, configfile) == 1) {
      if(x == '\n') {
          break;
      }
      temp_string[i++] = x;
    }
    temp_string[i] = 0;
    n_info.cost = atoi(temp_string);
    entry.cost=n_info.cost;

    printf("%s %d %d and %d\n", n_info.fakeIP, n_info.port, n_info.cost, entry.dst_index);
    memcpy(&neighbors[k], &n_info, sizeof(neighborInfo));
    memcpy(&router_table[entry.dst_index], &entry, sizeof(routerTableEntry));

    //printf("1%s %d\n", neighbors[k].fakeIP, neighbors[k].port);
  }



  for(int k = 0; k < total_no_hosts; ++k) {
    if(router_table[k].cost==0) {
      router_table[k].dst_index = k;
      router_table[k].nxt_hop_index=-1;
      router_table[k].cost=-1;
    }
    if(k==my_index) {
      router_table[k].dst_index = k;
      router_table[k].nxt_hop_index=k;
      router_table[k].cost=0;
    }
  }
  printRoutingTable(router_table);
  //printNeighborTable();
  
}