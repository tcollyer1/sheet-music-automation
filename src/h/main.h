#ifndef MAIN_H
#define MAIN_H

#include <stdlib.h>
#include <portaudio.h>

void 	checkError(PaError err);
int 	paCallback(const void* pInputBuffer,
					void* pOutputBuffer,
					unsigned long framesPerBuffer,
					const PaStreamCallbackTimeInfo* pTimeInfo,
					PaStreamCallbackFlags statusFlags,
					void* pUserData);
void	configureIOParams(int inpDevice,
							int outDevice, 
							PaStreamParameters* i, 
							PaStreamParameters* o);
void 	processSamples(double* buff);

#endif
