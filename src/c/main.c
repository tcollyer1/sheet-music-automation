#include <string.h>
#include <stdio.h>

#include "../h/main.h"

#define SAMPLE_RATE         44100
#define WINDOW_SIZE         1024
#define CHANNELS            1


void checkError(PaError err)
{
    if (err != paNoError)
    {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        exit(1);
    }
}

void processSamples(double* buff)
{
    // Process samples here
}

// Callback function - may be needed
int paCallback
(
    const void* pInputBuffer,
    void* pOutputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* pTimeInfo,
    PaStreamCallbackFlags statusFlags,
    void* pUserData
)
{
    printf("Callback func - called\n");
    
    float* pInp = (float*)pInputBuffer;

    return 0;
}

// Sets up input/output PaStreamParameters vars
void configureIOParams(int inpDevice, int outDevice, PaStreamParameters* i, PaStreamParameters* o)
{
    memset(i, 0, sizeof(i));
    memset(o, 0, sizeof(o));
    
    printf("Input device: %s\n", Pa_GetDeviceInfo(inpDevice)->name);
    printf("Output device: %s\n", Pa_GetDeviceInfo(outDevice)->name);

    // L and R channels - input params
    i->channelCount = CHANNELS;

    i->device = inpDevice;
    i->hostApiSpecificStreamInfo = NULL;
    i->sampleFormat = paFloat32;
    i->suggestedLatency = Pa_GetDeviceInfo(inpDevice)->defaultLowInputLatency;

    // L and R channels - output params
    o->channelCount = CHANNELS;
    
    o->device = outDevice;
    o->hostApiSpecificStreamInfo = NULL;
    o->sampleFormat = paFloat32;
    o->suggestedLatency = Pa_GetDeviceInfo(outDevice)->defaultLowInputLatency;
}

int main(int argc, char** argv)
{
    // Buffer to store samples
    double* samples = (double*)malloc(sizeof(double) * WINDOW_SIZE);
    
    // Initialise PortAudio stream
    PaError err = Pa_Initialize();
    checkError(err);

    // Main code here
    int numDevices = Pa_GetDeviceCount();
    printf("Number of devices: %d\n", numDevices);

    if (numDevices < 0)
    {
        printf("Error getting the device count\n");
        exit(1);
    }
    else if (numDevices == 0)
    {
        printf("No available audio devices detected!\n");
        exit(0);
    }
    
    // Else devices found - display
    const PaDeviceInfo* pDeviceInfo;
    for (int i = 0; i < numDevices; i++)
    {
        pDeviceInfo = Pa_GetDeviceInfo(i);

        printf("-- DEVICE %d: --\n\tName: %s\n\tMax input channels: %d\n\tMax output channels: %d\n\tDefault sample rate: %f\n", 
        i, 
        pDeviceInfo->name, 
        pDeviceInfo->maxInputChannels,
        pDeviceInfo->maxOutputChannels,
        pDeviceInfo->defaultSampleRate);
    }
    
    PaStreamParameters inputParams;
    PaStreamParameters outputParams;

    // Use default input device
    int inpDevice = Pa_GetDefaultInputDevice();
    if (inpDevice == paNoDevice)
    {
        printf("No default input device.\n");
        exit(0);
    }
    
    // Use default output device
    int outDevice = Pa_GetDefaultOutputDevice();
    if (outDevice == paNoDevice)
    {
        printf("No default output device.\n");
        exit(0);
    }
    
    configureIOParams(inpDevice, outDevice, &inputParams, &outputParams);

    // Open PortAudio stream
    printf("Opening stream\n");
    PaStream* pStream;
    err = Pa_OpenStream
    (
        &pStream,
        &inputParams,
        &outputParams,
        SAMPLE_RATE,
        WINDOW_SIZE,
        paNoFlag,
        //paCallback, // Callback
        NULL, // No callback for now
        NULL // No user data
    );
    checkError(err);

    printf("Starting stream\n");
    err = Pa_StartStream(pStream);
    checkError(err);

    printf("About to start reading stream\n");
    int times = 100;
    //Pa_Sleep(10000); // Listens for 10000ms - so 10 secs
    while (times)
    {
        // Read & process samples
        err = Pa_ReadStream(pStream, samples, WINDOW_SIZE);
        checkError(err);
        
        processSamples(samples);
        
        times--;
    }
    
    err = Pa_StopStream(pStream);
    checkError(err);

    err = Pa_CloseStream(pStream);
    checkError(err);

    // --------------

    // Terminate PortAudio stream
    err = Pa_Terminate();
    checkError(err);

    return 0;
}
