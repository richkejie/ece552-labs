#include <stdio.h>
#include <stdlib.h>

/*
    Microbenchmark to test open-ended stride prefetcher: GHB approach

    compilation cmd:
        /cad2/ece552f/compiler/bin/ssbig-na-sstrix-gcc mbq6.c -O0 -o mbq6
*/

int main(void){

/*
	initialize the registers to iterate over memory addresses
*/
		
	asm("move	$3,$sp			");	        //$3 used in case 1
	asm("move	$7,$sp			");	
	asm("addu	$7, $7, 12800000	");	    //$7 and $ 10 used in case 2
    asm("move	$10, $7			");	

	asm("addu	$5, $sp, 12800000	");     // set loop limits
	asm("addu	$9, $7,  25600000	");

/*
    Scenario 1: memory access will jump by 1, 1, 2 block sizes repeatedly
        Since the GHB tracks sequences of two matching strides
        it will be able to predict the next stride accurately

        There will be (12800000 / (64+64+128)) * 3 = ~150k iterations
        --> ~150,000 cache accesses and hits on the load instruction
        --> ~0% data cache miss rate
*/

	asm("move	$6, $0			");             // $6 is flag: 0, 1, 2 to create the pattern
	asm("$SCEN1_1:	sltu	$2,$3,$5	"); 
	asm("beq	$2, $0, $SCEN2_1		");
	asm("lw		$4, 0($3)		");             // memory access

	asm("beq	$6, $0, $SCEN1_3		");     // if $6 == 0 , go to $SCEN1_3
	asm("sltiu	$2, $6, 2		");
	asm("bne	$2, $0, $SCEN1_2		");     // if $6 == 1 , go to $SCEN1_2
						                        // if $6 == 2 (else)
	asm("addu	$3, $3, 64		");             // increment by 1 block size
	asm("addu 	$6, $0, 0		");             // change flag to 0
	asm("j		$SCEN1_1			");

	asm("$SCEN1_2:	addu	$3,$3,64	");     // increment by 1 block size
	asm("addu 	$6, $0, 2		");             // change flag to 2
	asm("j		$SCEN1_1			");

	asm("$SCEN1_3:	addu	$3,$3,128	"); // increment by 2 block size
	asm("addu 	$6, $0, 1		"); // change flag to 1
	asm("j		$SCEN1_1			");


/*
    Scenario 2: memory accesses done by 2 load instructions, but they 
        load at addresses incrementing at 1 and 2 block sizes respectively
        because of this, GHB cannot find a sequence of two matching strides
        and will not prefetch correctly

        There will be 200k iterations --> ~400k cache accesses from the two loads
        --> ~400k cache misses --> ~100% data cache miss rate
*/

    // asm("$SCEN2_1: nop            ");

	asm("$SCEN2_1:	sltu	$2,$7,$9	");
	asm("beq	$2,$0,$END		");
	asm("lw		$8,0($7)		");             // memory access that jumps every 2 block sizes
	asm("lw		$8,0($10)		");             // memory access that jumpes every 1 block size
	asm("addu	$7,$7,128		");             // increment by 2 block sizes
	asm("addu	$10,$10,64		");             // increment by 1 block size
	asm("j	$SCEN2_1");

    asm("$END:	nop			");

/*
should be ~550,000 dl1 cache accesses
with ~400,00 dl1 cache misses
so miss rate of ~72.72%
*/

}