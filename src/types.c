#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

u32 round_to_highest_pow_of_2(u32 v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}
