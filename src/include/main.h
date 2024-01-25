#ifndef MAIN_H
#define MAIN_H

#include <stdlib.h>
#include <stdbool.h>
#include <portaudio.h>
#include <gtk/gtk.h>
#include <fftw3.h>

// PortAudio & GTK funcs
void 	checkError(PaError err);
void	configureInParams(int inpDevice, PaStreamParameters* i);
void	activate(GtkApplication* app, gpointer data);

void*	record(void* args);
void	toggleRecording(GtkWidget* widget, gpointer data);

// FFT preparation & calculation
void 	convertToComplexArray(float* samples, fftwf_complex* complex, int length);

void 	saveOverlappedSamples(const float* samples, float* overlap, int len);
void 	overlapWindow(const float* samples, const float* overlap, float* newSamples, int len);

void	lowPassData(float* input, float* output, int length, int cutoff);

void 	setUpHannWindow(float* windowData, int length);
void 	setWindow(float* windowData, float* samples, int length);

float 	calcMagnitude(float real, float imaginary);

int 	getArrayLen(int fftLen, int idx);
void 	harmonicProductSpectrum(fftwf_complex* result, float* outResult, int length);
void 	downsample(const fftwf_complex* result, float* out, int outLength, int idx);
void 	hps_getPeak(float* dsResult, int len, bool isOnset);
float   interpolate(float first, float last);

char* 	getPitch(float freq);

// Adding to output buffers
void 	pitchesAdd(char* pitch, int length);
void 	displayBufferContent();

#endif
