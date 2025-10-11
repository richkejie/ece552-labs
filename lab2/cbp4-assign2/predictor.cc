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
#define NUM_BHT_TABLE_ENTRIES   512
#define NUM_BHR_BITS            6
#define NUM_PHT_TABLES          8
#define PHT_INDEX_MASK          0x7
#define BHT_INDEX_MASK          0xFF8   // bits 3-11
#define BHT_INDEX_SHIFT         3
#define BHR_MASK                0x3F    // 6 bits
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
  int num_counters_per_table = 2 << NUM_BHR_BITS; // 64
  bht_table_2level = (int*)malloc(NUM_BHT_TABLE_ENTRIES*sizeof(int));
  pht_tables_2level = (int**)malloc(NUM_PHT_TABLES*sizeof(int*));
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
  int state = *(pht_table + bhr);
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
  int state = *(pht_table + bhr);
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

/*
Storage: max 128Kbits = 128 * 1024 = 131,072 bits
*/

/*
TAGE implementation:
1) history lengths:
    alpha = 2, L(i) = (int)(2^(i-1)*L(1) + 0.5), L(1) = 2 (from paper)
    0 <= i <= 8: {0, 2, 4, 8, 16, 32, 64, 128, 256}-bit length histories
2) table sizes:
    2^0 entries
    2^2 entries
    2^4 entries
    ...
    2^256 entries (?)

T_Block{
  ctr: pointer to array of counters --> for bimodal prediction
  tag: pointer to array of tags
  u: pointer to array of useful counters, u
}

Notation:
  provider component: T_Block that gives the prediction
  alternate prediction (altpred): prediction block if miss on provider component
  if no hitting component, altpred is default prediction (T0)

3) useful counter, u, 2-bits
    when altpred != pred:
      if pred is correct: u++
      else: u--
    reset u every <period> branches

4) prediction counter, ctr, 3-bits
    +1/saturating on correct prediction

if incorrect prediction:
    update provider component, Ti, pred: ctr--
    if for Ti, i < M, allocate entry on Tk, k > i:
      0) read M-i-j u_j useful counters from Tj, i < j < M
      A) priority for allocation:
        1) if there is k, s.t. u_k = 0, then Tk is allocated; else
        2) u from Tj, i < j < M, all decremented, no new entry allocated
      B) avoiding ping-phenomenon:
        if both Tj, Tk, j < k, can be allocated (u_j = u_k = 0),
        then Tj chosen with higher probability than Tk
          --> implement this using simple linear feedback register
      C) initializing allocated entry:
        set prediction ctr to weak correct
        set u to 0 ('strong not useful')



*/


void InitPredictor_openend() {

}

bool GetPrediction_openend(UINT32 PC) {

  return TAKEN;
}

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {

}

