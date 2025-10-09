#include "predictor.h"

/////////////////////////////////////////////////////////////
// 2bitsat
/////////////////////////////////////////////////////////////

#define TWO_BIT_SAT_TABLE_BITS 8192
#define TEN_LSB_MASK 0xFFF

int *state_table_2bitsat;
void InitPredictor_2bitsat() {
  /* 
    0 - strong NT
    1 - weak NT
    2 - weak T
    3 - strong T
  */
  /*
    represent each counter as a byte (so 8192/2 = 4096 counters)
    easier to implement indexing in this way...
  */
  state_table_2bitsat = (int*)malloc(TWO_BIT_SAT_TABLE_BITS/2*sizeof(int)); 
  int i = 0;
  /*
    init all counters to weak not taken
  */
  while(i < TWO_BIT_SAT_TABLE_BITS/2){
    *(state_table_2bitsat + i) = 1;
    i++;
  }
}

bool GetPrediction_2bitsat(UINT32 PC) {
  /*
    get the 10 LSB of PC to index state table
    2^10 = 1024 possible indices
  */
  int index = (int)(PC & (UINT32)TEN_LSB_MASK);
  int state = *(state_table_2bitsat + index);
  return state <= 1 ? NOT_TAKEN : TAKEN;
}

void UpdatePredictor_2bitsat(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  /*
    get the state of counter
  */
  int index = (int)(PC & (UINT32)TEN_LSB_MASK);
  int state = *(state_table_2bitsat + index);
  /*
    update the counter
  */
  if(resolveDir == TAKEN) state = ((state + 1) > 3 ? 3 : (state + 1));
  else state = ((state - 1) < 0 ? 0 : (state - 1));
  *(state_table_2bitsat + index) = state;
}

/////////////////////////////////////////////////////////////
// 2level
/////////////////////////////////////////////////////////////
#define HISTORY_LEVEL_ENTRIES 512;
#define HISTORY_BITS 6;
#define THREE_LSB_MASK 0x7;
#define TWELVE_LSB_MASK 0xFFF
int *history_table_forlevel;
int *state_table_2levelsat;
void InitPredictor_2level() {
  /* 
    0 - strong NT
    1 - weak NT
    2 - weak T
    3 - strong T
  */
  /*
    represent each counter as a byte (so 8192/8 = 1024 counters)
    easier to implement indexing in this way...
  */
  history_table_forlevel = (int*)malloc(TWO_BIT_SAT_TABLE_BITS/8*sizeof(int)); 
  state_table_2levelsat = (int*)malloc(HISTORY_LEVEL_ENTRIES/8*sizeof(int)); 
  int i = 0;
  /*
    init all counters to weak not taken
  */
  while(i < TWO_BIT_SAT_TABLE_BITS/8){
    *(state_table_2bitsat + i) = 1;
    i++;
  }

}

bool GetPrediction_2level(UINT32 PC) {

  return TAKEN;
}

void UpdatePredictor_2level(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {

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

