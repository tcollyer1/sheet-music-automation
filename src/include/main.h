#ifndef MAIN_H
#define MAIN_H

#include <stdlib.h>
#include <stdbool.h>
#include <portaudio.h>
#include <gtk/gtk.h>
#include <fftw3.h>
#include "midifile.h"

// PortAudio & GTK funcs
void 	checkError(PaError err);
void	configureInParams(int inpDevice, PaStreamParameters* i);
void	activate(GtkApplication* app, gpointer data);

void*	record(void* args);
void	toggleRecording(GtkWidget* widget, gpointer data);

// FFT preparation & calculation
void 	convertToComplexArray(float* samples, fftwf_complex* complex, int length);

void 	saveOverlappedSamples(const float* samples, float* overlapPrev, int len);
void 	overlapWindow(const float* samples, const float* nextSamples, const float* overlapPrev, float* newSamples, int len, bool* firstRun, const bool lastRun);

void	lowPassData(float* input, float* output, int length, int cutoff);

void 	setUpHannWindow(float* windowData, int length);
void 	setWindow(float* windowData, float* samples, int length);

float 	calcMagnitude(float real, float imaginary);

int 	getArrayLen(int fftLen, int idx);
void 	harmonicProductSpectrum(fftwf_complex* result, float* outResult, int length);
void 	downsample(const fftwf_complex* result, float* out, int outLength, int idx);
void 	hps_getPeak(float* dsResult, int len, bool isOnset);
float   interpolate(float first, float last);

char* 			getPitch(float freq, int* midiNote);
tMIDI_KEYSIG 	getMIDIKey(const char* keySig);
int 			getTimeSigDenom(const char* selected);

// Adding to output buffers
void 	pitchesAdd(char* pitch, int length, int midiNote);
void 	displayBufferContent();

// MIDI
//int 	getNoteType(float noteDur, float qNoteLen, int* upperPossibility, float* lenReq);
int 	getNoteType(float noteDur, float qNoteLen);
void 	setMidiNotes();
void 	outputMidi(float frameTime);

#endif
