/*
	Richard Wu | wuricha8 | 1008078296
	Christian Vedtofte | vedtofte | 1012872125
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
	int N1 = 1000000, N2 = 2000000, N3 = 3000000, N4 = 4000000;
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
		N1) 39 * 1mil instructions, 1mil + 2 misspredictions
	*/ 
	for (i = 0; i < N1; i++) {
		/*
			loop condition assembly (from objdump):
			get N
				lw $2,16($30)
			compare iterator to N
				slt $10,$3,$2
			if loop condition met (i < N), continue to loop contents
				bne $10,$0,400310 <main+0x120>
			otherwise, jump to after loop, only executed on last instruction
				j 400380 <main+0x190>
		*/

        /* 
            Christian
            Trying to test for missed branch predictions here by having another smaller for loop inside this loop. 
        */
        /* 
            Should be slti and one branch instruction and then a lw and 6 ALU instructions and finally a jump instruction 
            for a total of 10 instructions per iteration of the inner loop. 
            If exiting the loop, there is a slti, bne that fails and then a jump, and then the outerloop increments and jumps, then outerloop lw, slt and bne. 
			for a total of 8 instructions that iteration.
            This along with the addu for setting k to 0 means a total of 9 intructions outside the inner loop per iteration of the outer loop.
            This gives a total of 1 missprediction out of 10*3+9 = 39 total instructions, and then an additional 2 mispredictions per N1 since we have two 
            misspredictions while the predictor is "training".
            For the 2-bit counter assuming it defaults to NT then it should misspredict the first two taken branches, and after that only misspredict the not taken branches
            In total it should be a lw,slt and bne
        */

		

		/*
			loop condition assembly (from objdump):
			set k to 0 
				addu $4,$0,$0
			compare iterator to 3
				slti $2,$4,3
			if loop condition met (k < 3), continue to loop contents
				bne $2,$0,400330 <main+0x140>
			otherwise, jump to after loop, only executed on last instruction
				j 400370 <main+0x180>
		*/

        for (k = 0; k<3; k++){
            A = *D;
		    B = A + 1;
		    A++;
		    X = 0; X = 0; X = 0;
    
        }

				/*
			objdump:
			A = *D;
				lw $5,0($8)
			B = A + 1;
				addiu $6,$5,1
			A++;
				addiu $5,$5,1
			3 (X = 0) instrs
				addu $9,$0,$0
				addu $9,$0,$0
				addu $9,$0,$0
		*/
		/*
			increment iterator i
				addiu $4,$4,1
			go back to start of inner loop
				j 400318 <main+0x128>
		*/

		/*
			increment iterator i
				addiu $3,$3,1
			go back to start of outer loop
				j 4002f0 <main+0x100>
		*/


	}

	/*
		N2) 33 * 2mil instructions, 2mil + 2 misspredictions
	*/
	for (i = 0; i < N2; i++) {
		/*
			loop condition assembly (from objdump):
			get N
				lw $2,20($30)
			compare iterator to N
				slt $10,$3,$2
			if loop condition met (i < N), continue to loop contents
				bne $10,$0,4003b0 <main+0x1c0>
			otherwise, jump to after loop, only executed on last instruction
				j 400408 <main+0x218>
		*/


        /* 
            Christian
            Trying to test for missed branch predictions here by having another smaller for loop inside this loop. 
        */
        /* 
            Should be slti and one branch instruction and then a lw and 3 ALU instructions and finally a jump instruction 
            for a total of 7 instructions per iteration of the inner loop. 
            If exiting the loop, there is a slti, bne that fails and then a jump, and then the outerloop increments and jumps, then outerloop lw, slt and bne. 
			for a total of 8 instructions that iteration.
            This along with the addu for setting k to 0 means a total of 9 intructions outside the inner loop per iteration of the outer loop.
            This gives a total of 1 missprediction out of 8*3+9 = 33 total instructions, and then an additional 2 mispredictions per N1 since we have two 
            misspredictions while the predictor is "training".
            For the 2-bit counter assuming it defaults to NT then it should misspredict the first two taken branches, and after that only misspredict the not taken branches
            In total it should be a lw,slt and bne
        */

		/*
			loop condition assembly (from objdump):
			set k to 0 
				addu $4,$0,$0
			compare iterator to 3
				slti $2,$4,3
			if loop condition met (k < 3), continue to loop contents
				bne $2,$0,4003d0 <main+0x1e0>
			otherwise, jump to after loop, only executed on last instruction
				j 4003f8 <main+0x208>
		*/

        for (k = 0; k<3; k++){
            A = *D;
		    B = A + 1;
		    A++;
        }

		/*
			A = *D;
				lw $5,0($8)
			B++;
				addiu $6,$5,1
			A++;
				addiu $5,$5,1
		*/
		/*
			increment iterator i
				addiu $4,$4,1
			go back to start of inner loop
				j 4003b8 <main+0x1c8>
		*/

		/*
			increment iterator i
				addiu $3,$3,1
			go back to start of outer loop
				j 400390 <main+0x1a0>
		*/
	}

	/*
		N3) 30 * 3mil instructions, 3mil + 2 misspredictions
	*/
	for (i = 0; i < N3; i++) {
		/*
			loop condition assembly (from objdump):
			get N
				lw $2,24($30)
			compare iterator to N
				slt $10,$3,$2
			if loop condition met (i < N), continue to loop contents
				bne $10,$0,400438 <main+0x248>
			otherwise, jump to after loop, only executed on last instruction
				j 400488 <main+0x298>
		*/


        /* 
            Christian
            Trying to test for missed branch predictions here by having another smaller for loop inside this loop. 
        */
        /* 
            Should be slti and one branch instruction and then a lw and 3 ALU instructions and finally a jump instruction 
            for a total of 10 instructions per iteration of the inner loop. 
            If exiting the loop, there is a slti, bne that fails and then a jump, and then the outerloop increments and jumps, then outerloop lw, slt and bne. 
			for a total of 7 instructions that iteration.
            This along with the addu for setting k to 0 means a total of 9 intructions outside the inner loop per iteration of the outer loop.
            This gives a total of 1 missprediction out of 7*3+9 = 30 total instructions, and then an additional 2 mispredictions per N1 since we have two 
            misspredictions while the predictor is "training".
            For the 2-bit counter assuming it defaults to NT then it should misspredict the first two taken branches, and after that only misspredict the not taken branches
            In total it should be a lw,slt and bne        */




		/*
			loop condition assembly (from objdump):
			set k to 0
				addu $4,$0,$0
			compare iterator to 3
				slti $2,$4,3
			if loop condition met (k < 3), continue to loop contents
				bne $2,$0,400458 <main+0x268>
			otherwise, jump to after loop, only executed on last instruction
				j 400478 <main+0x288>
		*/




        for (k = 0; k<3; k++){
            A = *D;
		    A++;
        }

        /*
			A = *D;
				lw $5,0($8)
			A++;
				addiu $5,$5,1
		*/



		/*
			increment iterator i
				addiu $4,$4,1
			go back to start of inner loop
				j 400440 <main+0x250>
		*/

		/*
			increment iterator i
				addiu $3,$3,1
			go back to start of outer loop
				j 400418 <main+0x228>
		*/
	}

	/*
		N2) 24 * 4mil instructions, 4mil + 2 misspredictions
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
            Should be slti and one branch instruction and 2 ALU instructions and finally a jump instruction 
            for a total of 6 instructions per iteration of the inner loop. 
            If exiting the loop, there is a slti, bne that fails and then a jump, and then the outerloop increments and jumps, then outerloop lw, slt and bne. 
			for a total of 8 instructions that iteration.
            This along with the addu for setting k to 0 means a total of 9 intructions outside the inner loop per iteration of the outer loop.
            This gives a total of 1 missprediction out of 5*3+9 = 24 total instructions, and then an additional 2 mispredictions per N1 since we have two 
            misspredictions while the predictor is "training".
            For the 2-bit counter assuming it defaults to NT then it should misspredict the first two taken branches, and after that only misspredict the not taken branches
            In total it should be a lw,slt and bne
        */



		/*
			loop condition assembly (from objdump):
			set k to 0
				addu $4,$0,$0
			compare iterator to 3
				slti $2,$4,3
			if loop condition met (k < 3), continue to loop contents
				bne $2,$0,4004d8 <main+0x2e8>
			otherwise, jump to after loop, only executed on last instruction
				j 4004f0 <main+0x300>
		*/



        for (k = 0; k<3; k++){
            X = 0;
        }
		/*
			X = 0;...;
				addu $9,$0,$0
		*/
		/*
			increment iterator i
				addiu $4,$4,1
			go back to start of inner loop
				j 4004c0 <main+0x2d0>
		*/

		/*
			increment iterator i
				addiu $3,$3,1
			go back to start of outer loop
				j 400498 <main+0x2a8>
		*/
	}

	return 0;
}


/*
ANALYSIS

Using -O0 optimization level compilation of the above program,
we get the objdump assembly instructions in the snippets above.
4 loops are run for 1-4 million times respectively.

Total#insn = (39 + 33*2 + 30*3 + 24*4)mil = 291,000,000
Total#prediction = (5+10+15+20)mil = 50,000,000
Total#misspredictions = (1 + 2 + 3 + 4)mil = 10,000,000

62.468 actual benchmark
Calc for 2 bit) MKI = 50,000,000 / 291,000 = 34.36426117    ------- not the result we're looking for
*/


