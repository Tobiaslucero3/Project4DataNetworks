#include "project4.h"

// Sender thread function
void* sender(void* args) {

  int server_address_size = sizeof(sockaddr_in);

  char addr[16];
  memcpy(addr, my_ipaddr, strlen(my_ipaddr));
  // Get the first packet ready and send it
  
  int sockets[number_of_neighbors]; // We're gonna need a socket for each neighbor
  sockaddr_in servers[number_of_neighbors]; // And a server sockaddr_in for each neighbor
  sleep(2);

  // This loop opens all sockets with my neighbors and sends the first packet
  // to let my neighbors know my sender port
  for(int i = 0; i < number_of_neighbors; ++i) {
      if ((sockets[i] = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
      {
        printf("socket()");
        exit(1);
      }
      servers[i].sin_family      = AF_INET;            /* Internet Domain    */
      servers[i].sin_port        =  htons( (neighbors[i]).port );              /* Server Port        */
      servers[i].sin_addr.s_addr   = inet_addr("127.0.0.1");
      printf("Trying to send to port %d\n", htons(servers[i].sin_port));
      if (sendto(sockets[i], addr, sizeof(addr), 0, (struct sockaddr *)&servers[i],server_address_size) < 0)
      {
        printf("sendto()");
        exit(2);
      }
  }

  printf("%d\n", router_table_size);

  unsigned char buf[router_table_size+5];

  while(true) 
  {
    // Loop over all neighbors and see if they need a packet or acknowledgement sent
    for(int i = 0; i < number_of_neighbors; ++i) 
    {
      sleep(1);
      
      if (sendto(sockets[i], router_table, router_table_size, 0, (struct sockaddr *)&servers[i],server_address_size) < 0)
      {
        printf("sendto()");
        exit(2);
      }
    }
  }

  for(int i = 0; i < number_of_neighbors; ++i) {
    close(sockets[i]);
  }

}

// Receiver thread function
void* receiver(void* args) {

  int s, other_s, namelen, client_address_size;
  struct sockaddr_in client, server, other_server;

  /*
  * Create a datagram socket in the internet domain and use the
  * default protocol (UDP).
  */
  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
      printf("socket()");
      exit(1);
  }

  /*
    * Bind my name to this socket so that clients on the network can
    * send me messages. (This allows the operating system to demultiplex
    * messages and get them to the correct server)
    *
    * Set up the server name. The internet address is specified as the
    * wildcard INADDR_ANY so that the server can get messages from any
    * of the physical internet connections on this host. (Otherwise we
    * would limit the server to messages from only one network
    * interface.)
    */
  server.sin_family      = AF_INET;  /* Server is in Internet Domain */
  server.sin_port        = htons(my_port);         /* Use any available port      */
  server.sin_addr.s_addr = inet_addr("127.0.0.1");/* Server's Internet Address   */

  if (bind(s, (struct sockaddr *)&server, sizeof(server)) < 0)
  {
      printf("bind()");
      exit(2);
  }

  /* Find out what port was really assigned and print it */
  namelen = sizeof(server);
  if (getsockname(s, (struct sockaddr *) &server, (socklen_t *) &namelen) < 0)
  {
      printf("getsockname()");
      exit(3);
  }
  
  printf ("Server is running at this IP address %s\n and on port %d\n", 
            inet_ntoa(server.sin_addr), ntohs(server.sin_port));

  client_address_size = sizeof(client);

  char addr[16];
  int addrlen = sizeof(addr);

  // At this point all hosts will perform a packet exchange where theyll all get the sender port numbers of their neighbors
  for(int i = 0; i < number_of_neighbors; ++i) 
  {
    if((recvfrom(s, addr, addrlen, 0, (struct sockaddr *) &client, (socklen_t *) &client_address_size)) <0) 
    {
      printf("recvfrom()");
      exit(4);
    }

    printf("Recieved message from port: %d with ipaddr %s\n", htons(client.sin_port), addr);

    for(int j = 0; j < number_of_neighbors; ++j) 
    {
      if(compareAddresses(addr, neighbors[j].fakeIP)) 
      {
        // Set the neigbors sender port
        neighbors[j].sender_port = htons(client.sin_port);
      }
    }
  }

  unsigned char buf[router_table_size];

  routerTableEntry neighbor_router_table[total_no_hosts];

  int len = -1;
  int neighbor_index;

  while (true) { 
    
    buf[0] = '\0';
    // Receive from the client
    sleep(1);
    if((len = recvfrom(s, neighbor_router_table, router_table_size, 0, (struct sockaddr *) &client, (socklen_t *) &client_address_size)) <0)
    {
      printf("recvfrom()");
      exit(4);
    }

    neighbor_index = routeBySenderPortNum(ntohs(client.sin_port));
    if(neighbor_index==-1) {
      break;
    }
    
    printf("Received msg len=%d sender port %d which is addr %s\n", len,
      ntohs(client.sin_port),getAddrByIndexOnRouterTable(neighbor_index));


    printf("my routing table b4 update\n");
    printRoutingTable(router_table);

    int costToNeighbor = router_table[neighbor_index].cost;
    int costFromNeighborToNext, currentCostToNext, sumCost;
    for(int i = 0; i < total_no_hosts; ++i) {
      if((i!=my_index)&&(i!=neighbor_index)&&(neighbor_router_table[i].cost!=-1)) {
        
        costFromNeighborToNext=neighbor_router_table[i].cost;
        
        currentCostToNext=router_table[i].cost;
        
        sumCost = costToNeighbor + costFromNeighborToNext;

        if((currentCostToNext==-1)&&(costFromNeighborToNext!=-1)) {
          router_table[i].cost = sumCost;
          router_table[i].nxt_hop_index = neighbor_index;
        } else if((currentCostToNext!=-1)&&(sumCost < currentCostToNext)) {
          router_table[i].cost = sumCost;
          router_table[i].nxt_hop_index = neighbor_index;
        }
      }
    }

    printf("received this routing table\n");
    printRoutingTable(neighbor_router_table);

    printf("my routing table after update\n");
    printRoutingTable(router_table);
  }
}

/** I used pcap.h to parse through the pcap file */
int main(int argc, char** argv)
{
  //argv[1] = pcap file, argv[2] = config file,
  if(argc!=3)
  {
      printf("Usage: %s <pcap file> <config file> \n",argv[0]);
      exit(1);
  }

  /** OPENING CONFIG FILE */
  FILE *configfile;
  configfile = fopen(argv[2], "r");
  if (configfile == NULL) 
  { 
      fprintf(stderr, "\nError opening config file\n"); 
      exit(1); 
  }  


  parseFiles(configfile);


  fclose(configfile);

  /** PREPARING GLOBAL VARIABLES *//*
  queue<unsigned char*> frames_local;

  frames = &frames_local;

  neighbor_queues = new queue<unsigned char*>[number_of_neighbors];

  send_next = new int[number_of_neighbors];
  send_ack = new int[number_of_neighbors];
  acked = new int[number_of_neighbors];
  for(int i = 0; i < number_of_neighbors; ++i) {
    send_next[i] = 0;
    send_ack[i] = 0;
    acked[i] = 1;
  }

  sent_time = new clock_t[number_of_neighbors];

  /** THREADING */
  //pthread_t queue_watch_thread;

   // Create a thread with watch queue function
  //pthread_create(&queue_watch_thread, NULL, watchQueue, NULL);
  
  // Create the sender and receiver thread
  pthread_t sender_thread;
  pthread_create(&sender_thread, NULL, sender, NULL);

  pthread_t receiver_thread;
  pthread_create(&receiver_thread, NULL, receiver, NULL);

  /* PREPARATION TO READ FROM PCAP FILE */ /*
  char error_buffer[PCAP_ERRBUF_SIZE]; // Error buffer in case any errors with pcap
  pcap_t *pcap_t; // The instance of pcap which we will use to process the packets
  struct pcap_pkthdr *header; // This will be where the packet header goes
  const unsigned char *data; // This will be used to store the packet data

  // Try to open pcap offline (since we are using file) using filename
  if((pcap_t = pcap_open_offline(argv[1], error_buffer)) == NULL) {
    printf("Could not open pcap %s \n", error_buffer);
  }

  char src_ip[32];
  char dst_ip[32];
  struct in_addr ipaddr; 
  unsigned char* b;

  int index = -1;
  int len;

  sleep(10);

  /* LOOP TO READ FROM PCAP FILE */ /*
  while(((pcap_next_ex(pcap_t, &header, &data)) >= 0)) {
    // Extract the src ip addr from the pcap data
    memcpy(&ipaddr, &data[66], sizeof(in_addr));

    // Get the string version
    strcpy(src_ip, inet_ntoa(ipaddr));

    // Only send the packet that match my ip address
    if(compareAddresses(src_ip, my_ipaddr) == 1) {

      // Get the length of the packet (total number of bytes)
      len = header->len;

      // Get the destination IP address
      memcpy(&ipaddr, &data[70], sizeof(in_addr));
      strcpy(dst_ip, inet_ntoa(ipaddr));
    
      // Route the packet
      index = route(dst_ip);
      
      // Make sure that the destination is actually in our routing table
      if(index != -1) {
        b = (unsigned char*)malloc(len); // Allocate this so the frame exists even after i get the next pcap file
        memcpy(b, data, len);
        
        printf("Added a message into %s with destination %s queue index %d\n", neighbors[index].fakeIP, dst_ip, index);
        
        // Push the pointer to the data into the queue for the neighbor which routes this packet
        neighbor_queues[index].push(b);

        // We are ready for sending
        send_next[index] = 1;
      }
    }

  }

  printf("Got through all the pcap packets\n"); 

  /** FINISHED, JOINING THREADS AND FREEING MALLOC */
  pthread_join(sender_thread, NULL);
  pthread_join(receiver_thread, NULL);

  //pthread_join(queue_watch_thread, NULL);

  printNeighborTable();
  printRoutingTable(router_table);

  free(neighbors);
  free(router_table);

  return 0;
}


