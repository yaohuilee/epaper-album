#include "epaper_pcm_resample.h"
#include <stdint.h>
size_t epaper_pcm_16_to_24(const int16_t *input, size_t count, int16_t *output, size_t capacity)
{
    if (!input || !output || !count || count > SIZE_MAX / 3)
        return 0;
    size_t samples = count * 3 / 2;
    if (capacity < samples) return 0;
    for (size_t i = 0; i < samples; i++) {
        size_t position = i * 2 / 3;
        unsigned fraction = (unsigned)(i * 2 % 3);
        int32_t a = input[position];
        int32_t b = input[position + 1 < count ? position + 1 : position];
        output[i] = (int16_t)((a * (3 - (int)fraction) + b * (int)fraction) / 3);
    }
    return samples;
}
