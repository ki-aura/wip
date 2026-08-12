#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <limits.h>

#define DEFAULT_MAX_ITERATIONS 100
#define CONVERGENCE_TOLERANCE 1e-15

int main(int argc, char *argv[]) {
    int max_iterations = DEFAULT_MAX_ITERATIONS;
    int opt;

    /* Parse command-line options */
    while ((opt = getopt(argc, argv, "i:")) != -1) {
        if (opt == 'i') {
            char *endptr;
            errno = 0;
            long val = strtol(optarg, &endptr, 10);
            if (errno != 0 || *endptr != '\0' || optarg == endptr ||
                val < 0 || val > INT_MAX) {
                fprintf(stderr, "Error: Invalid iteration count.\n");
                return 1;
            }
            max_iterations = (int)val;
        } else {
            fprintf(stderr, "Usage: %s [-i max_iterations] <target_number>\n", argv[0]);
            return 1;
        }
    }

    /* Ensure the target number was provided after options */
    if (optind >= argc) {
        fprintf(stderr, "Usage: %s [-i max_iterations] <target_number>\n", argv[0]);
        return 1;
    }

    char *endptr;
    errno = 0;

    /* Use argv[optind] to get the target, skipping parsed options */
    double target = strtod(argv[optind], &endptr);

    /* Validate parsing success and ensure no trailing non-numeric characters */
    if (errno != 0 || *endptr != '\0' || argv[optind] == endptr) {
        fprintf(stderr, "Error: Invalid number provided.\n");
        return 1;
    }

    /* Reject inf/nan, which strtod accepts but which break log2()/exp2() */
    if (!isfinite(target)) {
        fprintf(stderr, "Error: Target must be a finite number.\n");
        return 1;
    }

    /* Target cannot be negative for real-number square roots */
    if (target < 0.0) {
        fprintf(stderr, "Error: Target cannot be less than 0.\n");
        return 1;
    }

    /* Step 1 & 2: Base-2 approximation and Heron's method loop */
    double approximation = 0.0;
    int actual_iterations = 0;

    if (target > 0.0) {
        /* Obtain the rough order-of-magnitude seed.
         * floor() is used explicitly here because C's integer division
         * truncates toward zero, not toward -infinity, which would give
         * the wrong floor for negative exponents (i.e. target < 1). */
        int power = (int)floor(log2(target) / 2.0);
        approximation = exp2(power);

        /* Refine using Heron's method until convergence or the
         * iteration cap is reached, whichever comes first. Quadratic
         * convergence means most inputs settle in ~5-6 steps; the cap
         * just guards against pathological inputs or a tolerance that's
         * unreachable for a given target's magnitude. */
        for (actual_iterations = 0; actual_iterations < max_iterations; actual_iterations++) {
            double prev = approximation;
            approximation = 0.5 * (approximation + (target / approximation));
            if (fabs(approximation - prev) <= CONVERGENCE_TOLERANCE * approximation) {
                actual_iterations++; /* Increment to count the final breaking step */
                break;
            }
        }
    }

    /* Output the refined approximation, the library check, and the iteration count */
    printf("%.17f %.17f (Iterations: %d)\n", approximation, sqrt(target), actual_iterations);
    
    return 0;
}

