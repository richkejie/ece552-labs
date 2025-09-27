/*
	Richard Wu | wuricha8 | 1008078296
	microbenchmark for ECE552 Lab 1

	compile:
		/cad2/ece552f/compiler/bin/ssbig-na-sstrix-gcc mbq1.c -O0 -o mbq1
	assembly:
		/cad2/ece552f/compiler/bin/ssbig-na-sstrix-gcc mbq1.c -O0 -S
	objdump:
		/cad2/ece552f/compiler/bin/ssbig-na-sstrix-objdump -x -d -l mbq1 > mbq1.objdump

	**Analysis at bottom of file**
*/

#include <stdio.h>

int main() {

	/* initialization */
	int N = 10000000; /* 10,000,000 loop iterations */
	/* register keyword tells compiler to use registers */
	register int i = 0; /* iterator */

	register int A = 0, B = 0, C = 0;
	register int *D;

	/* X used for instrs with no dependencies to skip over several instrs */
	register int X = 0;

	/* c1, c2 used to test load and store instrs */
	int c1 = 0, c2 = 0;

	/* main loop */
	for (i = 0; i < N; i++) {

		/*
			loop condition assembly (from objdump):
			get next element for N
				400270:	28 00 00 00 	lw $2,16($30)
				400274:	10 00 02 1e 
			compare iterator to N
			q1) 2 cycle stall (internal forwarding)
			q2) 2 cycle stall (WX bypass)
				400278:	5b 00 00 00 	slt $9,$3,$2
				40027c:	00 09 02 03 
			if loop condition met (i < N), continue to loop contents
			q1) 2 cycle stall (internal forwarding)
			q2) 1 cycle stall (MX bypass)
				400280:	06 00 00 00 	bne $9,$0,400290 <main+0xa0>
				400284:	02 00 00 09 
			otherwise, jump to after loop, only executed on last instruction
				400288:	01 00 00 00 	j 400340 <main+0x150>
				40028c:	d0 00 10 00 
		*/

		A = *D;
		B = A + 1;
		A++;
		X = 0; X = 0; X = 0;
		/*
			objdump:
			A = *D;
				400290:	28 00 00 00 	lw $4,0($7)
				400294:	00 00 04 07 
			B = A + 1;
			q1) 2 cycle stall (internal forwarding)
			q2) 2 cycle stall (WX bypass)
				400298:	43 00 00 00 	addiu $5,$4,1
				40029c:	01 00 05 04 
			A++;
			no stall needed, since no dependency
				4002a0:	43 00 00 00 	addiu $4,$4,1
				4002a4:	01 00 04 04 
			3 (X = 0) instrs
				addu $8,$0,$0
				addu $8,$0,$0
				addu $8,$0,$0
		*/

		A = *D;
		B++;
		A++;
		X = 0; X = 0; X = 0;
		/*
			A = *D;
				4002c0:	28 00 00 00 	lw $4,0($7)
				4002c4:	00 00 04 07 
			B++;
			no stall since no dependency
				4002c8:	43 00 00 00 	addiu $5,$5,1
				4002cc:	01 00 05 05 
			A++;
			q1) 1 cycle stall since 2 instrs away (internal forwarding)
			q2) 1 cycle stall since 2 instrs away (WX bypass)
				4002d0:	43 00 00 00 	addiu $4,$4,1
				4002d4:	01 00 04 04 
			X = 0;...;
				addu $8,$0,$0
				addu $8,$0,$0
				addu $8,$0,$0
		*/

		A = 1;
		B = 2;
		C = A + B;
		A++;
		B = A + 1;
		X = 0; X = 0; X = 0;
		/*
			A = 1;
			$4 needs 2 cycles to be ready
				4002f0:	43 00 00 00 	addiu $4,$0,1
				4002f4:	01 00 04 00 
			B = 2;
			$5 needs 2 cycles to be ready
				4002f8:	43 00 00 00 	addiu $5,$0,2
				4002fc:	02 00 05 00 
			C = A + B;
			q1) 2 cycle stall, need to wait for both A and B
			(A only needs 1 more cycle, but B still needs 2 cycles)
			q2) 1 cycle stall, still need to wait for B 
				400300:	42 00 00 00 	addu $6,$4,$5
				400304:	00 06 05 04 
			A++;
				400308:	43 00 00 00 	addiu $4,$4,1
				40030c:	01 00 04 04 
			B = A + 1;
			q1) 2 cycle stall (internal forwarding)
			q2) 1 cycle stall (MX bypass)
				400310:	43 00 00 00 	addiu $5,$4,1
				400314:	01 00 05 04 
			X = 0;...;
				addu $8,$0,$0
				addu $8,$0,$0
				addu $8,$0,$0
		*/

		A = *D;
		c1 = A;
		X = 0; X = 0; X = 0;
		/*
			A = *D;
				400330:	28 00 00 00 	lw $4,0($7)
				400334:	00 00 04 07 
			c1 = A;
			q1) 2 cycle stall (internal forwarding)
			q2) no stalls (WM bypass)
				400338:	34 00 00 00 	sw $4,20($30)
				40033c:	14 00 04 1e 
			X = 0;...;
				addu $8,$0,$0
				addu $8,$0,$0
				addu $8,$0,$0
		*/

		B = 0;
		B++;
		c2 = B;
		X = 0; X = 0; X = 0;
		/*
			B = 0;
				400358:	42 00 00 00 	addu $5,$0,$0
				40035c:	00 05 00 00 
			B++;
			q1) 2 cycle stall (internal forwarding)
			q2) 1 cycle stall (MX bypass)
				400360:	43 00 00 00 	addiu $5,$5,1
				400364:	01 00 05 05 
			c2 = B;
			q1) 2 cycle stall (internal forwarding)
			q2) no stall (WM bypass)
				400368:	34 00 00 00 	sw $5,24($30)
				40036c:	18 00 05 1e 
			X = 0;...;
				addu $8,$0,$0
				addu $8,$0,$0
				addu $8,$0,$0
		*/
		
		A = 0;
		B = 0;
		A++;
		X = 0; X = 0; X = 0;
		/*
			A = 0;
				400388:	42 00 00 00 	addu $4,$0,$0
				40038c:	00 04 00 00 
			B = 0;
				400390:	42 00 00 00 	addu $5,$0,$0
				400394:	00 05 00 00 
			A++;
			q1) 1 cycle stall (internal forward, 2 instrs apart)
			q2) no stall (MX bypass)
				400398:	43 00 00 00 	addiu $4,$4,1
				40039c:	01 00 04 04 
			X = 0;...;
				addu $8,$0,$0
				addu $8,$0,$0
				addu $8,$0,$0
		*/

		/*
			increment iterator i
				addiu $3,$3,1
			go back to start of loop
				j 400260 <main+0x70>
		*/
	}

	return 0;
}


/*
ANALYSIS

Using -O0 optimization level compilation of the above program,
we get the objdump assembly instructions in the snippets above.
The loops is run 10,000,000 times, which means we can neglect
the effects of the initialization portion of the program.

There are a total of 42 assembly instructions within the loop
that are run each iteration.

For Q1: we expect 8 2-cycle stalls and 2 1-cycle stalls
so expected CPI = 1 + (1/42)*(2*8 + 1*2) = 1.4286

For Q2: we expect 2 2-cycle stalls and 5 1-cycle stalls
so expected CPI = 1 + (1/42)*(2*2 + 1*5) = 1.2143

Below is the statistics output of running 'sim-safe mbq1'

	sim: ** simulation statistics **
	sim_num_insn              420006322 # total number of instructions executed
	sim_num_refs               60003724 # total number of loads and stores executed
	sim_elapsed_time                 31 # total simulation time in seconds
	sim_inst_rate          13548591.0323 # simulation speed (in insts/sec)
	sim_num_RAW_hazard_q1     100000934 # total number of RAW hazards (q1)
	sim_num_RAW_hazard_q2      70000861 # total number of RAW hazards (q2)
	num_1cyc_stalls_q1         20000079 # total number of 1 cycle stalls (q1)
	num_1cyc_stalls_q2         50000769 # total number of 1 cycle stalls (q2)
	num_2cyc_stalls_q1         80000855 # total number of 2 cycle stalls (q1)
	num_2cyc_stalls_q2         20000092 # total number of 2 cycle stalls (q2)
	num_other_stalls_q1               0 # total number of non 1/2 cycle stalls (q1)
	num_other_stalls_q2               0 # total number of non 1/2 cycle stalls (q2)
	CPI_from_RAW_hazard_q1       1.4286 # CPI from RAW hazard (q1)
	CPI_from_RAW_hazard_q2       1.2143 # CPI from RAW hazard (q2)
	ld_text_base             0x00400000 # program text (code) segment base
	ld_text_size                  23536 # program text (code) size in bytes
	ld_data_base             0x10000000 # program initialized data segment base
	ld_data_size                   4096 # program init'ed `.data' and uninit'ed `.bss' size in bytes
	ld_stack_base            0x7fffc000 # program stack segment base (highest address in stack)
	ld_stack_size                 16384 # program initial stack size
	ld_prog_entry            0x00400140 # program entry point (initial PC)
	ld_environ_base          0x7fff8000 # program environment base address address
	ld_target_big_endian              0 # target executable endian-ness, non-zero if big endian
	mem.page_count                   13 # total number of pages allocated
	mem.page_mem                    52k # total size of memory pages allocated
	mem.ptab_misses                  13 # total first level page table misses
	mem.ptab_accesses        1800179328 # total page table accesses
	mem.ptab_miss_rate           0.0000 # first level page table miss rate

Our expected CPIs are correct.

*/