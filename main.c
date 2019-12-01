#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"



typedef struct{
  uint64_t k, v;
}KV;



typedef struct{
  union{
    void*    nodes[16];
    KV       pairs[16];
  }data;
  int      size;
}NODE;



typedef struct{
  uint64_t slicemaps[64];
}SLICEMAP;


uint64_t rng(uint64_t seed){
  seed = (seed * 15378915136106581) + 183578614119697;
  uint64_t rot = (seed >> 15) % 64;
  seed = (seed >> rot) | (seed << (64 - rot));
  return seed;
}

SLICEMAP newSlicemap(uint64_t seed){
  SLICEMAP ret;
  for(int i = 0; i < 64; i++){
    int cont = 1;
    uint64_t x = rng(i+56848);
    while(cont){
      x &= rng(x);
      int match = 0;
      if(__builtin_popcountl(x) == 15){
        cont = 0;
        for(int j = 0; j < i; j++){
          match |= (ret.slicemaps[j] == x)? 1 : 0;
        }
        uint64_t last = (i > 0)? 0 : ret.slicemaps[i-1];
        if(__builtin_popcountl(x & last) > 3){
          cont = 1;
        }
        cont |= match;
      }else{
        x = rng(x);
      }
    }
    ret.slicemaps[i] = x;
  }
  return ret;
}

void printBits(uint64_t x){
  for(int i = 0; i < 64; i++){
    char c = ((x >> i) & 1)? '#' : ' ';
    printf("%c", c);
  }
  printf(" %i\n", __builtin_popcountl(x));
}


uint64_t rngBits(uint64_t seed, int bits){
  while(1){
    seed = rng(seed) & rng(seed+1);
    if(__builtin_popcountl(seed) == bits) return seed;
  }
}


int order(SLICEMAP* sm, int depth, uint64_t k){
  int x = __builtin_popcountl(k & sm->slicemaps[depth]);
  x += (k & ~sm->slicemaps[depth]) != 0;
  x = (x < 2)? 0 : x - 1;
  return x;
}



void insert(SLICEMAP* sm, NODE* latmap, KV kv, int depth){
  int x = order(sm, depth, kv.k);

  if(latmap->size < 16){
    // Array insert
    latmap->data.pairs[latmap->size] = kv;
    latmap->size++;
  }else if(latmap->size == 16){
    // Phase change
    KV kvs[17];
    for(int i = 0; i < 16; i++){
      kvs[i] = latmap->data.pairs[i];
    }
    kvs[16] = kv;

    latmap->size = 17;
    for(int i = 0; i < 16; i++){
      latmap->data.nodes[i] = NULL;
    }
    for(int i = 0; i < 17; i++){
      insert(sm, latmap, kvs[i], depth+1);
    }
  }else{
    // Standard insert
    NODE* n = latmap->data.nodes[x];
    if(n == NULL){
      // Allocate new node
      n = malloc(sizeof(NODE));
      n->size = 0;
      latmap->data.nodes[x] = n;
    }

    insert(sm, n, kv, depth+1);
  }
}


void leftpad(int pad){
  for(int i = 0; i < pad; i++) printf(" ");
}


void printLatmap(NODE* latmap, int prefix, int depth){
  leftpad(depth);
  if(prefix >= 0) printf("%i ", prefix);

  if(latmap->size <= 16){
    printf("(Flat %i\n", latmap->size);
    for(int i = 0; i < latmap->size; i++){
      leftpad(depth+2);
      printBits(latmap->data.pairs[i].k);
    }
  }else{
    int nilct = 0;
    for(int i = 0; i < 16; i++){
      if(latmap->data.nodes[i] == NULL) nilct++;
    }
    printf("(Fork %i\n", 16-nilct);

    for(int i = 0; i < 16; i++){
      NODE* n = latmap->data.nodes[i];
      if(n != NULL){
        printLatmap(n, i, depth+2);
      }
    }
  }

  leftpad(depth);
  printf(")\n");
}


/*
  Depth-first-search
*/
int subset(SLICEMAP* sm, NODE* latmap, int depth, uint64_t k, KV* ret, int limit){
  if(latmap == NULL) return 0;
  if(latmap->size <= 16){
    int max = (limit > latmap->size)? latmap->size : limit;
    int ix  = 0;
    for(int i = 0; i < max; i++){
      if((latmap->data.pairs[i].k & k) == latmap->data.pairs[i].k){
        ret[ix] = latmap->data.pairs[i];
        ix++;
      }
    }
    return ix;
  }

  int x = order(sm, depth, k);
  int count = 0;
  for(int i = x; i >= 0; i--){
    int gain = subset(sm, latmap->data.nodes[i], depth+1, k, &ret[count], limit-count);
    count += gain;
    if(count >= limit){
      return limit;
    }
  }
  return count;
}



int main(){
  SLICEMAP sm = newSlicemap(51231);
  for(int i = 0; i < 64; i++){
    printBits(sm.slicemaps[i]);
  }

  int size = 4096;
  KV* kvs = malloc(sizeof(KV) * size);
  for(int i = 0; i < size; i++){
    kvs[i].k = rngBits(i, 12);
  }

  NODE latmap;
  latmap.size = 0;

  for(int i = 0; i < size; i++){
    insert(&sm, &latmap, kvs[i], 0);
  }

  printLatmap(&latmap, -1, 0);

  for(int i = 0; i < 16; i++){
    uint64_t k = rng(513+i);
    KV buffer[16];
    int x = subset(&sm, &latmap, 0, k, buffer, 16);
    printf("%i:\n", x);
    printf("--");
    printBits(k);
    for(int i = 0; i < x; i++){
      printf("==");
      printBits(buffer[i].k);
    }
  }

}
