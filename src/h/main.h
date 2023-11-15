#ifndef MAIN_H
#define MAIN_H

#include <stdlib.h>
#include <portaudio.h>
#include <gtk/gtk.h>

// PortAudio & GTK funcs
void 	checkError(PaError err);
void	configureInParams(int inpDevice, 
							PaStreamParameters* i);
void	activate(GtkApplication* app, gpointer data);
void	record(GtkWidget* widget, gpointer data);

// FFT calculation
void 	set2ndOrderLowPassFilterParams(float maxFreq, float sampleRate, float* paramsA, float* paramsB);
void	lowPassData(float* pSample, float* mem, float* paramsA, float* paramsB);
void 	setUpHannWindow(float* windowData, int length);
void 	setWindow(float* windowData, float* samples, int length);

#endif
