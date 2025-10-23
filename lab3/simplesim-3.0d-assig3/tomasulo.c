
#include <limits.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "syscall.h"
#include "dlite.h"
#include "options.h"
#include "stats.h"
#include "sim.h"
#include "decode.def"

#include "instr.h"

/* PARAMETERS OF THE TOMASULO'S ALGORITHM */

#define INSTR_QUEUE_SIZE         16

#define RESERV_INT_SIZE    5
#define RESERV_FP_SIZE     3
#define FU_INT_SIZE        3
#define FU_FP_SIZE         1

#define FU_INT_LATENCY     5
#define FU_FP_LATENCY      7

/* IDENTIFYING INSTRUCTIONS */

//unconditional branch, jump or call
#define IS_UNCOND_CTRL(op) (MD_OP_FLAGS(op) & F_CALL || \
                         MD_OP_FLAGS(op) & F_UNCOND)

//conditional branch instruction
#define IS_COND_CTRL(op) (MD_OP_FLAGS(op) & F_COND)

//floating-point computation
#define IS_FCOMP(op) (MD_OP_FLAGS(op) & F_FCOMP)

//integer computation
#define IS_ICOMP(op) (MD_OP_FLAGS(op) & F_ICOMP)

//load instruction
#define IS_LOAD(op)  (MD_OP_FLAGS(op) & F_LOAD)

//store instruction
#define IS_STORE(op) (MD_OP_FLAGS(op) & F_STORE)

//trap instruction
#define IS_TRAP(op) (MD_OP_FLAGS(op) & F_TRAP) 

#define USES_INT_FU(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_STORE(op))
#define USES_FP_FU(op) (IS_FCOMP(op))

#define WRITES_CDB(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_FCOMP(op))

/* FOR DEBUGGING */

//prints info about an instruction
#define PRINT_INST(out,instr,str,cycle)	\
  myfprintf(out, "%d: %s", cycle, str);		\
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);

#define PRINT_REG(out,reg,str,instr) \
  myfprintf(out, "reg#%d %s ", reg, str);	\
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);


/* ECE552 Assignment 3 - BEGIN CODE */
/* DEFINES */
#define NUM_INPUT_REGS    3
#define NUM_OUTPUT_REGS   2

/* ECE552 Assignment 3 - END CODE */

/* ECE552 Assignment 3 - BEGIN CODE */
/* VARIABLES */

//instruction queue for tomasulo
static instruction_t* IFQ[INSTR_QUEUE_SIZE];
// if head == tail: IFQ empty
// if tail 1 before head: IFQ full
// implementing a circular buffer
int IFQ_head = 0;
int IFQ_tail = 0;
//number of instructions in the instruction queue
static int IFQ_instr_count = 0;

//the index of the last instruction fetched
static int fetch_index = 0;

/* MAP TABLE */
int map_table[MD_TOTAL_REGS];

/* FUNCTIONAL UNITS */
typedef struct FUNCTIONAL_UNIT {
  instruction_t*    instr;
  int               cycles_to_completion;
  int               rs_num; // reservation station entry number
} func_unit_t;

func_unit_t fuINT[FU_INT_SIZE];
func_unit_t fuFP[FU_FP_SIZE];

/* RESERVATION STATIONS */
typedef struct RESERVATION_STATION {
  bool              busy;
  bool              executing;
  instruction_t*    instr;
  int               R[NUM_OUTPUT_REGS];
  int               T[NUM_INPUT_REGS];
  int               inst_cycle;
} res_stat_t;

res_stat_t reservINT[RESERV_INT_SIZE];
res_stat_t reservFP[RESERV_FP_SIZE];

/* COMMON DATA BUS */
typedef struct COMMON_DATA_BUS {
  instruction_t*    instr;
  int               T; // tag
} cdb_t;

cdb_t CDB;

/* ECE552 Assignment 3 - END CODE */

/* ECE552 Assignment 3 - BEGIN CODE */
// helper function prototypes
bool h_IFQ_full();
bool h_IFQ_empty();
void h_IFQ_push(instruction_t*);
instruction_t* h_IFQ_head();
instruction_t* h_IFQ_pop();

void h_alloc_rs_entry(res_stat_t*, int, instruction_t*, int);
/* ECE552 Assignment 3 - END CODE */

/* ECE552 Assignment 3 - BEGIN CODE */
/* 
 * Description: 
 * 	Checks if simulation is done by finishing the very last instruction
 *      Remember that simulation is done only if the entire pipeline is empty
 * Inputs:
 * 	sim_insn: the total number of instructions simulated
 * Returns:
 * 	True: if simulation is finished
 */
static bool is_simulation_done(counter_t sim_insn) {
  if (fetch_index < sim_insn) return false; // have not fetched the last instr yet
  if (!h_IFQ_empty()) return false; // have not handled all instrs in IFQ yet

  // if RS not empty or FU not empty (i.e. instr is executing), need to continue
  for (int i = 0; i < RESERV_INT_SIZE; i++) {
    if (reservINT[i].busy || reservINT[i].executing) return false;
  }
  for (int i = 0; i < RESERV_FP_SIZE; i++) {
    if (reservFP[i].busy || reservFP[i].executing) return false;
  }

  // if CDB not empty, need to continue
  if (CDB.T != -1) return false;

  return true;
}
/* ECE552 Assignment 3 - END CODE */

/* ECE552 Assignment 3 - BEGIN CODE */
/* 
 * Description: 
 * 	Retires the instruction from writing to the Common Data Bus
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void CDB_To_retire(int current_cycle) {
  if (CDB.instr != NULL) {
    CDB.instr->tom_cdb_cycle++; // count num cycles instr has been in CDB
  }

  // broadcast to reservation stations
  for (int i = 0; i < RESERV_INT_SIZE; i++) {
    for (int j = 0; j < NUM_INPUT_REGS; j++) {
      if (reservINT[i].T[j] == CDB.T) reservINT[i].T[j] = -1;
    }
  }
  for (int i = 0; i < RESERV_FP_SIZE; i++) {
    for (int j = 0; j < NUM_INPUT_REGS; j++) {
      if (reservFP[i].T[j] == CDB.T) reservFP[i].T[j] = -1;
    }
  }

  // broadcast to map table
  for (int i = 0; i < MD_TOTAL_REGS; i++) {
    if (map_table[i] == CDB.T) map_table[i] = -1;
  }

  // clear CDB
  CDB.T     = -1;
  CDB.instr = NULL;
}
/* ECE552 Assignment 3 - END CODE */


/* 
 * Description: 
 * 	Moves an instruction from the execution stage to common data bus (if possible)
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void execute_To_CDB(int current_cycle) {

  /* ECE552: YOUR CODE GOES HERE */

}

/* 
 * Description: 
 * 	Moves instruction(s) from the issue to the execute stage (if possible). We prioritize old instructions
 *      (in program order) over new ones, if they both contend for the same functional unit.
 *      All RAW dependences need to have been resolved with stalls before an instruction enters execute.
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void issue_To_execute(int current_cycle) {

  /* ECE552: YOUR CODE GOES HERE */
}

/* ECE552 Assignment 3 - BEGIN CODE */
/* 
 * Description: 
 * 	Moves instruction(s) from the dispatch stage to the issue stage
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void dispatch_To_issue(int current_cycle) {
  // if IFQ empty don't do anything
  if (h_IFQ_empty()) {
    printf("IFQ empty, no instrs to dispatch\n");
    return;
  } else {
    printf("Dispatching...\n");
  }

  instruction_t* dispatched_instr = h_IFQ_head();
  assert (dispatched_instr != NULL);
  dispatched_instr->tom_dispatch_cycle++;

  // conditional and unconditional branches do not get dispatched to RS
  // so simply pop from IFQ and skip
  if (IS_COND_CTRL(dispatched_instr->op) || IS_UNCOND_CTRL(dispatched_instr->op)) {
    printf("dispatched instr was branch, popping from IFQ and skipping...\n");
    h_IFQ_pop();
    return;
  }

  // move into appropriate RS
  bool found_available_rs_entry = false;
  if (USES_INT_FU(dispatched_instr->op)) {
    printf("dispatched instr uses INT FU, searching for available RS entry...\n");
    // look for available RS
    for (int i = 0; i < RESERV_INT_SIZE; i++) {
      if (!reservINT[i].busy) { // this entry is not allocated!
        // allocate this entry, then stop
        printf("found available RS entry at index %d! allocating and popping from IFQ...\n", i);
        h_alloc_rs_entry(&reservINT[i], i, dispatched_instr, current_cycle);
        h_IFQ_pop();
        printf("allocated and popped! IFQ_instr_count: %d; head: %d; tail: %d\n", IFQ_instr_count, IFQ_head, IFQ_tail);
        found_available_rs_entry = true;
        break;
      }
    }
  } else if (USES_FP_FU(dispatched_instr->op)) {
    printf("dispatched instr uses FP FU, searching for available RS entry...\n");
    for (int i = 0; i < RESERV_FP_SIZE; i++) {
      if (!reservFP[i].busy) {
        printf("found available RS entry at index %d! allocating and popping from IFQ...\n", i);
        h_alloc_rs_entry(&reservFP[i], i, dispatched_instr, current_cycle);
        h_IFQ_pop();
        printf("allocated and popped! IFQ_instr_count: %d; head: %d; tail: %d\n", IFQ_instr_count, IFQ_head, IFQ_tail);
        found_available_rs_entry = true;
        break;
      }
    }
  } else {
    printf("ERROR: dispatched instr uses does not INT nor FP FU\n");
  }
  if (!found_available_rs_entry) printf("no available RS entry.\n");
}
/* ECE552 Assignment 3 - END CODE */

/* ECE552 Assignment 3 - BEGIN CODE */
/* 
 * Description: 
 * 	Grabs an instruction from the instruction trace (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	None
 */
void fetch(instruction_trace_t* trace) {
  instruction_t* fetched_instr;
  do {
    fetched_instr = get_instr(trace, ++fetch_index); // get_instr stated in instr.h
                                                     // pre-increment fetch_index bc it 
                                                     // is index of last fetched isntr
  } while (IS_TRAP(fetched_instr->op)); // skip trap instructions

  if (fetch_index <= sim_num_insn) { // only fetch if within limit of instrs
    h_IFQ_push(fetched_instr);
  } else {
    printf("fetch_index: %d, exceeds sim_num_insn: %d\n", fetch_index, (int)sim_num_insn);
  }
}
/* ECE552 Assignment 3 - END CODE */

/* ECE552 Assignment 3 - BEGIN CODE */
/* 
 * Description: 
 * 	Calls fetch and dispatches an instruction at the same cycle (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void fetch_To_dispatch(instruction_trace_t* trace, int current_cycle) {
  if (!h_IFQ_full()) {
    printf("IFQ not full; IFQ_instr_count: %d; fetching...\n", IFQ_instr_count); // to remove
    fetch(trace);
    printf("fetched! IFQ_instr_count: %d; head: %d; tail: %d\n", IFQ_instr_count, IFQ_head, IFQ_tail); // to remove
  } else {
    printf("IFQ full\n");
  }
}
/* ECE552 Assignment 3 - END CODE */

/* ECE552 Assignment 3 - BEGIN CODE */
/* 
 * Description: 
 * 	Performs a cycle-by-cycle simulation of the 4-stage pipeline
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	The total number of cycles it takes to execute the instructions.
 * Extra Notes:
 * 	sim_num_insn: the number of instructions in the trace
 */
counter_t runTomasulo(instruction_trace_t* trace)
{
  //initialize instruction queue
  int i;
  for (i = 0; i < INSTR_QUEUE_SIZE; i++) {
    IFQ[i] = NULL;
  }

  //initialize reservation stations
  for (i = 0; i < RESERV_INT_SIZE; i++) {
    reservINT[i].busy         = false;
    reservINT[i].executing    = false;
    reservINT[i].instr        = NULL;
    for (int j = 0; j < NUM_OUTPUT_REGS; j++) {
      reservINT[i].R[j]           = -1;
    }
    for (int j = 0; j < NUM_INPUT_REGS; j++) {
      reservINT[i].T[j]           = -1;
    }
    reservINT[i].inst_cycle   = 0;
  }

  for(i = 0; i < RESERV_FP_SIZE; i++) {
    reservFP[i].busy          = false;
    reservFP[i].executing     = false;
    reservFP[i].instr         = NULL;
    for (int j = 0; j < NUM_OUTPUT_REGS; j++) {
      reservFP[i].R[j]           = -1;
    }
    for (int j = 0; j < NUM_INPUT_REGS; j++) {
      reservFP[i].T[j]           = -1;
    }
    reservFP[i].inst_cycle    = 0;
  }

  //initialize functional units
  for (i = 0; i < FU_INT_SIZE; i++) {
    fuINT[i].instr                  = NULL;
    fuINT[i].cycles_to_completion   = -1;
    fuINT[i].rs_num                 = -1;
  }

  for (i = 0; i < FU_FP_SIZE; i++) {
    fuFP[i].instr                   = NULL;
    fuFP[i].cycles_to_completion    = -1;
    fuFP[i].rs_num                  = -1;
  }

  //initialize map_table to no producers
  int reg;
  for (reg = 0; reg < MD_TOTAL_REGS; reg++) {
    map_table[reg] = -1;
  }

  // initialize the CDB
  CDB.T     = -1;
  CDB.instr = NULL;
  
  printf("structures initialized!\n"); // to remove

  int cycle = 1;
  while (true) {
    printf("cycle %d:\n", cycle); // to remove

    CDB_To_retire(cycle);
    execute_To_CDB(cycle);
    issue_To_execute(cycle);
    dispatch_To_issue(cycle);
    fetch_To_dispatch(trace, cycle);

    cycle++;

    if (is_simulation_done(sim_num_insn)) break;
    if (cycle > 30) break; // to remove
  }
  
  return cycle;
}
/* ECE552 Assignment 3 - END CODE */

/* ECE552 Assignment 3 - BEGIN CODE */
// this section contains helper functions
bool h_IFQ_full() {
  assert (IFQ_instr_count <= INSTR_QUEUE_SIZE);
  assert (0 <= IFQ_instr_count);
  return IFQ_instr_count >= INSTR_QUEUE_SIZE;
}
bool h_IFQ_empty() {
  assert (IFQ_instr_count <= INSTR_QUEUE_SIZE);
  assert (0 <= IFQ_instr_count);
  return IFQ_instr_count == 0;
}

void h_IFQ_push(instruction_t* instr) {
  assert (!h_IFQ_full());

  IFQ[IFQ_tail] = instr;
  if (IFQ_tail == INSTR_QUEUE_SIZE-1) {
    IFQ_tail = 0;
  } else {
    IFQ_tail++;
  }
  IFQ_instr_count++;
}
instruction_t* h_IFQ_head() {
  return IFQ[IFQ_head];
}
instruction_t* h_IFQ_pop() {
  assert(!h_IFQ_empty());

  instruction_t* popped_instr;
  popped_instr = h_IFQ_head();

  IFQ[IFQ_head] = NULL;
  if (IFQ_head == INSTR_QUEUE_SIZE-1) {
    IFQ_head = 0;
  } else {
    IFQ_head++;
  }
  IFQ_instr_count--;

  return popped_instr;
}

void h_alloc_rs_entry(res_stat_t* res_stat_entry, int res_stat_index, instruction_t* instr, int inst_cycle) {
  res_stat_entry->busy = true;
  res_stat_entry->executing = false;
  res_stat_entry->instr = instr;

  // T0, T1, T2: input registers
  for (int i = 0; i < 3; i++) {
    if (instr->r_in[i] != -1) {
      res_stat_entry->T[i] = map_table[instr->r_in[i]];
    } else {
      res_stat_entry->T[i] = -1;
    }
  }

  // R0, R1: output registers (stores don't have output registers)
  // also update map table
  if (!IS_STORE(instr->op)) {
    for (int i = 0; i < 2; i++) {
      if (instr->r_out[i] != -1) {
        res_stat_entry->R[i] = instr->r_out[i];
        map_table[instr->r_out[i]] = res_stat_index;
      }
    }
  }

  res_stat_entry->inst_cycle = inst_cycle;
}
/* ECE552 Assignment 3 - END CODE */