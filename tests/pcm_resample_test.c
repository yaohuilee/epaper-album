#include "epaper_pcm_resample.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "FAIL resample line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
int main(void)
{
    int16_t output[8] = {123};
    const int16_t ramp[] = {0, 300, 600, 900};
    CHECK(epaper_pcm_16_to_24(ramp, 4, output, 8) == 6);
    const int16_t expected[] = {0, 200, 400, 600, 800, 900};
    for (unsigned i = 0; i < 6; i++) CHECK(output[i] == expected[i]);
    const int16_t extremes[] = {-32768, 32767};
    CHECK(epaper_pcm_16_to_24(extremes, 2, output, 8) == 3);
    CHECK(output[0] == -32768 && output[1] == 10922 && output[2] == 32767);
    const int16_t constant[] = {32767, 32767, 32767};
    CHECK(epaper_pcm_16_to_24(constant, 3, output, 8) == 4);
    for (unsigned i = 0; i < 4; i++) CHECK(output[i] == 32767);
    CHECK(epaper_pcm_16_to_24(ramp, 1, output, 8) == 1 && output[0] == 0);
    output[0] = 123;
    CHECK(!epaper_pcm_16_to_24(ramp, 4, output, 5) && output[0] == 123);
    CHECK(!epaper_pcm_16_to_24(NULL, 4, output, 8));
    CHECK(!epaper_pcm_16_to_24(ramp, 4, NULL, 8));
    CHECK(!epaper_pcm_16_to_24(ramp, 0, output, 8));
    CHECK(!epaper_pcm_16_to_24(ramp, SIZE_MAX, output, 8));
    printf("PCM resample tests: %u checks passed\n", checks);
    return 0;
}
