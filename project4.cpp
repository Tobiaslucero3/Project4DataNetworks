#include "project4.h"


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

  free(neighbors);
  free(router_table);

  return 0;
}

  /** PREPARING GLOBAL VARIABLES *//* *
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

  /** THREADING *//*
  pthread_t queue_watch_thread;

   // Create a thread with watch queue function
  pthread_create(&queue_watch_thread, NULL, watchQueue, NULL);
  
  // Create the sender and receiver thread
  pthread_t sender_thread;
  pthread_create(&sender_thread, NULL, sender, NULL);

  pthread_t receiver_thread;
  pthread_create(&receiver_thread, NULL, receiver, NULL);

  /* PREPARATION TO READ FROM PCAP FILE *//*
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

  /** FINISHED, JOINING THREADS AND FREEING MALLOC *//*
  pthread_join(sender_thread, NULL);
  pthread_join(receiver_thread, NULL);

  pthread_join(queue_watch_thread, NULL);



  return 0;*/


