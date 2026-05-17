#ifndef READ_ADC_H
#define READ_ADC_H

#include "fft.h"
#define NUM_BINS 64
#define NUM_SAMPLES 10000 // 30000
#define SAMPLE_RATE 10000

void init_adc();
void init_tuning_dma();
float get_freq(kiss_fft_cfg cfg, int buffer_num);

#endif