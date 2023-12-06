#ifndef MAIN_H
#define MAIN_H

#include <stdlib.h>
#include <portaudio.h>
#include <gtk/gtk.h>
#include <fftw3.h>

// PortAudio & GTK funcs
void 	checkError(PaError err);
void	configureInParams(int inpDevice, 
							PaStreamParameters* i);
void	activate(GtkApplication* app, gpointer data);
void	record(GtkWidget* widget, gpointer data);

// FFT preparation & calculation
//~ void 	convertToComplexArray(float* samples, float* complex, int length);
void 	convertToComplexArray(float* samples, fftwf_complex* complex, int length);
void	lowPassData(float* input, float* output, int length, int cutoff);
void 	setUpHannWindow(float* windowData, int length);
void 	setWindow(float* windowData, float* samples, int length);
float 	calcMagnitude(float real, float imaginary);
void	getPeak(fftwf_complex* result, int fftLen, float* avgFreq, int* count);
void 	getPitch(float* freq);

#endif
