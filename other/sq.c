#include <math.h>
#include <stdio.h>

float fsqrt(const float number, int iter)
{
	long i;
	float x2, y;
	const float threehalfs = 1.5F;

	x2 = number * 0.5F;
	y  = number;
	i  = * ( long * ) &y;                       // evil floating point bit level hacking
	i  = 0x5f3759df - ( i >> 1 );               // what the fuck?
	y  = * ( float * ) &i;
	y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
	if (iter == 2) {
		y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed
	}

	return y;
}


int main(void)
{
	float a, b, c;
	for(int n=1; n <= 100; n++){
		a = 1 / sqrt(n);
		b = fsqrt(n, 1);
		c = fsqrt(n, 2);
		printf("%d  %.6f  %.6f  %.6f  %.6f  %.6f\n", n, a, b, c, a-b, a-c);
	}
	return 0;
}

