#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"




typedef struct{
  uint64_t k, v;
}KV_PAIR;





/*
  This particular latmap is specialized toward 64-bit bitvector keys and uint64
  values. It is also hardcoded for a max of 32 layers. This is not meant to be a
  robust design, just a proof of concept.
*/
typedef union{
  struct{
    uint64_t keys[16];
    uint64_t vals[16];
  } kvs;
  struct{
    void* nodes[32];
  } pts;
}NODE_DATA;

typedef struct{
  NODE_DATA data;
  int size, depth;
}LATMAP_NODE;


typedef struct{
  uint64_t filter[32];
  LATMAP_NODE* roots[64];
}LATMAP_HEADER;



inline int getTopLevelBucket(uint64_t k){
  int pct = __builtin_popcountl(k);
  return (pct > 63)? 63 : pct;
}

inline int getBucket(uint64_t k, LATMAP_HEADER* h, int depth){
  if(depth > 32) return 0;
  int pct = __builtin_popcountl(k & h->filter[depth]);
  return (pct > 31)? 31 : pct;
}


void mkLatmap(uint64_t rngseed, uint64_t factor, LATMAP_HEADER* ret){
  uint64_t shiftRegister = rngseed;
  for(int i = 0; i < 32; i++){
    int j = 0;

    // Random number generator
    while((j < 64) || (__builtin_popcountl(shiftRegister) != 32)){
      j++;
      shiftRegister = (shiftRegister ^ ((0 - (shiftRegister & 1)) & factor));
      shiftRegister = (shiftRegister >> 1) | (shiftRegister << 63);
    }
    ret->filter[i] = shiftRegister;
  }

  // Nullify all buckets to start
  for(int i = 0; i < 64; i++) ret->roots[i] = NULL;
}


void insert(LATMAP_HEADER* latmap, uint64_t key, uint64_t val){

  // First off, figure out which top-level bucket to put it in
  int top = getTopLevelBucket(key);

  // If the top-level bucket node doesn't exist, allocate a node, insert, done.
  if(latmap->roots[top] == NULL){
    latmap->roots[top] = malloc(sizeof(LATMAP_NODE));
    latmap->roots[top]->size = 1;
    latmap->roots[top]->data.kvs.keys[0] = key;
    latmap->roots[top]->data.kvs.vals[0] = val;
    return;
  }

  // Iterate down the tree until a location is found.
  LATMAP_NODE* node = latmap->roots[top];
  int depth = 0;
  while(1){
    if(node->size < 16){ // Node has some free local space. Insert, done.
      node->data.kvs.keys[node->size] = key;
      node->data.kvs.vals[node->size] = val;
      node->size++;
      return;
    }
    if(node->size == 16){ // Node overflow! This is a tricky case!
      LATMAP_NODE* pts[32];
      for(int i = 0; i < 32; i++) pts[i] = NULL;

      uint16_t bucketset = 0;
      int buckets[17];
      for(int i = 0; i < 16; i++){
        buckets[i] = getBucket(node->data.kvs.keys[i], latmap, depth);
        bucketset |= (1 << buckets[i]);
      }
      buckets [16] = getBucket(key, latmap, depth);
      bucketset |= (1 << buckets[16]);
      if(__builtin_popcountl(bucketset) == 1){
        // Check for duplicate, replace if one is found. Done.
        for(int i = 0; i < 16; i++){
          if(node->data.kvs.keys[i] == key){
            node->data.kvs.vals[i] = val;
            return;
          }
        }

        // Not sure how to best handle this case, but it's pretty unlikely.
        // Let's ignore it for now.
        printf("Error at depth %i, bucket %i.\n", depth, buckets[16]);
        for(int i = 0; i < 16; i++){
          printf("  %lu\n", node->data.kvs.keys[i]);
        } printf("  %lu\n", key);
        exit(-1);
      }
      // Sort items into sub-buckets and return.
      for(int i = 0; i < 17; i++){
        if(pts[i] == NULL){
          pts[i] = malloc(sizeof(LATMAP_NODE));
          pts[i]->size = 1;
          pts[i]->data.kvs.keys[0] = node->data.kvs.keys[i];
          pts[i]->data.kvs.vals[0] = node->data.kvs.vals[i];
        }else{
          int ix = pts[i]->size;
          pts[i]->data.kvs.keys[ix]= node->data.kvs.keys[ix];
          pts[i]->data.kvs.vals[ix]= node->data.kvs.vals[ix];
          pts[i]->size++;
        }
      }
      node->size = 17;  // Full I guess
      for(int i = 0; i < 16; i++) node->data.pts.nodes[i] = pts[i];
      return;
    }
    // Node has no free space, we must go deeper.
    int bucket = getBucket(key, latmap, depth);
    LATMAP_NODE* newnode = node->data.pts.nodes[bucket];
    if(newnode == NULL){
      node->data.pts.nodes[bucket] = malloc(sizeof(LATMAP_NODE));
      ((LATMAP_NODE*)node->data.pts.nodes[bucket])->size = 0;
    }
    node = node->data.pts.nodes[bucket];
    depth++;
  }
}


int subsetLookup(uint64_t k, LATMAP_HEADER* latmap, int limit, KV_PAIR* ret){
  int maxBucket = getTopLevelBucket(k);
  int ct = 0, ix = maxBucket;
  while((ct <= limit) && (ix > 0)){

    //

  }
  return 0;
}


int supsetLookup(uint64_t k, LATMAP_HEADER* latmap, int limit, KV_PAIR* ret){
  return 0;
}


int betweenLookup(uint64_t ksup, uint64_t ksub, LATMAP_HEADER* latmap, int limit, KV_PAIR* ret){
  return 0;
}


uint64_t rstate = 718903561806071l;
uint64_t fastRNG(){
  rstate = (rstate * 58262467180937l) + 87501871526226381l;
  rstate ^= 3518971089716242429l;
  int rshift = rstate >> 58;
  rstate = (rstate >> rshift) | (rstate << rshift);
  return rstate;
}


void printbin(uint64_t x){
  for(int i = 63; i >= 0; i--)
    printf((x & (1l << i))? "#" : " ");
  printf("\n");
}


int main(){
  LATMAP_HEADER latmap;
  mkLatmap(891508971l, 0x8000000100300701, &latmap);
  for(int i = 0; i < 32; i++){
    printbin(latmap.filter[i]);
  }

  for(int i = 0; i < 1024; i++){
    uint64_t x = ((1l << ((fastRNG() >>  8) % 64)
                | (1l << ((fastRNG() >>  9) % 64))
                | (1l << ((fastRNG() >>  7) % 64))
                | (1l << ((fastRNG() >> 11) % 64))
                | (1l << ((fastRNG() >> 23) % 64))));
    printf("%i : %lu\n", i, x);
    insert(&latmap, x, i);
  }

  for(int i = 0; i < 64; i++){
    int x = 0;
    if(latmap.roots[i] != NULL){
      x = latmap.roots[i]->size;
    }
    printf("%i %i\n", i, x);
  }
}
