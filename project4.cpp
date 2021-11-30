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

  struct in_addr ip_src;  
  int len = 0;
  unsigned char ack[4]; // The ACK
  while(true) 
  {
    // Loop over all neighbors and see if they need a packet or acknowledgement sent
    for(int i = 0; i < number_of_neighbors; ++i) 
    {
      sleep(1);

      // This print statement tells you a lot of info about the current status of the queues
      printf("i: %d send-next %d send_ack %d Acked %d, queue sz: %ld\n",
       i, send_next[i], send_ack[i], acked[i], neighbor_queues[i].size());

      // Either we need to send an acknowledgment or its time to send a packet
      if( (send_ack[i]==1) || ( (!neighbor_queues[i].empty()) && (send_next[i] == 1) && (acked[i] == 1) ) ) 
      {
        // We have to send ACK
        if(send_ack[i] == 1) 
        {
          printf("Sending ACK to %s\n", neighbors[i].fakeIP);

          if (sendto(sockets[i], ack, sizeof(ack), 0, (struct sockaddr *)&servers[i],server_address_size) < 0)
          {
            printf("sendto()");
            exit(2);
          }
          // Set send_ack to 0
          send_ack[i] = 0;
        } 
        // We have to send a normal packet
        else 
        {
          printf("Sending a packet to %s size: %d, queue size: %ld\n", neighbors[i].fakeIP, len, neighbor_queues[i].size());

          if (sendto(sockets[i], neighbor_queues[i].front(), 138, 0, (struct sockaddr *)&servers[i],server_address_size) < 0)
          {
            printf("sendto()");
            exit(2);
          }

          // We need an ACK now
          acked[i] = 0;
          // We don't wanna send next again, we need to wait for ACK
          send_next[i] = 0;
          sent_time[i] = clock();
        }
      }
    }
    
    for(int i = 0; i < number_of_neighbors; ++i) 
    {
      if(acked[i] == 0)
      {
        if((clock() - sent_time[i]) > (3*10^20)) 
        {
          printf("Resending the packet\n");
          // Resend our packet
          send_next[i] = 1;
          acked[i] = 1;
        }
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

  struct in_addr ipaddr; 
  char dst_ip[32]; 
  int sndr_index, rcvr_index, len;

  unsigned char ack[4]; // The ACK 
  unsigned char buf[256]; // To receive with

  unsigned char* b; // This will be used to malloc

  int* acks_sent = new int[number_of_neighbors]; // To skip ACK
  for(int i = 0; i < number_of_neighbors; ++i) {
    acks_sent[i] = 1;
  }
  
  while (true) { 
    
    buf[0] = '\0';
    // Receive from the client
    sleep(1);
    if((len = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *) &client, (socklen_t *) &client_address_size)) <0)
    {
      printf("recvfrom()");
      exit(4);
    }
    
    printf("Received message with len %d\n", len);
    
    // It's not an ACK
    if(len > 100) {
      // We received a packet therefore we are going to decide if we drop the ACK (and not forward the packet or print it)
      sndr_index = routeBySenderPortNum(htons(client.sin_port));
      if(sndr_index != -1) {
        // We have to drop the ACK
        if((acks_sent[sndr_index]%3 == 0)&&(acks_sent[sndr_index]!=0)) 
        {
            acks_sent[sndr_index] = 1;
            send_ack[sndr_index] = 0;
            printf("Dropped the ACK\n");
        }
        // We dont have to drop ACK so forward packet or print
        else 
        {
          // Need to check dst address which starts at byte 70
          memcpy(&ipaddr, &buf[70], sizeof(in_addr));

          strcpy(dst_ip, inet_ntoa(ipaddr));
          
          printf("Received packet src port: %d destination %s, my ipaddr %s\n",htons(client.sin_port), dst_ip, my_ipaddr);

          b = (unsigned char*)malloc(len);
          memcpy(b, buf, len);

          // Check if im the destination
          if(compareAddresses(dst_ip, my_ipaddr) == 1) {
              // Push the frame into the queue
              (*frames).push(b);
              printf("Pushed a frame into Q\n");
          }
          else {
            rcvr_index = route(dst_ip); // Route the packet
            if(rcvr_index != -1) {
              printf("Routing packet with dst %s got index %d, ip addr %s as nxt hop\n", dst_ip, rcvr_index, neighbors[rcvr_index].fakeIP);
              neighbor_queues[rcvr_index].push(b);
              send_next[rcvr_index] = 1;
            }
            else {
              free(b);
              printf("Could not forward\n");
            }
          }

          send_ack[sndr_index] = 1;
          printf("Turned ACK flag on for %d queue\n", sndr_index);
          acks_sent[sndr_index]++;
        }
      }
      
    }
    
    // It is an ACK
    else {
      sndr_index = routeBySenderPortNum(htons(client.sin_port));

      if(sndr_index!=-1) {
        printf("Received acknowledgement from ipaddr %s\n", neighbors[sndr_index].fakeIP);
        free(neighbor_queues[sndr_index].front());
        neighbor_queues[sndr_index].pop();
        acked[sndr_index] = 1;
        ack_recv_time[sndr_index] = clock();
        send_next[sndr_index] = 1;
      }
    }
  }
  /*
  * Deallocate the socket.
  */

  close(s);
}

/** I used pcap.h to parse through the pcap file */
int main(int argc, char** argv)
{
  //argv[1] = pcap file, argv[2] = config file, argv[3] = routing table
  if(argc!=4)
  {
      printf("Usage: %s <pcap file> <config file> <routing info file> \n",argv[0]);
      exit(1);
  }

  /** OPENING SWITCH AND CONFIG FILE */
  FILE *configfile;
  configfile = fopen(argv[2], "r");
  if (configfile == NULL) 
  { 
      fprintf(stderr, "\nError opening config file\n"); 
      exit(1); 
  }  

  FILE *switchfile;
  switchfile = fopen(argv[3], "r");
  if (switchfile == NULL) 
  {
      fprintf(stderr, "\nError opening switch file\n"); 
      exit(1); 
  }

  parseFiles(configfile, switchfile);

  fclose(configfile);
  fclose(switchfile);

  /** PREPARING GLOBAL VARIABLES */
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
  pthread_t queue_watch_thread;

   // Create a thread with watch queue function
  pthread_create(&queue_watch_thread, NULL, watchQueue, NULL);
  
  // Create the sender and receiver thread
  pthread_t sender_thread;
  pthread_create(&sender_thread, NULL, sender, NULL);

  pthread_t receiver_thread;
  pthread_create(&receiver_thread, NULL, receiver, NULL);

  /* PREPARATION TO READ FROM PCAP FILE */
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

  /* LOOP TO READ FROM PCAP FILE */ 
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

  pthread_join(queue_watch_thread, NULL);

  free(neighbors);
  free(router_table);

  return 0;
}

