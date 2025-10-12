#include "predictor.h"

/////////////////////////////////////////////////////////////
// 2bitsat
/////////////////////////////////////////////////////////////

/*
  0 - strong NT
  1 - weak NT
  2 - weak T
  3 - strong T
*/
#define STRONG_NT   0
#define WEAK_NT     1
#define WEAK_T      2
#define STRONG_T    3

#define TWO_BIT_SAT_TABLE_BITS  8192
#define TWELVE_LSB_MASK         0xFFF
int *state_table_2bitsat;
void InitPredictor_2bitsat() {
  /*
    8192/2 = 4096 counters
    easier to implement indexing in this way...
  */
  state_table_2bitsat = (int*)malloc(TWO_BIT_SAT_TABLE_BITS/2*sizeof(int)); 
  int i = 0;
  // init all counters to weak not taken
  while(i < TWO_BIT_SAT_TABLE_BITS/2){
    *(state_table_2bitsat + i) = WEAK_NT;
    i++;
  }
}

bool GetPrediction_2bitsat(UINT32 PC) {
  /*
    get the 12 LSB of PC to index state table
    2^12 = 4096 possible indices
  */
  int index = (int)(PC & (UINT32)TWELVE_LSB_MASK);
  int state = *(state_table_2bitsat + index);
  return state <= WEAK_NT ? NOT_TAKEN : TAKEN;
}

void UpdatePredictor_2bitsat(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  // get the state of counter
  int index = (int)(PC & (UINT32)TWELVE_LSB_MASK);
  int state = *(state_table_2bitsat + index);
  // update the counter
  if (resolveDir == TAKEN) {
    state = ((state + 1) > STRONG_T ? STRONG_T : (state + 1));
  } else {
    state = ((state - 1) < STRONG_NT ? STRONG_NT : (state - 1));
  }
  *(state_table_2bitsat + index) = state;
}

/////////////////////////////////////////////////////////////
// 2level
/////////////////////////////////////////////////////////////

/*
Storage analysis:
BHT table:
  512 entries, 6 bit entries --> 512 bytes
  (CACTI pureRAM must allocate entries in bytes)
PHT table:
  8 tables of 64 entries, 2 bit counter/entry
  --> 8*64 = 512 bytes
total bytes = 512 + 512 = 1024
**block size = 1 (1 byte/entry)
**bus width = 8 (8 bits/entry)
*/

#define NUM_BHT_TABLE_ENTRIES   512
#define NUM_BHR_BITS            6
#define NUM_PHT_TABLES          8
#define PHT_INDEX_MASK          0x7
#define BHT_INDEX_MASK          0xFF8   // bits 3-11
#define BHT_INDEX_SHIFT         3
#define BHR_MASK                0x3F    // 6 bits
#define PHT_CTR_MASK            0b0011  // 2 bits
int *bht_table_2level;
int **pht_tables_2level;
void InitPredictor_2level() {
  /*
    setup tables:
      bht: 512 entries, each entry is 6-bit BHR
      pht: 8 tables, each table has 2^6=64 2-bit sat counters

    indexing scheme:
      PC[0:3] (3LSB) indexes pht to select one of 8 tables
      PC[3:12] (bits 3-11, total 9 bits) indexes bht to select on of 512 entries
      6-bit BHR indexes chosen pht table to select one of 64 2-bit sat counters
  */
  int num_counters_per_table = 1 << NUM_BHR_BITS; // 1 << 6 = 64
  bht_table_2level = (int*)malloc(NUM_BHT_TABLE_ENTRIES*sizeof(int));
  pht_tables_2level = (int**)malloc(NUM_PHT_TABLES*sizeof(int*));

  /*
    TODO: init all BHRs to 0 --- for consistency
  */

  /*
    pht_tables_2level is a pointer to an array of 8 pointers
    those 8 pointers will each point to the start of a pht table (below)
  */
  for (int i = 0; i < NUM_PHT_TABLES; i++){
    int **pht_table = pht_tables_2level + i;
    *pht_table = (int*)malloc(num_counters_per_table*sizeof(int));

    // init all counters in table to weak NT
    int *pht_table_start = *pht_table;
    for (int j = 0; j < num_counters_per_table; j++){
      *(pht_table_start + j) = WEAK_NT;
    }
  }
}

bool GetPrediction_2level(UINT32 PC) {
  // get indices, tables, state
  int pht_index = (int)(PC & (UINT32)PHT_INDEX_MASK);
  int bht_index = (int)((PC & (UINT32)BHT_INDEX_MASK) >> BHT_INDEX_SHIFT);
  int bhr = *(bht_table_2level + bht_index) & BHR_MASK;
  int *pht_table = *(pht_tables_2level + pht_index);
  int state = *(pht_table + bhr) & PHT_CTR_MASK;
  // return prediction
  return state <= WEAK_NT ? NOT_TAKEN : TAKEN;
}

// #define TAKEN_BIT       0x01
// #define NOT_TAKEN_BIT   0x00
void UpdatePredictor_2level(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  // get indices, tables, state
  int pht_index = (int)(PC & (UINT32)PHT_INDEX_MASK);
  int bht_index = (int)((PC & (UINT32)BHT_INDEX_MASK) >> BHT_INDEX_SHIFT);
  int bhr = *(bht_table_2level + bht_index) & BHR_MASK;
  int *pht_table = *(pht_tables_2level + pht_index);
  int state = *(pht_table + bhr) & PHT_CTR_MASK;
  // update the counter and update bhr history
  int new_bhr;
  if (resolveDir == TAKEN) {
    state = ((state + 1) > STRONG_T ? STRONG_T : (state + 1));
    new_bhr = (bhr << 1) | 0x01; // shift in 'taken' history
  } else {
    state = ((state - 1) < STRONG_NT ? STRONG_NT : (state - 1));
    new_bhr = (bhr << 1); //| NOT_TAKEN_BIT; // shift in 'not taken' history
  }
  // truncate BHR to 6 bits again
  new_bhr = new_bhr & BHR_MASK;
  // update state and tables
  *(pht_table + bhr) = state;
  *(bht_table_2level + bht_index) = new_bhr;
}

/////////////////////////////////////////////////////////////
// openend
/////////////////////////////////////////////////////////////

void InitPredictor_openend() {

}

bool GetPrediction_openend(UINT32 PC) {

  return TAKEN;
}

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {

}

