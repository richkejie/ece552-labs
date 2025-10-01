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
	int N1 = 1000000, N2 = 2000000, N3 = 3000000, N4 = 4000000, N5 = 5000000, N6 = 6000000; 
	/* register keyword tells compiler to use registers */
	register int i = 0; /* iterator */

	register int A = 0, B = 0, C = 0;
	register int *D;

	/* X used for instrs with no dependencies to skip over several instrs */
	register int X = 0;

	/* c1, c2 used to test load and store instrs */
	int c1 = 0, c2 = 0;

	/* main loops */
	/*
		N1) 11 * 1mil
		q1) 1-cyc: 0; 2-cyc: 3
		q2) 1-cyc: 1; 2-cyc: 2
	*/ 
	for (i = 0; i < N1; i++) {
		/*
			loop condition assembly (from objdump):
			get N
				lw $2,16($30)
			compare iterator to N
			q1) 2 cycle stall (internal forwarding)
			q2) 2 cycle stall (WX bypass)
				slt $9,$3,$2
			if loop condition met (i < N), continue to loop contents
			q1) 2 cycle stall (internal forwarding)
			q2) 1 cycle stall (MX bypass)
				bne $9,$0,400308 <main+0x118>
			otherwise, jump to after loop, only executed on last instruction
				j 400348 <main+0x158>
		*/
		A = *D;
		B = A + 1;
		A++;
		X = 0; X = 0; X = 0;
		/*
			objdump:
			A = *D;
				lw $4,0($7)
			B = A + 1;
			q1) 2 cycle stall (internal forwarding)
			q2) 2 cycle stall (WX bypass)
				addiu $5,$4,1
			A++;
			no stall needed, since no dependency
				addiu $4,$4,1
			3 (X = 0) instrs
				addu $8,$0,$0
				addu $8,$0,$0
				addu $8,$0,$0
		*/
		/*
			increment iterator i
				addiu $3,$3,1
			go back to start of loop
				j 4002e8 <main+0xf8>
		*/
	}

	/*
		N2) 11 * 2mil
		q1) 1-cyc: 1; 2-cyc: 2
		q2) 1-cyc: 2; 2-cyc: 1
	*/
	for (i = 0; i < N2; i++) {
		/*
			loop condition assembly (from objdump):
			get N
				lw $2,20($30)
			compare iterator to N
			q1) 2 cycle stall (internal forwarding)
			q2) 2 cycle stall (WX bypass)
				slt $9,$3,$2
			if loop condition met (i < N), continue to loop contents
			q1) 2 cycle stall (internal forwarding)
			q2) 1 cycle stall (MX bypass)
				bne $9,$0,400378 <main+0x188>
			otherwise, jump to after loop, only executed on last instruction
				j 4003b8 <main+0x1c8>
		*/
		A = *D;
		B++;
		A++;
		X = 0; X = 0; X = 0;
		/*
			A = *D;
				lw $4,0($7)
			B++;
			no stall since no dependency
				addiu $5,$5,1
			A++;
			q1) 1 cycle stall since 2 instrs away (internal forwarding)
			q2) 1 cycle stall since 2 instrs away (WX bypass)
				addiu $4,$4,1
			X = 0;...;
				addu $8,$0,$0
				addu $8,$0,$0
				addu $8,$0,$0
		*/
		/*
			increment iterator i
				addiu $3,$3,1
			go back to start of loop
				j 400358 <main+0x168>
		*/
	}

	/*
		N3) 13 * 3mil
		q1) 1-cyc: 0; 2-cyc: 4
		q2) 1-cyc: 3; 2-cyc: 1
	*/
	for (i = 0; i < N3; i++) {
		/*
			loop condition assembly (from objdump):
			get N
				lw $2,24($30)
			compare iterator to N
			q1) 2 cycle stall (internal forwarding)
			q2) 2 cycle stall (WX bypass)
				slt $9,$3,$2
			if loop condition met (i < N), continue to loop contents
			q1) 2 cycle stall (internal forwarding)
			q2) 1 cycle stall (MX bypass)
				bne $9,$0,4003e8 <main+0x1f8>
			otherwise, jump to after loop, only executed on last instruction
				j 400438 <main+0x248>
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
				addiu $4,$0,1
			B = 2;
			$5 needs 2 cycles to be ready
				addiu $5,$0,2
			C = A + B;
			q1) 2 cycle stall, need to wait for both A and B
			(A only needs 1 more cycle, but B still needs 2 cycles)
			q2) 1 cycle stall, still need to wait for B 
				addu $6,$4,$5
			A++;
				addiu $4,$4,1
			B = A + 1;
			q1) 2 cycle stall (internal forwarding)
			q2) 1 cycle stall (MX bypass)
				addiu $5,$4,1
			X = 0;...;
				addu $8,$0,$0
				addu $8,$0,$0
				addu $8,$0,$0
		*/
		/*
			increment iterator i
				addiu $3,$3,1
			go back to start of loop
				j 4003c8 <main+0x1d8>
		*/
	}

	/*
		N4) 10 * 4mil
		q1) 1-cyc: 0; 2-cyc: 3
		q2) 1-cyc: 1; 2-cyc: 1
	*/
	for (i = 0; i < N4; i++) {
		/*
			loop condition assembly (from objdump):
			get N
				lw $2,28($30)
			compare iterator to N
			q1) 2 cycle stall (internal forwarding)
			q2) 2 cycle stall (WX bypass)
				slt $9,$3,$2
			if loop condition met (i < N), continue to loop contents
			q1) 2 cycle stall (internal forwarding)
			q2) 1 cycle stall (MX bypass)
				bne $9,$0,400468 <main+0x278>
			otherwise, jump to after loop, only executed on last instruction
				j 4004a0 <main+0x2b0>
		*/
		A = *D;
		c1 = A;
		X = 0; X = 0; X = 0;
		/*
			A = *D;
				lw $4,0($7)
			c1 = A;
			q1) 2 cycle stall (internal forwarding)
			q2) no stalls (WM bypass)
				sw $4,40($30)
			X = 0;...;
				addu $8,$0,$0
				addu $8,$0,$0
				addu $8,$0,$0
		*/
		/*
			increment iterator i
				addiu $3,$3,1
			go back to start of loop
				j 400448 <main+0x258>
		*/
	}

	/*
		N5) 11 * 5mil
		q1) 1-cyc: 0; 2-cyc: 4
		q2) 1-cyc: 2; 2-cyc: 1
	*/
	for (i = 0; i < N5; i++) {
		/*
			loop condition assembly (from objdump):
			get N
				lw $2,32($30)
			compare iterator to N
			q1) 2 cycle stall (internal forwarding)
			q2) 2 cycle stall (WX bypass)
				slt $9,$3,$2
			if loop condition met (i < N), continue to loop contents
			q1) 2 cycle stall (internal forwarding)
			q2) 1 cycle stall (MX bypass)
				bne $9,$0,4004d0 <main+0x2e0>
			otherwise, jump to after loop, only executed on last instruction
				j 400510 <main+0x320>
		*/
		B = 0;
		B++;
		c2 = B;
		X = 0; X = 0; X = 0;
		/*
			B = 0;
				addu $5,$0,$0
			B++;
			q1) 2 cycle stall (internal forwarding)
			q2) 1 cycle stall (MX bypass)
				addiu $5,$5,1
			c2 = B;
			q1) 2 cycle stall (internal forwarding)
			q2) no stall (WM bypass)
				sw $5,44($30)
			X = 0;...;
				addu $8,$0,$0
				addu $8,$0,$0
				addu $8,$0,$0
		*/
		/*
			increment iterator i
				addiu $3,$3,1
			go back to start of loop
				j 4004b0 <main+0x2c0>
		*/
	}

	/*
		N6) 11 * 6mil
		q1) 1-cyc: 1; 2-cyc: 2
		q2) 1-cyc: 1; 2-cyc: 1
	*/
	for (i = 0; i < N6; i++) {
		/*
			loop condition assembly (from objdump):
			get N
				lw $2,36($30)
			compare iterator to N
			q1) 2 cycle stall (internal forwarding)
			q2) 2 cycle stall (WX bypass)
				slt $9,$3,$2
			if loop condition met (i < N), continue to loop contents
			q1) 2 cycle stall (internal forwarding)
			q2) 1 cycle stall (MX bypass)
				bne $9,$0,400540 <main+0x350>
			otherwise, jump to after loop, only executed on last instruction
				j 400580 <main+0x390>
		*/
		A = 0;
		B = 0;
		A++;
		X = 0; X = 0; X = 0;
		/*
			A = 0;
				addu $4,$0,$0
			B = 0;
				addu $5,$0,$0
			A++;
			q1) 1 cycle stall (internal forward, 2 instrs apart)
			q2) no stall (MX bypass)
				addiu $4,$4,1
			X = 0;...;
				addu $8,$0,$0
				addu $8,$0,$0
				addu $8,$0,$0
		*/
		/*
			increment iterator i
				addiu $3,$3,1
			go back to start of loop
				j 400520 <main+0x330>
		*/
	}

	return 0;
}


/*
ANALYSIS

Using -O0 optimization level compilation of the above program,
we get the objdump assembly instructions in the snippets above.
6 loops are run for 1-6 million times respectively.

Total#insn = (11 + 11*2 + 13*3 + 10*4 + 11*5 + 11*6)mil = 233,000,000

Q1) CPI = 1 + (1/233mil)(1*8mil + 2*63mil) = 1.5751
Q2) CPI = 1 + (1/233mil)(1*34mil + 2*22mil) = 1.3348

Below is the statistics output of running 'sim-safe mbq1'

	sim: ** simulation statistics **
	sim_num_insn              233006367 # total number of instructions executed
	sim_num_refs               37003734 # total number of loads and stores executed
	sim_elapsed_time                 22 # total simulation time in seconds
	sim_inst_rate          10591198.5000 # simulation speed (in insts/sec)
	sim_num_RAW_hazard_q1      71000954 # total number of RAW hazards (q1)
	sim_num_RAW_hazard_q2      56000876 # total number of RAW hazards (q2)
	num_1cyc_stalls_q1          8000079 # total number of 1 cycle stalls (q1)
	num_1cyc_stalls_q2         34000779 # total number of 1 cycle stalls (q2)
	num_2cyc_stalls_q1         63000875 # total number of 2 cycle stalls (q1)
	num_2cyc_stalls_q2         22000097 # total number of 2 cycle stalls (q2)
	num_other_stalls_q1               0 # total number of non 1/2 cycle stalls (q1)
	num_other_stalls_q2               0 # total number of non 1/2 cycle stalls (q2)
	CPI_from_RAW_hazard_q1       1.5751 # CPI from RAW hazard (q1)
	CPI_from_RAW_hazard_q2       1.3348 # CPI from RAW hazard (q2)
	ld_text_base             0x00400000 # program text (code) segment base
	ld_text_size                  23984 # program text (code) size in bytes
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
	mem.ptab_accesses        1006182226 # total page table accesses
	mem.ptab_miss_rate           0.0000 # first level page table miss rate

Our expected CPIs are correct.

*/