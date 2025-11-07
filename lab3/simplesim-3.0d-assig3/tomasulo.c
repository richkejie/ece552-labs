
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

#define DEBUG_PRINTF false
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

func_unit_t func_units[FU_INT_SIZE + FU_FP_SIZE];

/* RESERVATION STATIONS */
typedef struct RESERVATION_STATION {
  bool              busy;
  bool              executing;
  int               FU_unit;
  instruction_t*    instr;
  int               R[NUM_OUTPUT_REGS];
  int               T[NUM_INPUT_REGS];
  int               inst_cycle; // cycle this entry is instantiated (allocated)
  int               tag;
} res_stat_t;

res_stat_t reserv_stats[RESERV_INT_SIZE + RESERV_FP_SIZE];
static int uniqueTag;

/* COMMON DATA BUS */
typedef struct COMMON_DATA_BUS {
  instruction_t*    instr;
  int               T; // tag
} cdb_t;

cdb_t CDB;

/* INSTRUCTION LIST */
typedef struct INSTR_NODE { // implement as linked list since can be arbitrary size
  int                       RS_entry; // index of reserv_stats array
  struct INSTR_NODE         *prev;
  struct INSTR_NODE         *next;
} instr_node_t;

instr_node_t *instr_list_head = NULL;
int instr_list_size = 0;
/* ECE552 Assignment 3 - END CODE */

/* ECE552 Assignment 3 - BEGIN CODE */
// helper function prototypes
bool                h_IFQ_full();
bool                h_IFQ_empty();
void                h_IFQ_push(instruction_t*);
instruction_t*      h_IFQ_head();
instruction_t*      h_IFQ_pop();

void                h_alloc_rs_entry(res_stat_t*, int, instruction_t*, int);
void                h_dealloc_rs_entry(res_stat_t*);
bool                h_rs_entry_ready(int);

void                h_alloc_fu_unit(int, int, int);
void                h_dealloc_fu_unit(int);
void                h_advance_fu_unit(int, int);

void                h_instr_list_push(int);
void                h_instr_list_remove(instr_node_t*);

bool                h_CDB_free();
void                h_write_CDB(instruction_t*, int);
/* ECE552 Assignment 3 - END CODE */

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
  /* ECE552 Assignment 3 - BEGIN CODE */
  if (fetch_index < sim_insn) return false; // have not fetched the last instr yet
  if (!h_IFQ_empty()) return false; // have not handled all instrs in IFQ yet

  // if RS not empty or FU not empty (i.e. instr is executing), need to continue
  for (int i = 0; i < RESERV_INT_SIZE+RESERV_FP_SIZE; i++) {
    if (reserv_stats[i].busy || reserv_stats[i].executing) return false;
  }

  // if CDB not empty, need to continue
  if (CDB.T != -1) return false;

  return true;
  /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Retires the instruction from writing to the Common Data Bus
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void CDB_To_retire(int current_cycle) {
  /* ECE552 Assignment 3 - BEGIN CODE */
  if ((CDB.instr == NULL) || (CDB.T == -1)) {
    return; // CDB is empty
  } else {
    CDB.instr->tom_cdb_cycle = current_cycle;
    // PRINT_INST(stdout, CDB.instr, "broadcasting from cdb: ", current_cycle);
  }

  // broadcast to reservation stations
  for (int i = 0; i < RESERV_INT_SIZE+RESERV_FP_SIZE  ; i++) {
    for (int j = 0; j < NUM_INPUT_REGS; j++) {
      if (reserv_stats[i].T[j] == CDB.T) reserv_stats[i].T[j] = -1;
    }
  }

  // broadcast to map table
  for (int i = 0; i < MD_TOTAL_REGS; i++) {
    if (map_table[i] == CDB.T) map_table[i] = -1;
  }

  // clear CDB
  CDB.T     = -1;
  CDB.instr = NULL;
  /* ECE552 Assignment 3 - END CODE */
}


/* 
 * Description: 
 * 	Moves an instruction from the execution stage to common data bus (if possible)
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void execute_To_CDB(int current_cycle) {
  /* ECE552 Assignment 3 - BEGIN CODE */
  // advance all instrs in FU units by 1 cycle
  for (int i = 0; i < FU_INT_SIZE + FU_FP_SIZE; i++) {
    if (func_units[i].rs_num != -1) {
      // if (DEBUG_PRINTF) printf("instr in RS entry %d is executing...\n", func_units[i].rs_num);
      // PRINT_INST(stdout, func_units[i].instr, "executing %d ", current_cycle);
      // printf("cycles left: %d\n", func_units[i].cycles_to_completion);
    }
    h_advance_fu_unit(i, 1);
  }

  instr_node_t* node, *temp;
  // check for execution completion
  // check oldest instr first, since they get prio to enter CDB
  int FU_index;
  instruction_t *instr;
  for (node = instr_list_head; node != NULL; node = node->next) {
    if (reserv_stats[node->RS_entry].executing) {
      
      FU_index = reserv_stats[node->RS_entry].FU_unit;
      instr = reserv_stats[node->RS_entry].instr;

      assert(func_units[FU_index].instr == reserv_stats[node->RS_entry].instr);
      assert(func_units[FU_index].rs_num == node->RS_entry);

      if (func_units[FU_index].cycles_to_completion == 0) { 
        // PRINT_INST(stdout, func_units[FU_index].instr, "finished executing: ", current_cycle); 
        // this instr has finished executing
        // check if it needs to write to CDB
        if (WRITES_CDB(func_units[FU_index].instr->op)) {
          // if CDB is free, write instr to CDB, otherwise wait
          if (h_CDB_free()) {
            if (DEBUG_PRINTF) printf("CDB available, writing to CDB, and deallocating FU and RS entry and removing from list...\n");
            // PRINT_INST(stdout, reserv_stats[node->RS_entry].instr, "writing to cdb: ", current_cycle);
            h_write_CDB(instr, reserv_stats[node->RS_entry].tag);
            h_dealloc_fu_unit(FU_index);
            h_dealloc_rs_entry(&reserv_stats[node->RS_entry]);
            // temp = node->next;
            h_instr_list_remove(node);
            // node = temp;
            // if (node == NULL) {
            //   printf("end of linked list\n");
            //   break;
            // };
          } else {
            if (DEBUG_PRINTF) printf("CDB busy, waiting...\n");
            continue;
          }
        } else {
          if (DEBUG_PRINTF) printf("completed instr does not write to CDB, deallocating FU and RS entry and removing from list...\n");
          h_dealloc_fu_unit(FU_index);
          h_dealloc_rs_entry(&reserv_stats[node->RS_entry]);
          // temp = node->next;
          h_instr_list_remove(node);
          // node = temp;
          // if (node == NULL) {
          //   printf("end of linked list\n");
          //   break;
          // };
        }
      }
    }
  }
  /* ECE552 Assignment 3 - END CODE */
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
  /* ECE552 Assignment 3 - BEGIN CODE */
  instr_node_t *node;

  for (node = instr_list_head; node != NULL; node = node->next) {
    assert(reserv_stats[node->RS_entry].instr != NULL);
    // if instruction is already in FU, continue
    if (reserv_stats[node->RS_entry].executing) {
      if (DEBUG_PRINTF) printf("instr in RS entry %d is already executing...\n", node->RS_entry);
      continue; 
    }

    // if instruction is ready, move to execute
    if (h_rs_entry_ready(node->RS_entry)) { // checking if operands are ready
      // find available FU unit
      bool found_available_fu_unit = false;
      int start_idx = 0;
      int end_idx = 0;
      int cycles_to_completion = 0;
      if (USES_INT_FU(reserv_stats[node->RS_entry].instr->op)) {
        start_idx = 0;
        end_idx = FU_INT_SIZE;
        cycles_to_completion = FU_INT_LATENCY;
      } else if (USES_FP_FU(reserv_stats[node->RS_entry].instr->op)) {
        start_idx = FU_INT_SIZE;
        end_idx = FU_INT_SIZE + FU_FP_SIZE;
        cycles_to_completion = FU_FP_LATENCY;
      } else {
        if (DEBUG_PRINTF) printf("ERROR: dispatched instr uses does not INT nor FP FU\n");
      }
      for (int i = start_idx; i < end_idx; i++) {
        if (func_units[i].instr == NULL) {
          h_alloc_fu_unit(i, node->RS_entry, cycles_to_completion);
          found_available_fu_unit = true;
          if (DEBUG_PRINTF) printf("found available fu unit!\n");

          // instr enters execute the cycle after it ends issue --- so the first cycle it will be using a FU
          reserv_stats[node->RS_entry].instr->tom_execute_cycle = current_cycle + 1;
          break;
        }
      }
      if (!found_available_fu_unit) if (DEBUG_PRINTF) printf("no available fu unit!\n");
    } else {
      if (DEBUG_PRINTF) printf("instr in RS entry %d is not ready\n", node->RS_entry);
    }
  }
  /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Moves instruction(s) from the dispatch stage to the issue stage
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void dispatch_To_issue(int current_cycle) {
  /* ECE552 Assignment 3 - BEGIN CODE */
  // if IFQ empty don't do anything
  if (h_IFQ_empty()) {
    if (DEBUG_PRINTF) printf("IFQ empty, no instrs to dispatch\n");
    return;
  } else {
    if (DEBUG_PRINTF) printf("there are instrs in IFQ, dispatching...\n");
  }

  instruction_t* dispatched_instr = h_IFQ_head(); 
  assert (dispatched_instr != NULL);

  // conditional and unconditional branches do not get dispatched to RS
  // so simply pop from IFQ and skip
  if (IS_COND_CTRL(dispatched_instr->op) || IS_UNCOND_CTRL(dispatched_instr->op)) {
    if (DEBUG_PRINTF) printf("dispatched instr was branch, popping from IFQ and skipping...\n");
    h_IFQ_pop();
    return;
  }

  // move into appropriate RS
  bool found_available_rs_entry = false;
  int start_idx = 0;
  int end_idx = 0;
  if (USES_INT_FU(dispatched_instr->op)) {
    start_idx = 0;
    end_idx = RESERV_INT_SIZE;
  } else if (USES_FP_FU(dispatched_instr->op)) {
    start_idx = RESERV_INT_SIZE;
    end_idx = RESERV_INT_SIZE + RESERV_FP_SIZE;
  } else {
    if (DEBUG_PRINTF) printf("ERROR: dispatched instr uses does not INT nor FP FU\n");
  }

  if (start_idx != end_idx) if (DEBUG_PRINTF) printf("searching for available RS entry...\n");
  for (int i = start_idx; i < end_idx; i++) {
    if (!reserv_stats[i].busy) {

      if (DEBUG_PRINTF) printf("found available RS entry at index %d! allocating and popping from IFQ...\n", i);
      h_alloc_rs_entry(&reserv_stats[i], i, dispatched_instr, current_cycle); // will also set map table
      h_instr_list_push(i); // push to issue queue to record age of instructions
      h_IFQ_pop();
      if (DEBUG_PRINTF) printf("allocated and popped! IFQ_instr_count: %d; head: %d; tail: %d\n", IFQ_instr_count, IFQ_head, IFQ_tail);
      found_available_rs_entry = true;

      // instr enters issue the cycle after it ends dispatch --- so the first cycle it will be using a RS entry
      dispatched_instr->tom_issue_cycle = current_cycle+1;
      break;
    }
  }
  if (!found_available_rs_entry) if (DEBUG_PRINTF) printf("no available RS entry!\n");
  /* ECE552 Assignment 3 - END CODE */
}


/* 
 * Description: 
 * 	Grabs an instruction from the instruction trace (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	None
 */
void fetch(instruction_trace_t* trace) {
  /* ECE552 Assignment 3 - BEGIN CODE */
  instruction_t* fetched_instr;
  do {
    fetched_instr = get_instr(trace, ++fetch_index); // get_instr stated in instr.h
    fetched_instr->tom_issue_cycle = 0;
    fetched_instr->tom_execute_cycle = 0;
    fetched_instr->tom_dispatch_cycle = 0;
    fetched_instr->tom_cdb_cycle = 0;                                                   
                                                    // pre-increment fetch_index bc it 
                                                     // is index of last fetched isntr
  } while (IS_TRAP(fetched_instr->op)); // skip trap instructions

  if (fetch_index <= sim_num_insn) { // only fetch if within limit of instrs
    h_IFQ_push(fetched_instr);
    
  } else {
    if (DEBUG_PRINTF) printf("fetch_index: %d, exceeds sim_num_insn: %d\n", fetch_index, (int)sim_num_insn);
  }
  /* ECE552 Assignment 3 - END CODE */
}

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
  /* ECE552 Assignment 3 - BEGIN CODE */
  if (!h_IFQ_full()) {
    if (DEBUG_PRINTF) printf("IFQ not full; IFQ_instr_count: %d; fetching...\n", IFQ_instr_count);
    fetch(trace);
    // if (DEBUG_PRINTF) printf("fetched! IFQ_instr_count: %d; head: %d; tail: %d\n", IFQ_instr_count, IFQ_head, IFQ_tail); 
  } else {
    if (DEBUG_PRINTF) printf("IFQ full\n");
  }

  // instr enters dispatch when it makes it into the IFQ
  instruction_t *instr;
  for (int i = 0; i < INSTR_QUEUE_SIZE; i++) {
    instr = IFQ[i];
    if (instr == NULL) continue;
    if (instr->tom_dispatch_cycle == 0) {
      instr->tom_dispatch_cycle = current_cycle;
    }
  }

  dispatch_To_issue(current_cycle); 
  /* ECE552 Assignment 3 - END CODE */
}

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
  /* ECE552 Assignment 3 - BEGIN CODE */
  //initialize instruction queue
  int i;
  for (i = 0; i < INSTR_QUEUE_SIZE; i++) {
    IFQ[i] = NULL;
  }

  //initialize reservation stations
  for (i = 0; i < RESERV_INT_SIZE + RESERV_FP_SIZE; i++) {
    reserv_stats[i].busy          = false;
    reserv_stats[i].executing     = false;
    reserv_stats[i].FU_unit       = -1;
    reserv_stats[i].instr         = NULL;
    for (int j = 0; j < NUM_OUTPUT_REGS; j++) {reserv_stats[i].R[j] = -1;}
    for (int j = 0; j < NUM_INPUT_REGS; j++)  {reserv_stats[i].T[j] = -1;}
    reserv_stats[i].inst_cycle    = 0;
    reserv_stats[i].tag           = -1;
  }
  uniqueTag = 0;

  //initialize functional units
  for (i = 0; i < FU_INT_SIZE + FU_FP_SIZE; i++) {
    func_units[i].instr                  = NULL;
    func_units[i].cycles_to_completion   = -1;
    func_units[i].rs_num                 = -1;
  }

  //initialize map_table
  int reg;
  for (reg = 0; reg < MD_TOTAL_REGS; reg++) {
    map_table[reg] = -1;
  }

  // initialize the CDB
  CDB.T     = -1;
  CDB.instr = NULL;
  
  if (DEBUG_PRINTF) printf("structures initialized!\n\n\n");

  int cycle = 1;
  while (true) {
    if (DEBUG_PRINTF) printf("\ncycle %d:\n", cycle);
    CDB_To_retire(cycle);
    execute_To_CDB(cycle);
    issue_To_execute(cycle);
    fetch_To_dispatch(trace, cycle); // will call dispatch_To_issue
    if (is_simulation_done(sim_num_insn)) break;
    cycle++;
  }
  
  return cycle;
  /* ECE552 Assignment 3 - END CODE */
}


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
  return IFQ_instr_count <= 0;
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
  res_stat_entry->busy        = true;
  res_stat_entry->executing   = false;
  res_stat_entry->FU_unit     = -1;
  res_stat_entry->instr       = instr;

  // T0, T1, T2: input registers
  for (int i = 0; i < NUM_INPUT_REGS; i++) {
    if (instr->r_in[i] != -1) {
      res_stat_entry->T[i] = map_table[instr->r_in[i]];
    } else {
      res_stat_entry->T[i] = -1;
    }
  }

  // R0, R1: output registers (stores don't have output registers)
  // also update map table
  if (!IS_STORE(instr->op)) {
    res_stat_entry->tag = uniqueTag;
    for (int i = 0; i < NUM_OUTPUT_REGS; i++) { 
      if (instr->r_out[i] != -1) {
        res_stat_entry->R[i] = instr->r_out[i];
        // map_table[instr->r_out[i]] = res_stat_index;
        map_table[instr->r_out[i]] = uniqueTag;
      }
    }
    uniqueTag++;
  }

  res_stat_entry->inst_cycle = inst_cycle;
}
void h_dealloc_rs_entry(res_stat_t* res_stat_entry) {
  res_stat_entry->busy        = false;
  res_stat_entry->executing   = false;
  res_stat_entry->FU_unit     = -1;
  res_stat_entry->instr       = NULL;
  for (int i = 0; i < NUM_OUTPUT_REGS; i++) {res_stat_entry->R[i] = -1;}
  for (int i = 0; i < NUM_INPUT_REGS; i++)  {res_stat_entry->T[i] = -1;}
  res_stat_entry->inst_cycle  = 0;    
  res_stat_entry->tag         = -1;
}
bool h_rs_entry_ready(int RS_entry) {
  for (int i = 0; i < NUM_INPUT_REGS; i++) {
  if (reserv_stats[RS_entry].T[i] != -1) {
      return false;
    }
  }
  return true;
}

void h_alloc_fu_unit(int FU_index, int RS_entry, int cycles_to_completion) {
  func_units[FU_index].instr                  = reserv_stats[RS_entry].instr;
  func_units[FU_index].cycles_to_completion   = cycles_to_completion;
  func_units[FU_index].rs_num                 = RS_entry;

  reserv_stats[RS_entry].executing            = true;
  reserv_stats[RS_entry].FU_unit              = FU_index;
}
void h_dealloc_fu_unit(int FU_index) {
  func_units[FU_index].instr                  = NULL;
  func_units[FU_index].cycles_to_completion   = -1;
  func_units[FU_index].rs_num                 = -1;
}
void h_advance_fu_unit(int FU_index, int num_cycles) {
  int cur_cycles = func_units[FU_index].cycles_to_completion;
  int new_cycles = cur_cycles - num_cycles;
  func_units[FU_index].cycles_to_completion = new_cycles < 0 ? 0 : new_cycles;
}

void h_instr_list_push(int RS_entry) {
  instr_node_t *node = (instr_node_t*)malloc(sizeof(instr_node_t));
  node->RS_entry = RS_entry;
  node->prev = NULL;
  node->next = NULL;

  if (instr_list_head == NULL) {
    instr_list_head = node;
  } else {
    instr_node_t *ptr;
    for (ptr = instr_list_head; ptr->next != NULL; ptr = ptr->next){}
    ptr->next = node;
    node->prev = ptr;
  }
  instr_list_size++;
}
void h_instr_list_remove(instr_node_t* node) {
  assert (node != NULL);
  if (node == instr_list_head) {
    instr_list_head = node->next;
    if (node->next != NULL) node->next->prev = instr_list_head;
  } else {
    node->prev->next = node->next;
    if (node->next != NULL) node->next->prev = node->prev;
  }
  free(node);
  instr_list_size--;
}

bool h_CDB_free() {
  return (CDB.T == -1);
}
void h_write_CDB(instruction_t *instr, int T) {
  CDB.instr = instr;
  CDB.T = T;
}
/* ECE552 Assignment 3 - END CODE */
