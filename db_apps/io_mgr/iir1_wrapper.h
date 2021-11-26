#ifndef __IIR1_WRAPPER_H__
#define __IIR1_WRAPPER_H__

void* lowpassfilter_create();
void lowpassfilter_destroy(void* cpl);

void lowpassfilter_setup(void* clp,double sampleRate,double cutoffFrequency);
void lowpassfilter_reset(void* clp);
double lowpassfilter_filter(void* clp,double c);


#endif

