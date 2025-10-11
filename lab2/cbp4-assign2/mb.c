/*
	Richard Wu | wuricha8 | 1008078296
	Christian
	microbenchmark for ECE552 Lab 2
*/

#include <stdio.h>

int main() {

	/* initialization */
	int N1 = 10000000;
	// movl   $0x989680,-0x14(%rbp)

	register int i = 0; /* iterator */
	// mov    $0x0,%ebx

	register int A = 0;
	// mov    $0x0,%r12d

	register int B = 0;
	// mov    $0x0,%r13d


	/* main loop */
	for (i = 0; i < N1; i++) {
    // cmp    -0x14(%rbp),%ebx
    // jl     1148 <main+0x23>
/*
	115a (jl): will be taken every iteration except the last
	1) mispredict: wrong 1st iter; wrong last iter
	2) history will become: 1 1 1 1 1 1
		pattern will learn to predict: S_T --> so ~ no mispredictions here
*/

		if (A == 9) {
			A = 0;
		} else {
			A++;
		}
/*
    cmp    $0x9,%r12d
    jne    1156 <main+0x31>
    mov    $0x0,%r12d
    jmp    115a <main+0x35>
    add    $0x1,%r12d
*/
/*
	not taken every 10 iters
	1) W_NT -> W_T -> S_T -> S_T --> ... -> W_T (10th iter) -> S_T
	will mispredict every 10 iters --> ~1,000,000 mispredicts
	2) 10 iters outside of 6 bit history --> mispredict on 10th iter --> ~1,000,000 mispredicts
*/


		if (B == 1) {
			B = 0;
		} else {
			B = 1;
		}
/*
    cmp    $0x1,%r12d
    jne    1177 <main+0x52>
    mov    $0x0,%r12d
    jmp    117d <main+0x58>
    mov    $0x1,%r12d
*/
/*
	1144 (jne): 1st iter: T; will be taken every other loop iteration
	1) predict: wrong 1st iter, state becomes W_T; wrong 2nd iter, state becomes W_NT;
		...will mispredict every loop iteration...
	2) BHR will become: 1 0 1 0 1 0 or 0 1 0 1 0 1
		patterns will learn to predict correctly for each of those patterns
		--> so no ~ mispredictions here
*/
/*
    add    $0x1,%ebx
*/
	}
	return 0;
}

/*
Total mispredicts:
1) 1 + 1 + 10,000,000 + 1,000,000 =~ 11,000,000
2) ~1,000,000
*/