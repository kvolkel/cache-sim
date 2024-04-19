/**************************************************************************************************************

Filename:     cache.h


Date Modified: 10/1/17


Author: Kevin Volkel


Description: This file is the header file for the cache class. It specifies
all variables that are associated with the cache class. This class is used to instantiate a
general cache memory that can be configured with various parameters. This file also contains a struct
that is used to represent 1 block in a cache. The structure contains fields such as tag, dirty, and valid, and
a counter variable used to keep track of the age of a block in a set for the two different replacement policies.

More counters for added to this class for part B of the project. This includes the CRF counter, global counter, 
and the last time stamp counter. All of these are needed for the LRFU replacement policy. More functions were also
added in order to support LRFU, and the new required feature of the victim cache.

*****************************************************************************************************************/
#include <stdint.h>

//structure that represents a cache block
struct block{
  int tag;
  int age;
  int dirty;
  int valid;
  int print_LRU;
  double CRF;
  int last_time_stamp;
};



//Class that represents an instance of cache
class Cache{
 private:
  //variables that hold the configuration of the cache and the number of bits needed for each field
  int assoc;
  int block_size;
  int blk_per_set;
  int num_sets;
  int set_bits;
  int tag_bits;
  int block_bits;
  int write_policy;
  int replace_policy;
  //This pointer is used to point to the cache. Each element is a pointer to an array of block structures. Each element represents a set. 
  struct block** set_array;
  //counters for the sets needed for LFU
  uint32_t* set_counters;
  //pointer to a Cache class instance, specifically the cache directly underneath the Cache level that this variable is in.
  Cache* next_level;
  Cache* victim;
 public:
  //counter for vicitm cache swaps
  int swaps;
  //counter to count reads and writes
  int global_counter;
  //flag it indicate swap occuring
  int swap;
  //variable to hold lambda value
  double lambda;
  //variables that are used to calculate statistics about the cache
  float average_time;
  float miss_rate;
  float miss_penalty;
  float hit_time;
  char read_or_write;
  int num_reads;
  int num_writes;
  int write_miss;
  int read_miss;
  int write_backs;
  int mem_traffic;
  char* cache_name;
  //functions that are used to implement the replace and write policies. 
  Cache(int blocksize, int size, int ass,double  rep_policy, int wr_policy, Cache* next_cache, int level,char* name, Cache* victim_c);
  void LRU_update(int tag, int set, int hit);
  void update_Print_LRU(int tag, int set, int hit);
  void LFU_update(int tag, int set, int hit);
  void LRFU_update(int tag, int set, int hit);
  void LRU_replace(int tag, int set, unsigned long long address,int victim_index);
  void LFU_replace(int tag, int  set, unsigned long long address,int victim_index);
  void LRFU_replace(int tag, int  set, unsigned long long address,int victim_index);
  //funciton that inputs a address to the cache
  void cache_in(unsigned long long address, char r_or_w);
  //function to check to see if there is a hit on an address
  int hit_or_miss(int tag, int set);
  //wrapper function that calls cache_in for the next level of cache
  void issue_to_next(unsigned long long address, char read_or_w);
  //reports out the statistics of the cache level, eg. miss rate, average access time
  void report();
};
