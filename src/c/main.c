#include <string.h>
#include <stdio.h>
#include <math.h> // M_PI, sqrt, sin, cos

#include "../h/main.h"

#define REAL 0
#define IMAG 1

#define SAMPLE_RATE         44100
#define WINDOW_SIZE         1024
#define CHANNELS            1       // Mono input
#define MAX_FREQUENCY       524     // For now, limit the range to two piano octaves from  
                                    // C3-C5, so a frequency range of 130.8 Hz - 523.25 Hz
#define BIN_SIZE            (SAMPLE_RATE / WINDOW_SIZE)

// For now, manually establish notes and corresponding
// frequencies
char* notes[25] = 
{ 
    "C3", "C#3", "D3", "D#3", "E3", "F3", "F#3", "G3", "G#3", "A3", "Bb3", "B3",
    "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4", "Bb4", "B4",
    "C5"
};

// Add C#5 purely for upper bound checking in case the note detected is a C5
float frequencies[26] = 
{
    // C    // C#   // D    // D#   // E    // F    // F#   // G    // G#   // A    // Bb   // B
    130.81, 138.59, 146.83, 155.56, 164.81, 174.61, 185.00, 196.00, 207.65, 220.00, 233.08, 246.94,
    261.63, 277.18, 293.66, 311.13, 329.63, 349.23, 369.99, 392.00, 415.30, 440.00, 466.16, 493.88,
    523.25, 554.37
};


void checkError(PaError err)
{
    if (err != paNoError)
    {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        exit(-1);
    }
}

// FFTW3 complex array types are in the form
//
// typedef float fftwf_complex[2];
//
// where 0 = real, 1 = imaginary
void convertToComplexArray(float* samples, fftwf_complex* complex, int length)
{    
    for (int i = 0; i < length; i++)
    {
        // Every odd sample is the imaginary part - fill with 0s for now
        complex[i][IMAG] = 0.0f;
        
        // Every even sample is the real part
        complex[i][REAL] = samples[i];
    }
}

// Calculates magnitude
float calcMagnitude(float real, float imaginary)
{
    float magnitude = sqrt(real * real + imaginary * imaginary);
    
    return (magnitude);
}

// Prints the (estimated) pitch of a note (needs refining) based on a frequency.
void getPitch(float* freq)
{
    char* pitch = NULL;
    int found = 0;
    
    float lastFreq = 0.0f;
    
    // 25 notes in range C3-C5
    for (int i = 0; i < 25; i++)
    {
        if (i != 0)
        {
            lastFreq = frequencies[i-1];
        }
        
        if (*freq > lastFreq && *freq < frequencies[i+1])
        {
            if (abs(frequencies[i] - *freq) < abs(frequencies[i+1] - *freq))
            {
                pitch = notes[i];
                found = 1;
            }
        }
        
        if (found == 1)
        {
            break;
        }  
    }
    
    if (pitch != NULL)
    {
        printf("\nNOTE DETECTED: %s\n", pitch);
    }
    // Frequency not in C3-C5 range
    else
    {
        printf("\n[!] Note undetectable\n");
        *freq = 0.0f;
    }
}

// Gets the peak magnitude from the computed FFT output
void getPeak(fftwf_complex* result, float peakFreq, int fftLen, float* avgFreq, int* count)
{
    float highest = 0.0f;
    float current = 0.0f;
    int peakBinNo = 0;
    
    for (int i = 0; i < fftLen; i++)
    {
        current = calcMagnitude(result[i][REAL], result[i][IMAG]);
        
        if (current > highest)
        {
            highest = current;
            peakBinNo = i;
        }
    }
    
    peakFreq = peakBinNo * BIN_SIZE;
    
    printf("\nPeak frequency obtained: %f\n", peakFreq);
    
    // Estimate the pitch based on the highest frequency reported
    getPitch(&peakFreq);
    
    if (peakFreq != 0.0f)
    {
        (*avgFreq) += peakFreq;
        (*count)++;
    }
}

// Not currently working properly
void lowPassData(float* input, float* output, int length, int cutoff)
{
    // Filter constant
    float rc = 1.0 / (cutoff * 2 * M_PI);    
    
    float dt = 1.0 / SAMPLE_RATE;
    
    float alpha = dt / (rc + dt);
    
    output[0] = input[0];
    
    for (int i = 1; i < length; i++)
    {
        output[i] = output[i-1] + (alpha * (input[i] - output[i-1]));
    }
}

// Sets up the window using the Hann function
void setUpHannWindow(float* windowData, int length)
{
    for (int i = 0; i < length; i++)
    {
        // Hann function
        windowData[i] = 0.5 * (1.0 - cos(2 * M_PI * i / (length - 1.0)));
    }
}

// Applies Hann window to samples
void setWindow(float* windowData, float* samples, int length)
{
    for (int i = 0; i < length; i++)
    {
        // Apply windowing function to the data
        samples[i] = samples[i] * windowData[i];
    }
}

// Sets up input PaStreamParameters
void configureInParams(int inpDevice, PaStreamParameters* i)
{
    memset(i, 0, sizeof(i));
    
    printf("Input device selected: %s (device %d)\n", Pa_GetDeviceInfo(inpDevice)->name, inpDevice);

    i->channelCount = CHANNELS; // 1

    i->device = inpDevice;
    i->hostApiSpecificStreamInfo = NULL;
    i->sampleFormat = paFloat32; // FP values between 0.0-1.0
    i->suggestedLatency = Pa_GetDeviceInfo(inpDevice)->defaultHighInputLatency;
}

// Main function for processing microphone data.
void record(GtkWidget* widget, gpointer data)
{
    //gtk_label_set_text(label, "Recording...");

    // Update GUI with label update?
    //while (gtk_events_pending())
    //{
    //    gtk_main_iteration();
    //}

    printf("\nrecord() - called\n");
    // Buffers to store data, window for windowing the data
    float samples[WINDOW_SIZE];
    float lowPassedSamples[WINDOW_SIZE];
    //~ float complexSamples[WINDOW_SIZE * 2];
    float window[WINDOW_SIZE];
    
    // FFTW3 input and output array definitions, initialisation
    fftwf_complex*   inp;
    fftwf_complex*   outp;
    fftwf_plan       plan;   // Contains all data needed for computing FFT
    
    inp = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * WINDOW_SIZE);
    outp = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * WINDOW_SIZE);
    plan = fftwf_plan_dft_1d(WINDOW_SIZE, inp, outp, FFTW_FORWARD, FFTW_ESTIMATE);
    
    
    

    // Initialise PortAudio stream
    PaError err = Pa_Initialize();
    checkError(err);

    // List all audio I/O devices found
    int numDevices = Pa_GetDeviceCount();
    printf("Number of devices: %d\n", numDevices);

    if (numDevices < 0)
    {
        printf("Error getting the device count\n");
        exit(-1);
    }
    else if (numDevices == 0)
    {
        printf("No available audio devices detected!\n");
        exit(0);
    }

    // Devices found - display info
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

    // Use default input device
    //int inpDevice = Pa_GetDefaultInputDevice();
    
    // Use device 8 (seems to be my microphone)
    int inpDevice = 8;
    if (inpDevice == paNoDevice)
    {
        printf("No default input device.\n");
        exit(0);
    }

    // Configure input params for PortAudio stream
    configureInParams(inpDevice, &inputParams);
    
    // Prepare window
    setUpHannWindow(window, WINDOW_SIZE);

    // Open PortAudio stream
    printf("Opening stream\n");
    PaStream* pStream;
    err = Pa_OpenStream
    (
        &pStream,
        &inputParams,
        NULL, // Output parameters - not outputting data, so set to null
        SAMPLE_RATE,
        WINDOW_SIZE,
        paClipOff, // Not outputting out of range samples, don't clip?
        NULL, // No callback function, so null
        NULL // No user data here, is processed instead below, so null
    );
    checkError(err);

    printf("Starting stream\n");
    err = Pa_StartStream(pStream);
    checkError(err);

    printf("Recording...\n");
    int times = 10;
    
    float peakFrequency = 0.0f;
    float avgFrequency = 0.0f;
    int count = 0;
 
    
    while (times) // To be replaced with until 'stop' button pressed
    {
        // Read samples from microphone.
        err = Pa_ReadStream(pStream, samples, WINDOW_SIZE);
        checkError(err);
        
        /*Low-pass the data
        * -----------------
        * Remove unwanted/higher frequencies or noise from the sample
        * collected from the microphone.
        * 
        * For now, limit the range to two octaves from C3-C5, so a frequency
        * range of 130.8 Hz - 523.25 Hz
        */
        //lowPassData(samples, lowPassedSamples, WINDOW_SIZE, MAX_FREQUENCY);
        
        /*Apply windowing function (Hann)
        * -------------------------------
        * Reduces spectral leakage.
        * The FFT expects a finite, periodic signal with an integer number of 
        * periods to analyse.
        * 
        * Realistically this may not be the case on the segment of data analysed, 
        * as the data is segmented by WINDOW_SIZE and may not be cut off evenly.
        * This is how spectral leakage occurs.
        * 
        * The waveform we get likely won't be periodic and will be a non-continuous 
        * signal, so to circumvent this we apply a windowing function 
        * to reduce the amplitude of the discontinuities in the waveform.
        */
        //setWindow(window, lowPassedSamples, WINDOW_SIZE);
        setWindow(window, samples, WINDOW_SIZE);
        
        // Convert to FFTW3 complex array
        //convertToComplexArray(lowPassedSamples, inp, WINDOW_SIZE);
        convertToComplexArray(samples, inp, WINDOW_SIZE);
        
        // Carry out the FFT
        fftwf_execute(plan);
        
        // Find peaks
        getPeak(outp, peakFrequency, WINDOW_SIZE, &avgFrequency, &count);

        times--;
    }

    err = Pa_StopStream(pStream);
    checkError(err);

    err = Pa_CloseStream(pStream);
    checkError(err);
    
    printf("Stream closed.\n");
    
    printf("\navgFrequency = %f, count = %d\n", avgFrequency, count);
    
    // Average out collected frequencies (assuming only one pitch played for now)
    avgFrequency /= count;
    
    printf("\n-------------------\n");
    printf("\nOverall pitch:\n");
    printf("\n-------------------\n");
    getPitch(&avgFrequency);

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
