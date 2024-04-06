/*************************************************************************************************

Filename: main.cpp

Date Modified: 10/1/2017

Author: Kevin Volkel

Description: This file is the main file for the cache simulator. It is in 
charge of passing the command line arguments to the constructors of the instantiated Caches.
After initiating the Caches, the main file opens a specified text file that contains the trace
that will be used for testing. Each line is read from the trace file, and from each line a hex address
and a read/write command is extracted. These two things are then passed to the cache_in function for
processing. 

The newest version of the simulator supports 2 level cache hierarchy with a victim cache. More 
raw measurements were also added to this simulation, such as the number of swaps, victim cache write backs, 
the L2 cache miss rate, etc.



*****************************************************************************************************/
#include "cache.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>






int main(int argc, char** argv ){
  FILE* file;
  unsigned long long address;
  unsigned long long  holder;
  char rorw;
  char LINE_IN[1024];
  double replacement;
  double replacement_L2;
  float average;
  Cache* second=NULL;
  Cache* victim_cache=NULL;
  //print out header of report 
  printf("===== Simulator configuration =====\n");
  printf("L1_BLOCKSIZE:   %s\n",argv[1]);
  printf("L1_SIZE:        %s\n",argv[2]);
  printf("L1_ASSOC:       %s\n",argv[3]);
  printf("Victim_Cache_SIZE:    %s \n",argv[4]);
  printf("L2_SIZE:           %s\n",argv[5]);
  printf("L2_ASSOC:          %s\n",argv[6]);
  printf("trace_file:      %s\n",argv[8]);
  replacement=(double)atof(argv[7]);
  if(replacement==2){
    printf("Replacement Policy:   LRU\n");
    replacement_L2=2;
  }
  else if (replacement==3){
    printf("Replacement Policy:    LFU\n");
    replacement_L2=3;
  }
  else{
    printf("Replacement Policy:      LRFU\nlambda: %s\n",argv[7]);
    replacement_L2=2;
  }
  printf("===================================\n\n");
 
  
  // L2 instantiate 
  Cache L2(atoi(argv[1]),atoi(argv[5]),atoi(argv[6]),replacement_L2,0,NULL,2,"L2",NULL);
  if(atoi(argv[5])>0)second=&L2;
  //instantiate victim
  Cache victim(atoi(argv[1]),atoi(argv[4]),atoi(argv[4])/atoi(argv[1]),2,0,second,1,"Victim",NULL);
  if(atoi(argv[4])>0)victim_cache=&victim;
  //initiate the top level cache                                                                                                                                                                           
  Cache L1(atoi(argv[1]),atoi(argv[2]),atoi(argv[3]),replacement,0,second,1,"L1",victim_cache);

  //open the file and start passing data in to the cache
  file=fopen(argv[8],"r");
  if(file==NULL){
    printf("Error opening file\n");
    return 0;
  }
  while(fgets(LINE_IN,1024,file)!=NULL){
    sscanf(LINE_IN,"%c %llX",&rorw,&holder);
    address=(unsigned long long)holder;
    L1.cache_in(address,rorw);
  }
  //close the file
  fclose(file);
  //report final results of L1 Cache and possibly L2 and victim
  L1.report();
  if(victim_cache!=NULL) victim.report();
  if(second!=NULL) L2.report();



  //report final calculated results                                                                                                                                                                        
    
  printf("\n");
  printf("====== Simulation results (raw) ======\n\n");
  printf("a. number of L1 reads:     %i\n",L1.num_reads);
  printf("b. number of L1 read misses:    %i\n",L1.read_miss);
  printf("c. number of L1 writes:     %i\n",L1.num_writes);
  printf("d. number of L1 write misses:      %i\n",L1.write_miss);
  printf("e. L1 miss rate:   %.4f\n",L1.miss_rate);
  printf("f. number of swaps:     %i\n",L1.swaps);
  printf("g. number of victim cache writeback:   %i\n",victim.write_backs);
  printf("h. number of L2 reads:     %i\n",L2.num_reads);
  printf("i. number of L2 read misses:    %i\n",L2.read_miss);
  printf("j. number of L2 writes:     %i\n",L2.num_writes);
  printf("k. number of L2 write misses:      %i\n",L2.write_miss);
  if(second!=NULL)printf("l. L2 miss rate:   %.4f\n",L2.miss_rate);
  else printf("l. L2 miss rate:   0\n");
  printf("m. number of L2 writeback:  %i\n",L2.write_backs);
  if(second!=NULL){
    printf("n. total memory traffic:  %i\n",L2.mem_traffic);
  }
  else if(victim_cache!=NULL){
    printf("n. total memory traffic:  %i\n",victim.write_backs+L1.mem_traffic);
  }
  else{
    printf("n. total memory traffic:  %i\n",L1.mem_traffic);
  }
  printf("\n");
  printf("==== Simulation results (performance) ====\n");
  if(second==NULL){
    printf("1. average access time:    %.4f ns",L1.average_time);
  }
  else{
    printf("1. average access time:    %.4f ns",L1.hit_time+(L1.miss_rate)*L2.average_time);
  }
  return 0;
}
