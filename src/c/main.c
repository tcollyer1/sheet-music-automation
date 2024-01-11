#include <string.h>
#include <stdio.h>
#include <math.h> // M_PI, sqrt, sin, cos
#include <pthread.h>

#include "../h/main.h"

#define REAL 0
#define IMAG 1

#define SAMPLE_RATE         22050//44100
#define WINDOW_SIZE         4096
#define CHANNELS            1       // Mono input
#define MAX_FREQUENCY       1109//555     // For now, limit the range to three piano octaves from  
                                    // C3-C6, (but cap at C#6) so a frequency range of 
                                    // 130.8 Hz - 1108.73 Hz
#define MIN_FREQUENCY       130
#define BIN_SIZE            ((float)SAMPLE_RATE / (float)WINDOW_SIZE)

#define NOISE_FLOOR         0.1f    // Ensure the amplitude is at least this value
                                    // to help cancel out quieter noise


static int      running     = 0;
static int      processing  = 0;

GtkWidget*      recBtn      = NULL;

pthread_t       procTask;

pthread_mutex_t mutex;
pthread_mutex_t procLock;

// For now, manually establish notes and corresponding
// frequencies
char* notes[37] = 
{ 
    "C3", "C#3", "D3", "D#3", "E3", "F3", "F#3", "G3", "G#3", "A3", "Bb3", "B3",
    "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4", "Bb4", "B4",
    "C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5", "A5", "Bb5", "B5",
    "C6"
};

// Add C#6 purely for upper bound checking in case the note detected is a C6
float frequencies[38] = 
{
    // C    // C#   // D    // D#   // E    // F    // F#   // G    // G#   // A    // Bb   // B
    130.81,  138.59, 146.83, 155.56, 164.81, 174.61, 185.00, 196.00, 207.65, 220.00, 233.08, 246.94,
    261.63,  277.18, 293.66, 311.13, 329.63, 349.23, 369.99, 392.00, 415.30, 440.00, 466.16, 493.88,
    523.25,  554.37, 587.33, 622.25, 659.25, 698.46, 739.99, 783.99, 830.61, 880.00, 932.33, 987.77,
    1046.50, 1108.73
};

void toggleRecording(GtkWidget* widget, gpointer data)
{
    int threadResult = 0;
    int threadResult2 = 0;
    
    
    if (running)
    {
        printf("\n*** Stopping recording thread... ***\n");
        
        pthread_mutex_lock(&mutex);
        running = 0;
        pthread_mutex_unlock(&mutex);
    
        // Should ideally wait for processing to be 0, then cancel the thread,
        // but this causes “Trace/breakpoint trap” error
        // Only thing omitting this does is not kill the thread
        
        /*threadResult = pthread_cancel(procTask);
        
        if (threadResult != 0)
        {
            g_error("\nERROR: Failed to stop thread!\n");
        }*/
        
        gtk_button_set_label(GTK_BUTTON(recBtn), "Record");
        
    }
    
    else
    {
        printf("\n*** Starting recording thread... ***\n");
        
        pthread_mutex_lock(&mutex);
        running = 1;
        pthread_mutex_unlock(&mutex);
        
        pthread_mutex_lock(&procLock);
        processing = 1;
        pthread_mutex_unlock(&procLock);
    
        threadResult2 = pthread_create(&procTask, NULL, record, NULL);
        
        if (threadResult2 != 0)
        {
            g_error("\nERROR: Failed to start thread\n");
        }
        
        gtk_button_set_label(GTK_BUTTON(recBtn), "Stop");
    }
}

void checkError(PaError err)
{
    if (err != paNoError)
    {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        //exit(-1); // Aborts program when user stops recording, unsure why as this if condition is never met?
    }
}

// Downsample the data and get the harmonic product spectrum output
void harmonicProductSpectrum(fftwf_complex* result, float* outResult, int length)
{
    int outLength2 = getArrayLen(length, 2);
    int outLength3 = getArrayLen(length, 3);
    int outLength4 = getArrayLen(length, 4);
    int outLength5 = getArrayLen(length, 5);
    
    // Downsample - compress spectrum 4x, by 2, by 3, by 4 and by 5
    float hps2[outLength2];
    float hps3[outLength3];
    float hps4[outLength4];
    float hps5[outLength5];
    
    downsample(result, length, hps2, outLength2, 2);
    downsample(result, length, hps3, outLength3, 3);
    downsample(result, length, hps4, outLength4, 4);
    downsample(result, length, hps5, outLength5, 5);
    
    for (int i = 0; i < outLength5; i++)
    {
        outResult[i] = sqrt(calcMagnitude(result[i][REAL], result[i][IMAG]) * calcMagnitude(hps2[i], 0.0f) * calcMagnitude(hps3[i], 0.0f) * calcMagnitude(hps4[i], 0.0f) * calcMagnitude(hps5[i], 0.0f));
    }
}

// Gets the array length for downsampled data depending on the
// downsampling value (idx)
int getArrayLen(int fftLen, int idx)
{
    int outLen = (int)ceil((float)((float)fftLen / idx));
    return (outLen);
}

void downsample(const fftwf_complex* result, int length, float* out, int outLength, int idx)
{
    for (int i = 0; i < outLength; i++)
    {
        out[i] = result[i * idx][REAL];
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

// Prints the (estimated) pitch of a note based on a frequency.
void getPitch(float* freq)
{
    static char* lastPitch = "N/A";
    static int len = 0;
    static int silenceLen = 0;
    
    char* pitch = NULL;
    int found = 0;
    
    float lastFreq = 0.0f;
    
    if (*freq < MAX_FREQUENCY)
    {
        // 37 notes in range C3-C6
        for (int i = 0; i < 37; i++)
        {
            if (i != 0)
            {
                lastFreq = frequencies[i-1];
            }
            
            if ((*freq) > lastFreq && (*freq) < frequencies[i + 1])
            {
                if (abs(frequencies[i] - (*freq)) < abs(frequencies[i + 1] - (*freq)))
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
            printf("NOTE DETECTED: %s\n", pitch);
            
            silenceLen = 0;
            
            // Get if note is continuing from last iteration
            if (strcmp(pitch, lastPitch) == 0)
            {
                //printf("(Same note)\n");
                len++;
            }
            else
            {
                printf("(NEW note)\n");
                lastPitch = pitch;
                len = 1;
            }
            
            //printf("Current length: %d\n", len);
        }
        // Assume background noise (so no note)
        else
        {
            //printf("[!] NO NOTE\n");
            *freq = 0.0f;
            
            silenceLen++;
            
            //printf("SILENCE: %d\n", silenceLen);            
        }
    }
    else
    {
        printf("[!] PITCH OUT OF RANGE\n");
        *freq = 0.0f;
    }    
}

void hps_getPeak(float* dsResult, int len)
{
    float highest = 0.0f;
    float current = 0.0f;
    int peakBinNo = 0;
    
    float peakFreq = 0.0f;
    
    float last = 0.0f;          // Last highest magnitude
    int lastPeakBin = 0;        // Bin number of last highest magnitude
    float otherPeakFreq = 0.0f; // Frequency of the bin at the last highest magnitude
    
    float threshold = 20.0f;    // +/- value for checking if an identified peak is
                                // around half in Hz of another peak (octave errors)
    
    
    for (int i = 0; i < len; i++)
    {
        current = dsResult[i];
        
        if (current > highest && i * BIN_SIZE > MIN_FREQUENCY && current >= NOISE_FLOOR)
        {
            //last = highest;
            //lastPeakBin = peakBinNo;
            
            highest = current;
            peakBinNo = i;
        }
    }
    
    //peakFreq = peakBinNo * BIN_SIZE;
    //otherPeakFreq = lastPeakBin * BIN_SIZE;
    
    // ------------------------------------------------------
    // In progress - to aid with octave errors
    /*if (otherPeakFreq < (peakFreq / 2) + threshold 
    && otherPeakFreq > (peakFreq / 2) - threshold
    && fabs(highest - last) <= 0.33f)
    {
        printf("\nPeak could also be %f", otherPeakFreq);
    }*/
    // Also try this
    // ------------------------------------------------------    
    /*int actualMax = 1;
    int maxFreq = peakBinNo * 3 / 4; // Search up to 3/4 of the identified peak's bins (?)

    for (int i = 2; i < maxFreq; i++)
    {
        if (dsResult[i] > dsResult[actualMax])
        {
            actualMax = i;
        }
    }

    if (abs(actualMax * 2 - peakBinNo) < 4)
    {
        if (dsResult[actualMax] / dsResult[peakBinNo] > 0.2f && actualMax * BIN_SIZE > MIN_FREQUENCY && dsResult[actualMax] >= NOISE_FLOOR)
        {
            //peakBinNo = actualMax;
            printf("\n[!] NEW PEAK BIN SET\n");
        }
    }*/
    // ------------------------------------------------------
    
    peakFreq = peakBinNo * BIN_SIZE;
    
    // Interpolate results if note detected
    if (peakFreq != 0.0f)
    {
        //printf("Amplitude: %f", highest);
        
        // Get 2 surrounding frequencies to the 
        // peak and interpolate
        float frequencies[2];

        // Ensure peak is not at the edges of the window (should not occur anyway)
        if (peakBinNo != 0 && peakBinNo != len - 1)
        {
            int n = peakBinNo;

            frequencies[0] = (n - 1) * BIN_SIZE;
            frequencies[1] = (n + 1) * BIN_SIZE;
        }
    
        peakFreq = interpolate(frequencies[0], frequencies[1]);
    }
    
    // ------------------------------------------------------

    if (peakFreq != 0.0f)
    {
        printf("\nPeak frequency obtained: %f\n", peakFreq);
    }
    
    // Estimate the pitch based on the highest frequency reported
    getPitch(&peakFreq);
}

// Interpolate 3 values to get a better peak estimate (won't have a huge impact - but good enough for now)
float interpolate(float first, float last)
{
    //float result = 0.5f * (last - first) / (2 * second - first - last);
    
    float result = first + 0.66f * (last - first);

    return (result);
}

// Gets the peak magnitude from the computed FFT output
void getPeak(fftwf_complex* result, int fftLen, float* avgFreq, int* count)
{
    float highest = 0.0f;
    float current = 0.0f;
    int peakBinNo = 0;
    
    float peakFreq = 0.0f;
    
    for (int i = 0; i < fftLen; i++)
    {
        current = calcMagnitude(result[i][REAL], result[i][IMAG]);
        
        if (current > highest && i * BIN_SIZE > MIN_FREQUENCY)
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

// Simple first order low pass filter with a cutoff frequency
void lowPassData(float* input, float* output, int length, int cutoff)
{
    // Filter constant
    float rc = 1.0 / (cutoff * 2 * M_PI);    
    
    float dt = 1.0 / SAMPLE_RATE;
    
    // Filter coefficient (alpha) - between 0 and 1, where 0 is no smoothing, 1 is maximum.
    // Determines amount of smoothing to be applied (currently around 0.06...)
    float alpha = dt / (rc + dt);
    
    output[0] = input[0];
    
    for (int i = 1; i < length; i++)
    {
        //output[i] = output[i-1] + (alpha * (input[i] - output[i-1]));
        output[i] = alpha * input[i] + (1 - alpha) * output[i - 1];
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

/*void initRecording(GtkWidget* widget, gpointer data)
{
    printf("\ninitRecording() - called\n");
    running = 1;

    pthread_create(&procTask, NULL, record, NULL);
    
    printf("\nThread created\n");
}*/

// Main function for processing microphone data.
void* record(void* args)
//void record(GtkWidget* widget, gpointer data)
{
    //gtk_label_set_text(label, "Recording...");

    // Update GUI with label update?
    //while (gtk_events_pending())
    //{
    //    gtk_main_iteration();
    //}

    // Buffers to store data, window for windowing the data
    float samples[WINDOW_SIZE];
    float lowPassedSamples[WINDOW_SIZE];
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
        //return NULL;
        exit(-1);
    }
    else if (numDevices == 0)
    {
        printf("No available audio devices detected!\n");
        //return NULL;
        exit(-1);
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
    int inpDevice = Pa_GetDefaultInputDevice();
    
    // Use device 8 (seems to be my microphone)
    //int inpDevice = 8;
    if (inpDevice == paNoDevice)
    {
        printf("No default input device.\n");
        //return NULL;
        exit(-1);
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
    
    //processing = 1;

    printf("--- Recording... ---\n");
    
    int dsSize = 0;
    
    //int temp = 10;

    // Main processing loop
    //while (temp)
    while (running)
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
        lowPassData(samples, lowPassedSamples, WINDOW_SIZE, MAX_FREQUENCY);
        
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
        setWindow(window, lowPassedSamples, WINDOW_SIZE);
        //setWindow(window, samples, WINDOW_SIZE);
        
        // Convert to FFTW3 complex array
        convertToComplexArray(lowPassedSamples, inp, WINDOW_SIZE);
        //convertToComplexArray(samples, inp, WINDOW_SIZE);
        
        // Carry out the FFT
        fftwf_execute(plan);
        
        // Get new array size for downsampled data - 5 harmonics considered
        dsSize = getArrayLen(WINDOW_SIZE, 5);
        float dsResult[dsSize];
        
        // Get HPS
        harmonicProductSpectrum(outp, dsResult, WINDOW_SIZE);
        
        // Find peaks
        //getPeak(outp, WINDOW_SIZE, &avgFrequency, &count);
        hps_getPeak(dsResult, dsSize);
        
        //temp--;
    }
    
    printf("Sample collection stopped.\n");

    err = Pa_StopStream(pStream);
    checkError(err);

    err = Pa_CloseStream(pStream);
    checkError(err);
    
    printf("Stream closed.\n");

    // --------------
    
    // Pa_Terminate() causes a segmentation fault when called on a separate thread...

    // Terminate PortAudio stream
    /*err = Pa_Terminate();
    printf("\nStream terminated.\n");
    checkError(err);*/
    
    fftwf_free(inp);
    fftwf_free(outp);
    pStream = NULL;
    
    printf("\nMemory freed.\n");
    
    pthread_mutex_lock(&procLock);
    processing = 0; // Indicate to main (GTK) thread that processing has now stopped
    pthread_mutex_unlock(&procLock);
}

void activate(GtkApplication* app, gpointer data)
{
    GtkWidget* pWindow          = gtk_application_window_new(app);
    GtkWidget* pRandomButton    = gtk_button_new_with_label("Placeholder");
    
    recBtn = gtk_button_new_with_label("Record");

    // Create grid for button display
    GtkWidget* pGrid = gtk_grid_new();

    // Set the window position & default size
    gtk_window_set_position(GTK_WINDOW(pWindow), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(pWindow), 1000, 800);
    gtk_window_set_title(GTK_WINDOW(pWindow), "Sheet Music Automation");
    gtk_window_set_resizable(GTK_WINDOW(pWindow), FALSE); // Non-resizable for now

    gtk_grid_set_column_spacing(GTK_GRID(pGrid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(pGrid), 16);
    gtk_grid_set_column_homogeneous(GTK_GRID(pGrid), TRUE); // Expands to full width of window

    // Add grid to the created window
    gtk_container_add(GTK_CONTAINER(pWindow), pGrid);

    // Attach buttons to grid
    gtk_grid_attach(GTK_GRID(pGrid), recBtn, 1, 1, 1, 1);
    gtk_grid_attach_next_to(GTK_GRID(pGrid), pRandomButton, recBtn, GTK_POS_RIGHT, 1, 1);

    // Connect click events to callback functions
    g_signal_connect(recBtn, "clicked", G_CALLBACK(toggleRecording), NULL);

    // Make the X button close the window correctly & end program
    g_signal_connect(pWindow, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Show window
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
