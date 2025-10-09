#include "predictor.h"

/////////////////////////////////////////////////////////////
// 2bitsat
/////////////////////////////////////////////////////////////

#define TWO_BIT_SAT_TABLE_BITS 8192
#define TWELVE_LSB_MASK 0xFFF
int *state_table_2bitsat;
void InitPredictor_2bitsat() {
  /* 
    0 - strong NT
    1 - weak NT
    2 - weak T
    3 - strong T
  */
  /*
    8192/2 = 4096 counters
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
    get the 12 LSB of PC to index state table
    2^12 = 4096 possible indices
  */
  int index = (int)(PC & (UINT32)TWELVE_LSB_MASK);
  int state = *(state_table_2bitsat + index);
  return state <= 1 ? NOT_TAKEN : TAKEN;
}

void UpdatePredictor_2bitsat(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  /*
    get the state of counter
  */
  int index = (int)(PC & (UINT32)TWELVE_LSB_MASK);
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

void InitPredictor_2level() {

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

