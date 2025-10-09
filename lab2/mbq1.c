// ------ I refer to lw in all the loops, but that is wrong since the loops compare to an immediate
/*
	Richard Wu | wuricha8 | 1008078296
	microbenchmark for ECE552 Lab 2

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
	register int k = 0; /* iterator */

	register int A = 0, B = 0, C = 0;
	register int *D;

	/* X used for instrs with no dependencies to skip over several instrs */
	register int X = 0;

	/* c1, c2 used to test load and store instrs */
	int c1 = 0, c2 = 0;

	/* main loops */
	/*
		N1) 37 * 1mil instructions, 1mil + 2 misspredictions
	*/ 
	for (i = 0; i < N1; i++) {
		/*
			loop condition assembly (from objdump):
			get N
				lw $2,16($30)
			compare iterator to N
				slt $9,$3,$2
			if loop condition met (i < N), continue to loop contents
				bne $9,$0,400308 <main+0x118>
			otherwise, jump to after loop, only executed on last instruction
				j 400348 <main+0x158>
		*/



        /* 
            Christian
            Trying to test for missed branch predictions here by having another smaller for loop inside this loop. 
         */
        /* 
            Should be a lw, a slt and one branch instruction and then 7 ALU instructions and finally a jump instruction. 
            If exiting the loop, there is a lw, a slt, bne that fails and then a jump for a total of 4 instructions that iteration.
            This gives a total of 1 missprediction out of (3+7+1)*3+4 = 37 total instructions, and then an additional 2 mispredictions per N1 since we have two 
            misspredictions while the predictor is "training".
            For the 2-bit counter assuming it defaults to NT then it should misspredict the first two taken branches, and after that only misspredict the not taken branches
            In total it should be a lw,slt and bne
         */
        for (int k = 0; k<3; k++){
            A = *D;
		    B = A + 1;
		    A++;
		    X = 0; X = 0; X = 0;
    
        }

				/*
			objdump:
			A = *D;
				lw $4,0($7)
			B = A + 1;
				addiu $5,$4,1
			A++;
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
		N2) 28 * 2mil instructions, 2mil + 2 misspredictions
	*/
	for (i = 0; i < N2; i++) {
		/*
			loop condition assembly (from objdump):
			get N
				lw $2,20($30)
			compare iterator to N
				slt $9,$3,$2
			if loop condition met (i < N), continue to loop contents
				bne $9,$0,400378 <main+0x188>
			otherwise, jump to after loop, only executed on last instruction
				j 4003b8 <main+0x1c8>
		*/


        /* 
            Christian
            Trying to test for missed branch predictions here by having another smaller for loop inside this loop. 
         */
        /* 
            Should be a lw, a slt and one branch instruction and then 7 ALU instructions and finally a jump instruction. 
            If exiting the loop, there is a lw, a slt, bne that fails and then a jump for a total of 4 instructions that iteration.
            This gives a total of 1 missprediction out of (3+4+1)*3+4 = 28 total instructions, and then an additional 2 mispredictions per N1 since we have two 
            misspredictions while the predictor is "training".
            For the 2-bit counter assuming it defaults to NT then it should misspredict the first two taken branches, and after that only misspredict the not taken branches
            In total it should be a lw,slt and bne
         */
        for (int k = 0; k<3; k++){
            A = *D;
		    B = A + 1;
		    A++;
        }

		/*
			A = *D;
				lw $4,0($7)
			B++;
				addiu $5,$5,1
			A++;
				addiu $4,$4,1
		*/
		/*
			increment iterator i
				addiu $3,$3,1
			go back to start of loop
				j 400358 <main+0x168>
		*/
	}

	/*
		N3) 22 * 3mil instructions, 3mil + 2 misspredictions
	*/
	for (i = 0; i < N3; i++) {
		/*
			loop condition assembly (from objdump):
			get N
				lw $2,24($30)
			compare iterator to N
				slt $9,$3,$2
			if loop condition met (i < N), continue to loop contents
				bne $9,$0,4003e8 <main+0x1f8>
			otherwise, jump to after loop, only executed on last instruction
				j 400438 <main+0x248>
		*/


        /* 
            Christian
            Trying to test for missed branch predictions here by having another smaller for loop inside this loop. 
         */
        /* 
            Should be a lw, a slt and one branch instruction and then 7 ALU instructions and finally a jump instruction. 
            If exiting the loop, there is a lw, a slt, bne that fails and then a jump for a total of 4 instructions that iteration.
            This gives a total of 1 missprediction out of (3+2+1)*3+4 = 22 total instructions, and then an additional 2 mispredictions per N1 since we have two 
            misspredictions while the predictor is "training".
            For the 2-bit counter assuming it defaults to NT then it should misspredict the first two taken branches, and after that only misspredict the not taken branches
            In total it should be a lw,slt and bne
         */
        for (int k = 0; k<3; k++){
            A = *D;
		    A++;
        }

        /*
			A = *D;
				lw $4,0($7)
			A++;
				addiu $4,$4,1
		*/



		/*
			increment iterator i
				addiu $3,$3,1
			go back to start of loop
				j 4003c8 <main+0x1d8>
		*/
	}

	/*
		N2) 19 * 4mil instructions, 4mil + 2 misspredictions
	*/
	for (i = 0; i < N4; i++) {
		/*
			loop condition assembly (from objdump):
			get N
				lw $2,28($30)
			compare iterator to N
				slt $9,$3,$2
			if loop condition met (i < N), continue to loop contents
				bne $9,$0,400468 <main+0x278>
			otherwise, jump to after loop, only executed on last instruction
				j 4004a0 <main+0x2b0>
		*/



        /* 
            Christian
            Trying to test for missed branch predictions here by having another smaller for loop inside this loop. 
         */
        /* 
            Should be a lw, a slt and one branch instruction and then 7 ALU instructions and finally a jump instruction. 
            If exiting the loop, there is a lw, a slt, bne that fails and then a jump for a total of 4 instructions that iteration.
            This gives a total of 1 missprediction out of (3+1+1)*3+4 = 19 total instructions, and then an additional 2 mispredictions per N1 since we have two 
            misspredictions while the predictor is "training".
            For the 2-bit counter assuming it defaults to NT then it should misspredict the first two taken branches, and after that only misspredict the not taken branches
            In total it should be a lw,slt and bne
         */
        for (int k = 0; k<3; k++){
            X = 0;
        }
		/*
			X = 0;...;
				addu $8,$0,$0
		*/
		/*
			increment iterator i
				addiu $3,$3,1
			go back to start of loop
				j 400448 <main+0x258>
		*/
	}

	return 0;
}


/*
ANALYSIS

Using -O0 optimization level compilation of the above program,
we get the objdump assembly instructions in the snippets above.
6 loops are run for 1-6 million times respectively.

Total#insn = (11 + 11*2 + 13*3 + 10*4)mil = 233,000,000

Q1) CPI = 1 + (1/233mil)(1*8mil + 2*63mil) = 1.5751
Q2) CPI = 1 + (1/233mil)(1*34mil + 2*22mil) = 1.3348
*/
