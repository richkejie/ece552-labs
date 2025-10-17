#include "predictor.h"
#include <cassert>

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
  ctr: pointer to array of counters --> for ctr prediction
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
/*
Storage Analysis
GHR = 256 bits
T0 = (T0_num_entries * 3)  bits = 512 * 2  = 1024
Ti = (Ti_num_entries * 16) bits = 1024 * 16 = 16,384
Ti, i = [1,8] --> 16,384 * 8 = 131,072
total = 256 + 1024 + 131,072 = 132,352 bits = 129.25Kbits
*/

/* 
below is not completely faithful TAGE implementation
some simplifications were done
*/

#define NUM_TBLOCKS             9
#define INIT_CTR_STATE          3 
#define INIT_USABILITY_LEVEL    0  
#define GHR_BITS                256
#define TAG_BITS                11
#define RESET_PERIOD            500000

// prediction counter ranges
#define CTR_BITS                 3 // 3 bits for Ti, i!=0
#define CTR_BITS_T0              2 // 2 bits fot T0

// hash parameters
#define H1_FOLD_WIDTH           13
#define H1_FOLD_MASK            0x00001FFF
#define H2_FOLD_WIDTH           7
#define H2_FOLD_MASK            0x00001FFF

//Data structures
typedef struct TBlock{
  int     *ctr;   // 3 bits
  UINT32  *tag;   // 11 bits
  int     *u;     // 2 bits
} TBlock; // total bits = 3 + 11 + 2 = 16 bits


//Function prototypes
void incr_u_ctr(int, int);
void decr_u_ctr(int, int);
void allocate(int, int, bool, UINT32, int);
void update_ctr(int, int, int, bool);
bool get_prediction(int, int);
void update_GHR(bool);
UINT32 hash_fcn(UINT32, int, int, UINT32);

//Global variables
UINT32 GHR[GHR_BITS/32] = {0};
TBlock **TBLOCKS;
int PROVIDER_COMPONENT;
UINT32 H1[NUM_TBLOCKS];
UINT32 H2[NUM_TBLOCKS];
int HISTORY_LENGTHS[NUM_TBLOCKS]  = {0,2,4,8,16,32,64,128,256};
int TBLOCK_SIZES[NUM_TBLOCKS]     = {512,1024,1024,1024,1024,1024,1024,1024,1024};
// int BRANCH_COUNTER = 0;

void InitPredictor_openend() {
  // allocate memory to simulate T Blocks
	TBLOCKS = (TBlock**)malloc(NUM_TBLOCKS*sizeof(TBlock*));
	for(int i = 0; i < NUM_TBLOCKS; i++){
		TBLOCKS[i] = (TBlock*)malloc(sizeof(TBlock));
		TBLOCKS[i]->ctr       = (int*)malloc(TBLOCK_SIZES[i]*sizeof(int));
		TBLOCKS[i]->tag       = (UINT32*)malloc(TBLOCK_SIZES[i]*sizeof(UINT32));
		TBLOCKS[i]->u         = (int*)malloc(TBLOCK_SIZES[i]*sizeof(int));
		for(int j = 0; j < TBLOCK_SIZES[i]; j++){
			TBLOCKS[i]->ctr[j]      = (int)INIT_CTR_STATE;
			TBLOCKS[i]->u[j]        = (int)INIT_USABILITY_LEVEL;
			TBLOCKS[i]-> tag[j]     = (UINT32)0;
		}
	}
}

bool GetPrediction_openend(UINT32 PC) {
	// BRANCH_COUNTER++;

	bool prediction;
  // get base prediction from T0:
	prediction = get_prediction((TBLOCKS[0]->ctr)[PC%TBLOCK_SIZES[0]], CTR_BITS_T0);

  // compute hashes
  // then if hash2 hits (matches tag), update prediction
  // use prediction of tag hitting T Block that uses longest history
  PROVIDER_COMPONENT = 0;
	for(int i = NUM_TBLOCKS-1; i > 0; i--){
		H1[i] = hash_fcn(PC, HISTORY_LENGTHS[i], H1_FOLD_WIDTH, H1_FOLD_MASK)%(TBLOCK_SIZES[i]);
		H2[i] = hash_fcn(PC, HISTORY_LENGTHS[i], H2_FOLD_WIDTH, H2_FOLD_MASK)%(0x00000001<<TAG_BITS);		
		if((TBLOCKS[i]->tag[H1[i]]) == H2[i]){  //Get the prediction if tags match
			prediction = get_prediction(TBLOCKS[i]->ctr[H1[i]], CTR_BITS);
      PROVIDER_COMPONENT = i;
			break;
		} else {
			continue;
		}
	}
	return prediction;
}

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  // update prediction counter
	if(PROVIDER_COMPONENT == 0){
		update_ctr(PROVIDER_COMPONENT,PC%TBLOCK_SIZES[0],CTR_BITS_T0,resolveDir);
	} else {
		update_ctr(PROVIDER_COMPONENT,H1[PROVIDER_COMPONENT],CTR_BITS,resolveDir);
	}

  // if prediction correct, increment useful counter, u
	if(predDir == resolveDir){     
		incr_u_ctr(PROVIDER_COMPONENT,H1[PROVIDER_COMPONENT]);
	} else {		
  // otherwise, find an entry to allocate, if can't find entry decrement useful counter, u
		if(PROVIDER_COMPONENT != (NUM_TBLOCKS-1)){ // if prediction came from last T Block, skip
			for(int i = PROVIDER_COMPONENT + 1; i < NUM_TBLOCKS; i++){
				if((TBLOCKS[i]->u)[H1[i]] == 0){
					allocate(i,H1[i],resolveDir,H2[i],INIT_USABILITY_LEVEL);
					break;
				} else {
					decr_u_ctr(i,H1[i]);
				}
			}
		}
	}
  
  // update history
	update_GHR(resolveDir);
}

void incr_u_ctr(int block_i, int i){
  int state = TBLOCKS[block_i]->u[i];
	TBLOCKS[block_i]->u[i] = ((state + 1) > 3) ? 3 : (state + 1);
}

void decr_u_ctr(int block_i, int i){
  int state = TBLOCKS[block_i]->u[i];
	TBLOCKS[block_i]->u[i] = ((state - 1) < 0) ? 0 : (state - 1);
}

void allocate(int block_i, int index, bool taken, UINT32 tag, int u){
	TBLOCKS[block_i]->tag[index] = tag;
	TBLOCKS[block_i]->u[index] = u;
	TBLOCKS[block_i]->ctr[index] = (taken == TAKEN) ? 4 : 3;
}

void update_ctr(int block_i, int index, int ctr_bits, bool taken){
  int max = (1 << ctr_bits)-1;
	int current = TBLOCKS[block_i]->ctr[index];
	if(taken == TAKEN){
		TBLOCKS[block_i]->ctr[index] = ((current + 1) > max) ? max : (current + 1);
	} else {
		TBLOCKS[block_i]->ctr[index] = ((current - 1) < 0) ? 0 : (current - 1);
	}
}
bool get_prediction(int ctr, int ctr_bits){
  int mid = (1 << ctr_bits)/2;
	if(ctr < mid){
		return NOT_TAKEN;
	} else {
		return TAKEN;
	}
}

void update_GHR(bool taken){
  // simply shift in new outcome into GHR
	int GHR_size = GHR_BITS/32;
	UINT32 msb, old_msb;
	if(taken == TAKEN){
		old_msb = 0x00000001;
	} else {
		old_msb = 0x00000000;
	}
	for(int i = 0; i < GHR_size; i++){
		msb = (GHR[i] & 0x80000000) >> 31;
		GHR[i] = GHR[i] << 1;
		GHR[i] = GHR[i] | old_msb;
		old_msb = msb;
	}
	return;
}

UINT32 hash_fcn(UINT32 PC, int length, int fold_width, UINT32 fold_mask){
  // take fold_width folds of GHR and PC and hash them using XOR
  UINT32 hash = 0;
  UINT32 temp;
  int i = 0;
  while (length > 0) {
    if (length > 32) {
      temp = GHR[i];
    } else {
      temp = 0;
      for (int j = 0; j < length; j++){
        temp |= GHR[i]&(1<<j);
      }
    }
    while (temp != 0) {
      hash ^= (temp&fold_mask);
      temp = temp >> fold_width;
    }
    length -= 32;
    i++;
  }
  temp = PC;
  while (temp != 0) {
    hash ^= (temp&fold_mask);
    temp = temp >> fold_width;
  }
	return hash;
}
