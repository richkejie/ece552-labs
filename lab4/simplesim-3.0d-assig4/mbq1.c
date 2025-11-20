#include <stdio.h>
#include <stdlib.h>

/*
	Microbenchmark to test next-line prefetcher

	compilation cmd:
		/cad2/ece552f/compiler/bin/ssbig-na-sstrix-gcc mbq1.c -O0 -o mbq1
*/

int main(void){
		
/*
	initialize the registers to iterate over memory addresses
*/

	asm("move	$3,$sp");
	asm("move	$6,$sp");
	asm("addu	$6, $6, 12800000"); // $6 used in Scenario 2

	asm("addu	$5, $sp, 12800000"); // set the loop limits
	asm("addu	$8, $sp, 25600000");

/*
	Scenario 1: memory access will jump two blocks at a time (128)
		This goes outside next-line fetch range, so data cache miss should occur every time
		This will iterate 100k times so should get ~100,000 dl1 cache misses here
// */
	asm("$L101:	sltu	$2,$3,$5");
	asm("beq	$2,$0,$L201");
	asm("lw		$4,0($3)");
	asm("addu	$3,$3,128"); //increment by 2 block sizes
	asm("j	$L101");

/*
	Scenario 2: memory access will jump one block at a time (64)
		The next-line prefetcher will work successfully
		This loop will run 200k times so should get ~0 dl1 cache misses here
*/
  
	asm("$L201:	sltu	$2,$6,$8");
	asm("beq	$2,$0,$L1");
	asm("lw		$7,0($6)");
	asm("addu	$6,$6,64"); //increment by 1 block size
	asm("j	$L201");

}

/*
should be ~300,000 dl1 cache accesses
with ~100,000 dl1 cache misses
so miss rate of ~33.33%
*/