/* Teaching bug case A: intentionally wrong feature index. */
/* Build: gcc -std=c11 -Wall -Wextra -pedantic bug_case_A.c -o bug_case_A */
#include <stdio.h>
typedef struct { double features[5]; int label; } Sample;
/* features[3] = byte_00_ratio, features[4] = byte_ff_ratio */
int validate_buggy(const Sample *s) {
    if (s->label != 0 && s->label != 1) return 2;
    /* BUG: features[2] is entropy, and features[4] is left unchecked. */
    if (s->features[2] < 0 || s->features[2] > 1 ||
        s->features[3] < 0 || s->features[3] > 1) return 3;
    return 0;
}
int main(void) {
    /* Entropy is deliberately 0.5 so an indexing bug is exposed by byte_ff_ratio=1.3. */
    Sample sample = {{8192, 42, 0.5, 0.2, 1.3}, 1};
    printf("buggy_result=%d; correct_result=3 (invalid byte_ff_ratio)\n",
           validate_buggy(&sample));
    puts("Task: inspect features[3]/[4], fix the check, rerun tests.");
    return 0;
}
