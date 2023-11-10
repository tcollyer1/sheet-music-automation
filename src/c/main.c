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

//void onRecordClicked(GtkButton* button, gpointer data)
//{
//    g_idle_add(record, data);
//}

void record(GtkWidget* widget, gpointer data)
{
    //gtk_label_set_text(label, "Recording...");

    // Update GUI with label update?
    //while (gtk_events_pending())
    //{
    //    gtk_main_iteration();
    //}

    // Record here
    printf("\nrecord() - called\n");
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
}

void activate(GtkApplication* app, gpointer data)
{
    GtkWidget* pWindow;
    GtkWidget* pButton;
    GtkWidget* pRecordBox;

    pWindow = gtk_application_window_new(app);

    // Set the window position & default size
    gtk_window_set_position(GTK_WINDOW(pWindow), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(pWindow), 1000, 800);
    gtk_window_set_title(GTK_WINDOW(pWindow), "Sheet Music Automation");
    gtk_window_set_resizable(GTK_WINDOW(pWindow), FALSE); // Non-resizable for now

    // Add a button box for record button
    pRecordBox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_container_add(GTK_CONTAINER(pWindow), pRecordBox);

    // Add the button and connect click event to record() func
    pButton = gtk_button_new_with_label("Record");
    g_signal_connect(pButton, "clicked", G_CALLBACK(record), NULL);
    //g_signal_connect_swapped(pButton, "clicked", G_CALLBACK(gtk_widget_destroy), pWindow);
    gtk_container_add(GTK_CONTAINER(pRecordBox), pButton);

    // 2) Make the X button close the window correctly & ends program
    g_signal_connect(pWindow, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // 3) Show window
    gtk_widget_show_all(pWindow);
}

int main(int argc, char** argv)
{
    // Initialise and run GTK app
    GtkApplication* app;
    int result = 0;

    app = gtk_application_new("pitch.detection", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    result = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return (result);
}
