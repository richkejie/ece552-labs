#include <stdio.h>
#include <stdlib.h>

/*
	Microbenchmark to test stride prefetcher

	compilation cmd:
		/cad2/ece552f/compiler/bin/ssbig-na-sstrix-gcc mbq2.c -O0 -o mbq2
*/

int main(void){
		
/*
	initialize the registers to iterate over memory addresses
*/

	asm("move	$3,$sp			");

    asm("addu	$7, $sp, 25600000	"); //address register for case 2
	asm("addu	$10, $sp, 51200000	"); //alternate address register for case 2

	asm("addu	$5, $sp, 12800000	");
	asm("addu	$9, $sp, 51200000	");

/*
	Case 1: We will access memory incrementing at alternating one and two block sizes
*/
	asm("move	$6, $0			"); //set a flag to alternate between incrementing by one and two block sizes
	asm("$L101:	sltu	$2,$3,$5	"); 
	asm("beq	$2, $0, $L201		");
	asm("lw		$4, 0($3)		"); //memory access
	asm("beq	$6, $0, $L102		"); //check flag 
	asm("addu	$3, $3, 64		"); //increment index by 1 block size
	asm("addu 	$6, $0, 0		"); //update flag
	asm("j		$L101			");
	asm("$L102:	addu	$3,$3,128	"); //increment by 2 block sizes
	asm("addu 	$6, $0, 1		"); //update flag
	asm("j		$L101			");
/*
	State sequence of the load instruction:
	Iteration	|	Address accessed	|	Stride detected	|	Prefetch address 	|   State
	1			|	0					|	--				|	--					|	Initial
	2			|	128					|	128				|	256					|	Transient
	3			|	192					|	64				|	256					|	No Prediction
	4			|	320					|	128				|	448					|	No Prediction
	...
	will stay in No Prediction state for the rest of the iterations
	thus the right prefetches will never be made

	There will be (12800000 / (128+64)) * 2 = ~133k iterations
	--> ~133k cache accesses and misses on the load instruction ---> ~100% data cache miss rate

	As for instruction cache, since the loop is predictable, there will be very few misses
	--> ~0% instruction cache miss rate
*/


/*
	Case 2: Fetch 1,2,1,2... block sizes again, but this time using two load instructions
		--> stride prefetcher will learn the pattern and prefetch correctly
		This will run 200k times --> ~400k data cache accesses --> ~400k data cache hits
		--> ~0% data cache miss rate
*/

    // asm("$L201:	nop");
	asm("$L201:	sltu	$2,$7,$9	");
	asm("beq	$2,$0,$END		");
	asm("lw		$8,0($7)		"); //this lw indexes memory at every 2 block sizes
	asm("lw		$8,0($10)		"); //this lw indexes memory at every block size
	asm("addu	$7,$7,128		");
	asm("addu	$10,$10,64		");
	asm("j	$L201");

/*
	State sequence of the first load instruction (offset by 25600000):
	Iteration	|	Address accessed	|	Stride detected	|	Prefetch address 	|   State		| Hit/Miss
	1			|	25600000			|	--				|	--					|	Initial		|	Miss
	2			|	25600128			|	128				|	25600256			|	Transient	|	Miss
	3			|	25600256			|	128				|	25600384			|	Steady		|	Hit
	4			|	25600384			|	128				|	25600512			|	Steady		|	Hit
	...
	Thus, the stride prefetcher will stay in Steady state for the rest of the iterations
	thus the right prefetches will be made
*/

/*
	All together:
	Total data cache accesses: ~533k
	Total data cache misses: ~133k
	Data cache miss rate: ~25%
	Instruction cache miss rate: ~0%
*/

	asm("$END:	nop");
}