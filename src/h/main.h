#ifndef MAIN_H
#define MAIN_H

#include <stdlib.h>
#include <portaudio.h>
#include <gtk/gtk.h>
#include <fftw3.h>

typedef struct 
{
    float a;
    float b;
    float c;
} COEFFICIENTS;

// PortAudio & GTK funcs
void 	checkError(PaError err);
void	configureInParams(int inpDevice, 
							PaStreamParameters* i);
void	activate(GtkApplication* app, gpointer data);

//void*	record(void* args);
void 	record(GtkWidget* widget, gpointer data);

//void	initRecording(GtkWidget* widget, gpointer data);
void	stopRecording(GtkWidget* widget, gpointer data);

// FFT preparation & calculation
void 	convertToComplexArray(float* samples, fftwf_complex* complex, int length);

void	lowPassData(float* input, float* output, int length, int cutoff);

void 	setUpHannWindow(float* windowData, int length);
void 	setWindow(float* windowData, float* samples, int length);

float 	calcMagnitude(float real, float imaginary);

int 	getArrayLen(int fftLen, int idx);
void 	harmonicProductSpectrum(fftwf_complex* result, float* outResult, int length);
void 	downsample(const fftwf_complex* result, int length, float* out, int outLength, int idx);
void 	hps_getPeak(fftwf_complex* result, float* dsResult, int len, float* avgFreq, int* count);
float   interpolate(float first, float second, float last);

void	getPeak(fftwf_complex* result, int fftLen, float* avgFreq, int* count);
void 	getPitch(float* freq);
void	quadraticRegr(const float* values, int num);
void 	summations(const float* result, int numItems);
void 	solve (COEFFICIENTS eq1, COEFFICIENTS eq2, COEFFICIENTS eq3, float* a, float* b, float* c);

#endif
