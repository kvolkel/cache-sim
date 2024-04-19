/**************************************************************************************************************************

Filename: cache.cpp

Date modified: 10/1/17

Author Kevin Volkel

Description: This file contains all of the implementations for the functions declared in the cache.h file.
The cache model supports 3 different replacement policies right now, LRU and LFU, and LRFU.
The cache model supports 2 different write policies WBWA, and WTNA, though for part B we only use WBWA.
When a cache needs to make a read or write to the next level, the cache_in function for the next lowest level is called with
a wrapper function that passes the appropriate address and the appropriate read/write command.
The most important function in this file is the cache_in function. This function takes an address and r/w 
command. The function then checks to see if there is a hit or miss and calls appropriate functions depending on the 
replacement policy specified for the instance of the model.

The newest version for part B also supports simulation of a vicitm cache and supports the addition of multiple levels of cache.



****************************************************************************************************************************/

#include "cache.h"
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//initiate the cache with input parameters
Cache::Cache(int blocksize, int size, int ass,double rep_polic, int wr_policy, Cache* next_cache, int level, char* name,Cache* victim_c){
  //initiate stats                                                                                                                    
  num_reads=0;
  num_writes=0;
  read_miss=0;
  write_miss=0;
  write_backs=0;
  miss_rate=0;
  swap=0;
  swaps=0;
  //counter for LRFU counts all reads and writes
  global_counter=0;
  //only set things if this instance of cache is enabled
  if(size>0){
    //load in cache parameters
    cache_name=name;
    blk_per_set=ass;
    num_sets=(size/(blocksize*ass));
    block_size=blocksize;
    assoc=ass;
    write_policy=wr_policy;
    //set for the appropriate replacment policy
    if(rep_polic==2){
      replace_policy=0;
    }
    else if(rep_polic==3){
      replace_policy=1;
    }
    else replace_policy=2;
    //set the lambda variable for LRFU
    lambda=rep_polic;
    //calculate number of bits for each field
    set_bits=(int)log2(num_sets);
    block_bits=(int)log2(block_size);
    tag_bits=32-(set_bits+block_bits);
    //initiate the cache, and set counters used in replacement policies
    set_array=(struct block**)malloc(num_sets*sizeof(struct block*));
    set_counters=(uint32_t*)malloc(num_sets*sizeof(uint32_t));
    for(int i=0; i<num_sets;i++){
      set_array[i]=(struct block*)malloc(blk_per_set*sizeof(struct block));
      for(int j=0;j<blk_per_set;j++){
	set_array[i][j].valid=0;
	set_array[i][j].dirty=0;
	set_array[i][j].age=0;
	set_array[i][j].print_LRU=0;
	set_array[i][j].last_time_stamp=0;
	set_array[i][j].CRF=0;
      }
      set_counters[i]=0;
    }
    //initiate the next level
    next_level=next_cache;
    //set victim cache
    victim=victim_c;
    
    //calculate miss_penalty and hit_time need to change for two level
    if(level==1){
      miss_penalty=20.0+0.5*((float)block_size/16.0);
      hit_time=0.25+2.5*(float(size)/524288.0)+0.025*((float)block_size/16.0)+0.025*(float)ass;
    }
    else if(level==2){
      miss_penalty=20.0+0.5*((float)block_size/16.0);
      hit_time=2.5+2.5*((float)size/524288.0)+0.025*((float)block_size/16.0)+0.025*(float)ass;
    }
  }
}

//function to read in a address and write/read command. figures out what to do with the request
void Cache::cache_in(unsigned long long address, char r_or_w){
  int tag;
  int set;
  int index_of_hit;
  int vh_index=-1;
  int block_tag=block_bits+tag_bits;
  unsigned long long victim_address;
  //set swap initially to 0
  swap=0;
  //increment the number of reads or number of writes variables
  read_or_write=r_or_w;
  if(r_or_w=='r') num_reads++;
  else num_writes++;
  //find the tag and set
  tag=((address)>>(set_bits+block_bits));
  set=((address<<tag_bits)&0x00000000FFFFFFFF)>>(block_tag);
  //if theres a victim cache look in it
  if(victim!=NULL) vh_index=victim->hit_or_miss(address>>block_bits,0);
  //look in the cache for the tag
  index_of_hit=hit_or_miss(tag, set);
  //increment the global counter
  global_counter++;
  if(vh_index==-1 || victim==NULL){ 
    //have a cache hit
    if(index_of_hit!=-1){
      //hit on a read
      if(r_or_w=='r'){
	//update counters for the cache depending on replacement policy
	if(replace_policy==0) LRU_update(tag, set, index_of_hit);
	else if(replace_policy==1) LFU_update(tag,set,index_of_hit);
	else LRFU_update(tag,set,index_of_hit);
      }
      
      //hit on a write
      else{
	//if WBWA dirty the block that was written to 
	if(write_policy==0) set_array[set][index_of_hit].dirty=1;
	else{
	  //issue write to next level for WTNA
	  issue_to_next(address,'w');
	}
	//on either write policy, update counters for the replacement policies
	if(replace_policy==0)LRU_update(tag,set,index_of_hit);
	else if(replace_policy==1) LFU_update(tag,set,index_of_hit);
	else LRFU_update(tag,set,index_of_hit);
      }
    }
    //we have a miss
    else{
      //read miss
      if(r_or_w=='r'){
	//increment read_miss counter and call the appropriate replacement policy replace function
	read_miss++;
	if(replace_policy==0) LRU_replace(tag,set, address,0);
	else if(replace_policy==1) LFU_replace(tag,set,address,0);
	else LRFU_replace(tag,set,address,0);
      }
      else{
	//write miss, increment write miss counter
	write_miss++;
	if(write_policy==0){
	  //only replace a block on a WBWA write policy
	  if(replace_policy==0) LRU_replace(tag,set,address,0);
	  else if(replace_policy==1) LFU_replace(tag,set,address,0);
	  else LRFU_replace(tag,set,address,0);
	}
	else{
	  //issue write to next level and do not replace a block (WTNA)
	  issue_to_next(address,'w');
	}
      }
    }
  }
  //found the address contents in the victim cache
  else if(victim!=NULL && vh_index!=-1){
    //we know the index we hit in the victim cache
    //need to issue a replace in the L1 cache
    //swap flag indicates that this rpelace call was for a swap
    //send in the victim hit index so stuff can be transferred
    swaps++;
    victim->LRU_update(address>>block_bits,0, vh_index);
    swap=1;
    if(replace_policy==0)LRU_replace(tag,set,address,vh_index);
    else if(replace_policy==1) LFU_replace(tag,set,address,vh_index);
    else LRFU_replace(tag,set,address,vh_index);
    swap=0;
  }
}




//function for updating on the LRU policy
void Cache::LRU_update(int tag, int set, int hit){
  int old_age=set_array[set][hit].age;
  update_Print_LRU(tag,set,hit);
  set_array[set][hit].age=0;
  //update the block counters who are valid and less than the old counter of the accessed block
  for(int i=0;i<blk_per_set;i++){
    if(set_array[set][i].age<old_age && set_array[set][i].valid==1 && i!=hit) set_array[set][i].age++;
  }
}




//function for updating on the LRFU policy                                                                                                                                                                 
void Cache::LRFU_update(int tag, int set, int hit){
  //calculate new CRF for accessed block
  set_array[set][hit].CRF=(double)1+(pow((1.0/2.0),((double)global_counter-(double)set_array[set][hit].last_time_stamp)*lambda)*set_array[set][hit].CRF);
  set_array[set][hit].last_time_stamp=global_counter;
}




//function to find the right block to evict in the set on LRFU policy
void Cache::LRFU_replace(int tag, int set, unsigned long long address, int victim_index){
  int index0;
  int found0;
  double lowest_CRF;
  int lowest_index=0;
  int hold_dirty;
  double temporary_CRF[blk_per_set];
  unsigned long long write_back_address=0;
  unsigned long long victim_address=0;
  found0=0;
  lowest_index=0;
  //look to see if vacant spot in the set
  for(int i=0; i<blk_per_set;i++){
    if(found0==0){
      //found a vacant block
      if(set_array[set][i].valid==0){
	index0=i;
	found0=1;
	//issue the read to the next level, only if you arent a vicitim cache
	if(strcmp(cache_name,"Victim")!=0)issue_to_next(address,'r');
	//update the block and dirty if it is a write
	set_array[set][i].tag=tag;
	//set CRF and last time ref
	set_array[set][i].CRF=1.0;
	set_array[set][i].last_time_stamp=global_counter;
	set_array[set][i].dirty=0;
	set_array[set][i].valid=1;
	
	//dirty the block if we need to
	if(read_or_write=='w') set_array[set][i].dirty=1;
      }
    }
  }
  //if did not find invalid to evict, look for a valid one
  if(found0==0){
    //calculate temporary CRF values
    for(int i=0; i<blk_per_set;i++){
      temporary_CRF[i]=(pow((1.0/2.0),((double)global_counter-(double)set_array[set][i].last_time_stamp)*lambda)*set_array[set][i].CRF);
    }
    lowest_CRF=temporary_CRF[0];
    //look for lowest CRF value
    for(int i=0; i<blk_per_set; i++){
      if(lowest_CRF>temporary_CRF[i]){
	lowest_CRF=temporary_CRF[i];
	lowest_index=i;
      }
    }
    //now we know the block who had the lowest temp CRF
    //If we are evicting and have a victim cache, need to call victim cache replace with the old block being replace                                                              
    if(swap==0 && victim!=NULL){
      victim_address=((unsigned long long)set_array[set][lowest_index].tag)<<(set_bits+block_bits);
      victim_address=victim_address+((unsigned long long)set<<block_bits);
      if(set_array[set][lowest_index].dirty==1) victim->read_or_write='w';
      else victim->read_or_write='r';
      victim->LRU_replace(victim_address>>block_bits,0,victim_address,0);
    }

    //only want to issue to a next level if this is not a swap
    if(swap==0){
      //if this block is dirty write it back, but only if it does not have a victim cache
      //victim cache handles all writebacks for a certain level
      if(set_array[set][lowest_index].dirty==1){
	if(victim==NULL){
	  //issue write back, need to construct the address into an appropriate value for the issue_to_next function call
	  write_back_address=((unsigned long long)set_array[set][lowest_index].tag&0x00000000FFFFFFFF)<<(set_bits+block_bits);
	  write_back_address=write_back_address+((unsigned long long)set<<block_bits);
	  issue_to_next(write_back_address,'w');
	  //increment write back counter
	  write_backs++;
	}
      }
      //issue read to lower level as long as you are not a vicitm cache
      if(strcmp(cache_name,"Victim")!=0)issue_to_next(address,'r');
    }

    //update the block with new information
    //if we have a swap we need to swap blocks with the victim
    //need to give the victim the cache's dirty bit, and need to update victim to have appropriate tag
    if(swap==1 && victim!=NULL){
      victim_address=((unsigned long long)set_array[set][lowest_index].tag)<<(set_bits+block_bits);
      victim_address=victim_address+((unsigned long long)set<<block_bits);
      hold_dirty=set_array[set][lowest_index].dirty;
      set_array[set][lowest_index].dirty=victim->set_array[0][victim_index].dirty;
      victim->set_array[0][victim_index].tag=victim_address>>block_bits;
      victim->set_array[0][victim_index].dirty=hold_dirty;
    }
    //load in a new block for the cache
    set_array[set][lowest_index].tag=tag;
    set_array[set][lowest_index].CRF=(double)1.0;
    set_array[set][lowest_index].last_time_stamp=global_counter;
    if(swap==0){
      set_array[set][lowest_index].dirty=0;
    }
      //If WBWA miss we need to dirty this new block, only if not a victim
    if(read_or_write=='w') set_array[set][lowest_index].dirty=1;
  }  
}





//function for updating on the LFU policy
void Cache::LFU_update(int tag, int set, int hit){
  //update the LRU order for printing
  update_Print_LRU(tag,set,hit);
  //when you hit on a LFU policy just update the age counter
  set_array[set][hit].age++;
}
    


//function to find the right block to evict in the set on LRU policy
void Cache::LRU_replace(int tag, int set, unsigned long long address, int victim_index){
  int index0;
  int found0;
  int oldest_age;
  int oldest_index;
  int hold_dirty;
  unsigned long long write_back_address=0;
  unsigned long long victim_address=0;
  found0=0;
  oldest_age=set_array[set][0].age;
  oldest_index=0;
  //look to see if vacant spot in the set
  for(int i=0; i<blk_per_set;i++){
    if(found0==0){
      //found a vacant block
      if(set_array[set][i].valid==0){
	index0=i;
	found0=1;
	//issue the read to the next level, only if you arent a vicitim cache
	if(strcmp(cache_name,"Victim")!=0)issue_to_next(address,'r');
	//update the block and dirty if it is a write
	set_array[set][i].tag=tag;
	//set age back to zero
	set_array[set][i].print_LRU=0;
	set_array[set][i].age=0;
	set_array[set][i].dirty=0;
	set_array[set][i].valid=1;
	//dirty the block if we need to
	if(read_or_write=='w') set_array[set][i].dirty=1;
      }
    }
  }
  //if did not find invalid to evict, look for a valid one
  if(found0==0){
    //look through all the blocks in the set and find the one with the highest counter
    for(int i=0; i<blk_per_set; i++){
      if(set_array[set][i].age>oldest_age){
	oldest_age=set_array[set][i].age;
	oldest_index=i;
      }
    }
    //If we are evicting and haev a victim cache, need to call victim cache replace with the old block being replaced                                                                                       
    if(swap==0 && victim!=NULL){
      victim_address=((unsigned long long)set_array[set][oldest_index].tag)<<(set_bits+block_bits);
      victim_address=victim_address+((unsigned long long)set<<block_bits);
      if(set_array[set][oldest_index].dirty==1) victim->read_or_write='w';
      else victim->read_or_write='r';
      victim->LRU_replace(victim_address>>block_bits,0,victim_address,0);
    }

    //only want to issue to a next level if this is not a swap
    if(swap==0){
      //now we know the oldest block in the set
      //if this block is dirty write it back, but only if it does not have a victim cache
      //victim cache handles all writebacks for a certain level
      if(set_array[set][oldest_index].dirty==1){
	if(victim==NULL){
	  //issue write back, need to construct the address into an appropriate value for the issue_to_next function call
	  write_back_address=((unsigned long long)set_array[set][oldest_index].tag&0x00000000FFFFFFFF)<<(set_bits+block_bits);
	  write_back_address=write_back_address+((unsigned long long)set<<block_bits);
	  issue_to_next(write_back_address,'w');
	  //increment write back counter
	  write_backs++;
	}
      }
      //issue read to lower level as long as you are not a vicitm cache
      if(strcmp(cache_name,"Victim")!=0)issue_to_next(address,'r');
    }

    //update the block with new information
    //if we have a swap we need to swap blocks with the victim
    //need to give the victim the caches dirty bit, and need to update victim to have appropriate tag
    if(swap==1 && victim!=NULL){
      victim_address=((unsigned long long)set_array[set][oldest_index].tag)<<(set_bits+block_bits);
      victim_address=victim_address+((unsigned long long)set<<block_bits);
      hold_dirty=set_array[set][oldest_index].dirty;
      set_array[set][oldest_index].dirty=victim->set_array[0][victim_index].dirty;
      victim->set_array[0][victim_index].tag=victim_address>>block_bits;
      victim->set_array[0][victim_index].dirty=hold_dirty;
    }
    //if we got here normally, then we are evicting a block
			      
    set_array[set][oldest_index].tag=tag;
    set_array[set][oldest_index].age=0;
    set_array[set][oldest_index].print_LRU=0;
    if(swap==0){
      set_array[set][oldest_index].dirty=0;
    }
      //If WBWA miss we need to dirty this new block, only if not a victim
    if(read_or_write=='w') set_array[set][oldest_index].dirty=1;
  } 
  if(found0==1) oldest_index=index0;
  //go through and update the age of all other valid blocks
  for(int i=0; i<blk_per_set; i++){
    if(set_array[set][i].valid==1 && i!= oldest_index){
      set_array[set][i].age++;
      set_array[set][i].print_LRU++;
    }
  }  
}







//function to find the index of the cache hit
int Cache::hit_or_miss(int tag, int set){
  //scan through all the blocks in the correct set
  //if tag matches and block is valid return the index for it
  for(int i=0; i<blk_per_set;i++){
    if(set_array[set][i].tag==tag && set_array[set][i].valid==1) return i;
  }
  //no match.... return -1
  return -1;
}






//function to replace a block on LFU policy
void Cache::LFU_replace(int tag, int set, unsigned long long address,int victim_index){
  int index0;
  int found0;
  int least_freq;
  int least_index;
  int hold_dirty;
  unsigned long long write_back_address=0;
  unsigned long long victim_address;
  found0=0;
  least_freq=set_array[set][0].age;
  least_index=0;
  //look to see if vacant spot in the set                                                                                                                                                                  
  for(int i=0; i<blk_per_set;i++){
    if(found0==0){
      //found a vacant block                                                                                                                                                                               
      if(set_array[set][i].valid==0){
        index0=i;
        found0=1;
        //issue the read to the next leveL
	if(strcmp(cache_name,"Victim")!=0)issue_to_next(address,'r');
        //update the block and dirty if it is a write                                                                                                                                                      
        set_array[set][i].tag=tag;
        //set block counter (age) to 1, since the set counter will be 0 if there are still vacant spots                                                                                                            
	set_array[set][i].print_LRU=0;   
        set_array[set][i].age=1;
        set_array[set][i].dirty=0;
        set_array[set][i].valid=1;
        if(read_or_write=='w') set_array[set][i].dirty=1;
      }
    }
  }
  //if did not find invalid to evict, look for a valid one                                        
  if(found0==0){
    //scan through all the blocks in a set
    //find the block with the smallest age counter (indicates least frequently used)
    for(int i=0; i<blk_per_set; i++){
      if(set_array[set][i].age<least_freq){
        least_freq=set_array[set][i].age;
        least_index=i;
      }
    }
    //if we are normally evicting a block then we have to place the evicted block into the victim cache                                                                                                     
    if(swap==0 && victim!=NULL){
      victim_address=((unsigned long long)set_array[set][least_index].tag)<<(set_bits+block_bits);
      victim_address=victim_address+((unsigned long long)set<<block_bits);
      if(set_array[set][least_index].dirty==1) victim->read_or_write='w';
      else victim->read_or_write='r';
      victim->LRU_replace((unsigned long long)victim_address>>block_bits,0,victim_address,0);
    }

    if(swap==0){
      //now we know the least frequent  block in the set                          
      //if this block is dirty write it back                                                        
      if(set_array[set][least_index].dirty==1 && victim==NULL){
	//issue write back       
	//calculate appropriate address for the next level cache
	write_back_address=((unsigned long long)set_array[set][least_index].tag)<<(set_bits+block_bits);
	write_back_address=write_back_address+((unsigned long long)set<<block_bits);
	issue_to_next(write_back_address,'w');
	write_backs++;
      }
      //issue read to lower level    
      if(strcmp(cache_name,"Victim")!=0)issue_to_next(address,'r');
    }
    //if we have a swap we need to swap blocks with the victim
    if(swap==1){
      victim_address=((unsigned long long)set_array[set][least_index].tag)<<(set_bits+block_bits);
      victim_address=victim_address+((unsigned long long)set<<block_bits);
      victim->set_array[0][victim_index].tag=victim_address>>block_bits;
      hold_dirty=set_array[set][least_index].dirty;
      set_array[set][least_index].dirty=victim->set_array[0][victim_index].dirty;
      victim->set_array[0][victim_index].dirty=hold_dirty;
    }
    
    //update the block with new information
    set_counters[set]=set_array[set][least_index].age;
    set_array[set][least_index].tag=tag;
    //set the new block's counter equal to the set counter +1
    set_array[set][least_index].age=set_counters[set]+1;
    update_Print_LRU(0,set,least_index);
    if(swap==0)set_array[set][least_index].dirty=0;
    //dirty the block if WBWA 
    if(read_or_write=='w') set_array[set][least_index].dirty=1;
  }

}

// print the contents of the cache and the report for statistics out
void Cache::report(){
  //calculate miss rate
  miss_rate=((float)write_miss+(float)read_miss)/((float)num_reads+(float)num_writes);
  //calculate the appropriate memory traffic depending on the write policy
  if(write_policy==0) mem_traffic=read_miss+write_miss+write_backs;
  else mem_traffic= read_miss+num_writes;
  average_time=hit_time+(miss_rate*miss_penalty);
  
  if(strcmp(cache_name,"Victim")!=0)printf("===== %s contents =====\n",cache_name);
  else printf("===== Victim Cache contents =====\n");
  //print the set, all the tags in the set, and if the block is dirty or not
  for(int i=0;i<num_sets;i++){
    printf("set     %i:   ",i);
    for(int j=0; j<blk_per_set;j++){
      if(set_array[i][j].valid==1){
	  printf(" %X   ",set_array[i][j].tag);
	  if(set_array[i][j].dirty==1)printf("  D  ");
      }
      else printf("  -   ");
    }
  
    printf("\n");
  }
}
  


//wrapper function to issue a read or write to the next level
void Cache::issue_to_next(unsigned long long address, char read_or_w){
  //if the pointer to the next level is not null issue a read or write
  if(next_level!=NULL) next_level->cache_in(address,read_or_w);
  
}





//function that keeps track of the LRU block
//mostly needed to adhere to the new printing requirements
void Cache::update_Print_LRU(int tag, int set, int hit){
  int old_LRU=set_array[set][hit].print_LRU;
  set_array[set][hit].print_LRU=0;
  //update the block counters who are valid and less than the old counter of the accessed block                                                                                                            
  for(int i=0;i<blk_per_set;i++){
    if(set_array[set][i].print_LRU<old_LRU && set_array[set][i].valid==1 && i!=hit) set_array[set][i].print_LRU++;
  }


}
