#include <string.h>
#include <stdio.h>
#include <math.h>       // M_PI, sqrt, sin, cos
#include <pthread.h>

#include "../include/main.h"
#include "../include/onsetsds.h"
#include "../include/tinywav.h"
#include "../include/midifile.h"

#define REAL 0
#define IMAG 1

#define SAMPLE_RATE         22050
#define WINDOW_SIZE         2048//4096
#define CHANNELS            1       // Mono input
#define MAX_FREQUENCY       1109    // For now, limit the range to three piano octaves from  
                                    // C3-C6, (but cap at C#6) so a frequency range of 
                                    // 130.8 Hz - 1108.73 Hz
#define MIN_FREQUENCY       130
#define BIN_SIZE            ((float)SAMPLE_RATE / (float)WINDOW_SIZE)

#define NOISE_FLOOR         0.05f//0.1f    // Ensure the amplitude is at least this value
                                    // to help cancel out quieter noise
                                    
#define MAX_NOTES           1000    // Maximum size of the buffer to contain note data
                                    // for writing to the MIDI file to save on memory
                                    
#define MEDIAN_SPAN         11//88      // Amount of previous frames to account for, for
                                    // onset detection. Around 11 is recommended for
                                    // a size 512 FFT; so around 88 for size 4096?
                                    
#define NUM_HARMONICS       5       // Number of harmonics for the harmonic product spectrum
                                    // to consider


static int      running     = 0;
static int      processing  = 0;
static int      firstRun    = 0;

GtkWidget*      recBtn      = NULL;

pthread_t       procTask;

pthread_mutex_t runLock;
pthread_mutex_t procLock;

// Buffers to store the output data to be translated into MIDI notes
char        recPitches[MAX_NOTES][4];
int         recLengths[MAX_NOTES];
int         recMidiPitches[MAX_NOTES];
static int  totalLen = 0;
static int  bufIndex = 0;

// OnsetsDS struct - onset detection
OnsetsDS ods;

// For reading from/writing to .wav to save user recording for 
// analysis
TinyWav tw;

// For now, manually establish notes and corresponding
// frequencies
char* notes[37] = 
{ 
    "C3", "C#3", "D3", "D#3", "E3", "F3", "F#3", "G3", "G#3", "A3", "Bb3", "B3",
    "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4", "Bb4", "B4",
    "C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5", "A5", "Bb5", "B5",
    "C6"
};

// Will store all of the corresponding MIDI pitch values per note
int midiNotes[37];

// Add C#6 purely for upper bound checking in case the note detected is a C6
float frequencies[38] = 
{
    // C    // C#   // D    // D#   // E    // F    // F#   // G    // G#   // A    // Bb   // B
    130.81,  138.59, 146.83, 155.56, 164.81, 174.61, 185.00, 196.00, 207.65, 220.00, 233.08, 246.94,
    261.63,  277.18, 293.66, 311.13, 329.63, 349.23, 369.99, 392.00, 415.30, 440.00, 466.16, 493.88,
    523.25,  554.37, 587.33, 622.25, 659.25, 698.46, 739.99, 783.99, 830.61, 880.00, 932.33, 987.77,
    1046.50, 1108.73
};

// Responsible for starting/stopping recording upon clicking the GUI button
void toggleRecording(GtkWidget* widget, gpointer data)
{
    int threadResult = 0;
    int threadResult2 = 0;
    
    
    if (running)
    {
        printf("\n*** Stopping recording thread... ***\n");
        
        pthread_mutex_lock(&runLock);
        running = 0;
        pthread_mutex_unlock(&runLock);
    
        // Should ideally wait for processing to be 0, then cancel the thread,
        // but this causes “Trace/breakpoint trap” error
        // Only thing omitting this does is not kill the thread
        
        /*threadResult = pthread_cancel(procTask);
        
        if (threadResult != 0)
        {
            g_error("\nERROR: Failed to stop thread!\n");
        }*/
        
        gtk_button_set_label(GTK_BUTTON(recBtn), "Record");
        
        totalLen = 0;
        bufIndex = 0;
    }
    
    else
    {
        firstRun = 1;
        
        printf("\n*** Starting recording thread... ***\n");
        
        pthread_mutex_lock(&runLock);
        running = 1;
        pthread_mutex_unlock(&runLock);
        
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

// Functions for appending to output pitch/length buffers
void pitchesAdd(char* pitch, int length, int midiNote)
{    
    strcpy(recPitches[bufIndex], pitch);
    recLengths[bufIndex] = length;
    //printf("\n[!] Adding %s (%d) of length %d to position %d\n", pitch, midiNote, length, bufIndex);
    recMidiPitches[bufIndex] = midiNote;
    
    bufIndex++; // Make sure to increase buffer index for next value
    totalLen = bufIndex;
    
    // Need to handle this properly:
    // a) Immediately stop running the recording loop here
    // b) Change button display back to "Record"
    // c) Carry out MIDI translation on buffer content as normal
    if (totalLen == MAX_NOTES)
    {
        printf("\n[!] STOPPING: Buffer full!\n");
        running = 0;
    }
}

void displayBufferContent()
{
    printf("\n--- CONTENT OF OUTPUT BUFFERS: ---");
    
    for (int i = 0; i < totalLen; i++)
    {
        printf("\nNOTE: %s | LENGTH: %d", recPitches[i], recLengths[i]);
    }
    
    printf("\n");
}

// Assign MIDI note values per pitch C3-C6
void setMidiNotes()
{
    // C3 is 36 in the MIDI library we're using.
    // However other sources suggest C3 should be 48, 
    // which is what C4 is set to.
    // The MIDI output is also an octave lower than expected.
    int c3 = MIDI_NOTE_C4;
    
    int midiVal = c3;
    
    printf("\n=== MIDI TABLE ===\n");
    
    // 37 notes between C3-C6
    for (int i = 0; i < 37; i++)
    {
        midiNotes[i] = midiVal;
        
        printf("\n%s is %d", notes[i], midiNotes[i]);
        
        midiVal++;
    }
    
    printf("\n===            ===\n");
}

// Get the note type for a given note duration.
int getNoteType(float noteDur, float qNoteLen)
{    
    // qNoteLen represents the length in seconds a quarter note
    // (crotchet) is expected to be. We calculate this in
    // outputMidi() below
    
    int noteType = 0;
    
    if (noteDur == 0.0f)
    {
        // Round to next smallest note (for now, QUAVER)
        noteDur = 0.5f;
    }
    
    // Semidemiquaver
    if (noteDur == qNoteLen * 0.125f)
    {
        printf("as a SEMIDEMIQUAVER\n====\n");
        noteType = MIDI_NOTE_SEMIDEMIQUAVER;
    }
    
    // Semiquavers
    else if (noteDur == qNoteLen * 0.25f)
    {
        printf("as a SEMIQUAVER\n====\n");
        noteType = MIDI_NOTE_SEMIQUAVER;
    }
    else if (noteDur == qNoteLen * 0.375f)
    {
        printf("as a DOTTED SEMIQUAVER\n====\n");
        noteType = MIDI_NOTE_DOTTED_SEMIQUAVER;
    }
    
    // Quavers
    else if (noteDur == qNoteLen * 0.5f)
    {
        printf("as a QUAVER\n====\n");
        noteType = MIDI_NOTE_QUAVER;
    }
    else if (noteDur == qNoteLen * 0.75f)
    {
        printf("as a DOTTED QUAVER\n====\n");
        noteType = MIDI_NOTE_DOTTED_QUAVER;
    }
    
    // Crotchets
    else if (noteDur == qNoteLen)
    {
        printf("as a CROTCHET\n====\n");
        noteType = MIDI_NOTE_CROCHET;
    }
    else if (noteDur == qNoteLen * 1.5f)
    {
        printf("as a DOTTED CROTCHET\n====\n");
        noteType = MIDI_NOTE_DOTTED_CROCHET;
    }
    
    // Minims
    else if (noteDur == qNoteLen * 2.0f)
    {
        printf("as a MINIM\n====\n");
        noteType = MIDI_NOTE_MINIM;
    }
    else if (noteDur == qNoteLen * 3.0f)
    {
        printf("as a DOTTED MINIM\n====\n");
        noteType = MIDI_NOTE_DOTTED_MINIM;
    }
    
    else
    {
        noteType = getNoteType(noteDur - 0.25f, qNoteLen);
    }
    
    return (noteType);
}

// Write the contents of the buffers to a MIDI file.
void outputMidi(float frameTime)
{    
    // For now, set tempo here
    int tempo = 60;
    
    int track = 1;
    
    // Try to create MIDI file
    MIDI_FILE* midiOutput = midiFileCreate("output.mid", TRUE); // (True for overwrite file if exists)
    
    if (midiOutput)
    {
        // Assign tempo of 60bpm for now.
        // Starts at track 1. May need to start it from
        // track 0 instead
        midiSongAddTempo(midiOutput, track, tempo);
        
        // Set key to C major for now.
        midiSongAddKeySig(midiOutput, track, keyCMaj);
        
        // Set current channel before writing data (only using one)
        midiFileSetTracksDefaultChannel(midiOutput, track, MIDI_CHANNEL_1);
        
        // Set instrument. Not really essential for its end purpose
        midiTrackAddProgramChange(midiOutput, track, MIDI_PATCH_ELECTRIC_GRAND_PIANO);
        
        // Simple 4/4 time for now
        midiSongAddSimpleTimeSig(midiOutput, track, 4, MIDI_NOTE_CROCHET);
        
        // TEST
        /*midiTrackAddNote(midiOutput, track, 48, MIDI_NOTE_CROCHET, MIDI_VOL_HALF, TRUE, FALSE);
        midiTrackAddNote(midiOutput, track, 50, MIDI_NOTE_QUAVER, MIDI_VOL_HALF, TRUE, FALSE);
        midiTrackAddRest(midiOutput, track, MIDI_NOTE_QUAVER, FALSE);
        midiTrackAddNote(midiOutput, track, 52, MIDI_NOTE_QUAVER, MIDI_VOL_HALF, TRUE, FALSE);
        midiTrackAddNote(midiOutput, track, 53, MIDI_NOTE_QUAVER, MIDI_VOL_HALF, TRUE, FALSE);
        midiTrackAddNote(midiOutput, track, 55, MIDI_NOTE_CROCHET, MIDI_VOL_HALF, TRUE, FALSE);
        */
    
        // Get the length of time one beat (crotchet) will account for given
        // the tempo
        float beatsPerSec = (float)tempo / 60;
        
        // Fastest note to detect is quavers, FOR NOW.
        float minimumNoteDur = beatsPerSec / 2;
        
        // Iterate through our buffers to write out the data
        for (int i = 0; i < totalLen; i++)
        {
            // Get note length by multiplying the duration of the frame
            // by the number of frames the note persists for, then rounding
            // this to (for now) the nearest half-note (quaver).
            float noteLen = (float)round((frameTime * recLengths[i]) / minimumNoteDur) * minimumNoteDur;
            
            // If not silence
            if (strcmp(recPitches[i], "N/A") != 0)
            {
                printf("\n====\nWriting %s (MIDI PITCH %d)\n", recPitches[i], recMidiPitches[i]);
                
                midiTrackAddNote(midiOutput, track, recMidiPitches[i], getNoteType(noteLen, beatsPerSec), MIDI_VOL_HALF, TRUE, FALSE);
            }
            else
            {
                // Not invalid length of silence
                if (recLengths[i] > 1)
                {
                    printf("\n====\nWriting a REST\n");
                    
                    midiTrackAddRest(midiOutput, track, getNoteType(noteLen, beatsPerSec), FALSE);
                }
            }
        }
        
        midiFileClose(midiOutput);
        
        printf("\nMIDI file output.mid successfully created.\n");
    }
    else
    {
        // If MIDi file creation fails
        printf("\n[!] ERROR: Failed to create MIDI file.\n");
    }
}

// Displays the text of a PortAudio error
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
    
    // Downsample - compress spectrum 4x --> by 2, by 3, by 4 and by 5
    float hps2[outLength2];
    float hps3[outLength3];
    float hps4[outLength4];
    float hps5[outLength5];
    
    downsample(result, hps2, outLength2, 2);
    downsample(result, hps3, outLength3, 3);
    downsample(result, hps4, outLength4, 4);
    downsample(result, hps5, outLength5, 5);
    
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

void downsample(const fftwf_complex* result, float* out, int outLength, int idx)
{
    for (int i = 0; i < outLength; i++)
    {
        out[i] = result[i * idx][REAL];
    }
}

/* FFTW3 complex array types are in the form
*
* typedef float fftwf_complex[2];
*
* where 0 = real, 1 = imaginary
*/
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
char* getPitch(float freq, int* midiNote)
{    
    char* pitch = NULL;
    int found = 0;
    
    float lastFreq = 0.0f;
    
    if (freq < MAX_FREQUENCY)
    {
        // 37 notes in range C3-C6
        for (int i = 0; i < 37; i++)
        {
            if (i != 0)
            {
                lastFreq = frequencies[i-1];
            }
            
            if (freq > lastFreq && freq < frequencies[i + 1])
            {
                if (abs(frequencies[i] - freq) < abs(frequencies[i + 1] - freq))
                {
                    pitch = notes[i];
                    (*midiNote) = midiNotes[i];
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
            printf("\nNOTE DETECTED: %s (MIDI %d)", pitch, (*midiNote));
        }
        
        // Otherwise assume background noise (so no note)
    }
    else
    {
        printf("[!] PITCH OUT OF RANGE\n");
    }
    
    return (pitch);
}

// Obtain the peak from the downsampled harmonic product spectrum
// output.
void hps_getPeak(float* dsResult, int len, bool isOnset)
{
    float highest = 0.0f;
    float current = 0.0f;
    int peakBinNo = 0;
    
    float peakFreq = 0.0f;
    
    static float prevAmplitude = 0.0f;  // Last recorded amplitude
    static int noteLen = 0;             // Length of current note (number of iterations the "same note"
                                        // has been tracked)
    static int silenceLen = 0;          // Length of silence (indicate rests)
    static char prevPitch[4];
    static int lastPeakBin = 0;
    char* curPitch;
    static int prevMidiNote = 0;
    int curMidiNote = 0;
    
    int newNote = 0;
    int wasSilence = 0;
    int lastNoteLen = 0;
    
    float threshold = 0.3f;
    
    static int tempNoteBuf[20];
    static int tempNoteCount[20];
    
    // Reset static values if a new recording
    if (firstRun)
    {
        prevAmplitude = 0.0f;
        noteLen = 0;

        silenceLen = 0;
        lastPeakBin = 0;
        prevMidiNote = 0;
        
        firstRun = 0;
    }
    
    
    for (int i = 0; i < len; i++)
    {
        current = dsResult[i];
        
        if (current > highest && i * BIN_SIZE > MIN_FREQUENCY && current >= NOISE_FLOOR)
        {           
            highest = current;
            peakBinNo = i;
        }
    }
    
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
        
        //printf("\nPeak frequency obtained: %f\n", peakFreq);
    }
    
    // Estimate the pitch based on the highest frequency reported
    curPitch = getPitch(peakFreq, &curMidiNote);
    
    // If note detected - 
    if (peakFreq != 0.0f)
    {
        if (silenceLen != 0)
        {
            wasSilence = 1; // Flag that there was silence
        }     
        
        //printf("Amplitude: %f", highest);
        
        /*If the onset detection function has flagged an onset in this window
        * or this is the first note the user has played during the recording
        * (no previous amplitude set), then we say this is a new note.
        * 
        * When a new note is detected, the previous detected note's identified
        * pitch and length value are buffered. The current new note may continue
        * into the next window, at which point we say this is the SAME note.
        * 
        * This is where the onset detection function has not flagged a different
        * onset, therefore it's implied that this note has continued.
        * 
        * As long as the "same" note is detected, its length value increases to
        * indicate how many successive windows in which it continues, and is stored
        * upon the next new note being tracked.
        * 
        * This "length" value can then be used to calculate the actual note length.
        */
        
        //if (isOnset || wasSilence || prevAmplitude == 0.0f || strcmp(prevPitch, curPitch) != 0)
        if (isOnset || prevAmplitude == 0.0f)
        {
            printf(" | (NEW note)"); // New note attack
            lastNoteLen = noteLen;
            //printf(" | saved lastNoteLen as %d", lastNoteLen);
            noteLen = 1; // Reset note length
            
            newNote = 1; // Flag new note
            
            //printf(" | LEN: %d\n", noteLen);
        }
        else
        {         
            if (noteLen < 20)
            {
                tempNoteBuf[noteLen] = curMidiNote;
                tempNoteCount[noteLen] = -1;
            }
               
            // This is likely a continuation of the same note being played, so continue to increase
            // the length
            //printf(" | (SAME note)"); // Note decay
            noteLen++;
            
            //printf(" | LEN: %d\n", noteLen);
        }        
        
        lastPeakBin = peakBinNo;
        
        prevAmplitude = highest;        
    }
    // Implies recording has just started - don't record silence until first note played
    // to prevent lots of rests at the start
    else if (prevAmplitude == 0.0f)
    {
        // Do nothing
    }
    // Else silence detected (rest)
    else
    {
        silenceLen++;
        lastNoteLen = noteLen;
        //printf("\n[SILENCE]");
    }
    
    // ------------------------------------------------------
    
    // If first note of the recording
    if (newNote && lastNoteLen == 0)
    {
        strcpy(prevPitch, curPitch);
        prevMidiNote = curMidiNote;
        
        tempNoteBuf[0] = prevMidiNote;
        tempNoteCount[0] = -1;
    }
    // If a new note after a period of silence
    else if (newNote && wasSilence)
    {
        pitchesAdd(prevPitch, silenceLen, prevMidiNote);

        silenceLen = 0;

        strcpy(prevPitch, curPitch);
        prevMidiNote = curMidiNote;
    }
    // If a new note (not the first) after another note, add the last note vals to buffers
    else if (newNote)
    {
        // Where the detected frequency can sometimes fluctuate,
        // get the most common detected note
        for (int i = 0; i < lastNoteLen; i++)
        {
            int count = 1;
            
            for (int j = i + 1; j < lastNoteLen; j++)
            {
                if (tempNoteBuf[i] == tempNoteBuf[j])
                {
                    count++;
                    tempNoteCount[j] = 0;
                }
            }
            
            if (tempNoteCount[i] != 0)
            {
                tempNoteCount[i] = count;
            }
        }
        
        int highest = 0;
        int highestIdx = 0;
        
        for (int i = 0; i < lastNoteLen; i++)
        {
            if (tempNoteCount[i] > highest)
            {
                highest = tempNoteCount[i];
                highestIdx = i;
            }
        }
        
        int realMidiNote = tempNoteBuf[highestIdx];
        
        //pitchesAdd(prevPitch, lastNoteLen, prevMidiNote);
        pitchesAdd(prevPitch, lastNoteLen, realMidiNote);
        
        strcpy(prevPitch, curPitch);
        
        tempNoteBuf[0] = prevMidiNote;
        tempNoteCount[0] = -1;
    }   
    // If we're starting a point of silence (rests), store the last pitch
    else if (silenceLen == 1)
    {
        // Where the detected frequency can sometimes fluctuate,
        // get the most common detected note
        for (int i = 0; i < lastNoteLen; i++)
        {
            int count = 1;
            
            for (int j = i + 1; j < lastNoteLen; j++)
            {
                if (tempNoteBuf[i] == tempNoteBuf[j])
                {
                    count++;
                    tempNoteCount[j] = 0;
                }
            }
            
            if (tempNoteCount[i] != 0)
            {
                tempNoteCount[i] = count;
            }
        }
        
        int highest = 0;
        int highestIdx = 0;
        
        for (int i = 0; i < lastNoteLen; i++)
        {
            if (tempNoteCount[i] > highest)
            {
                highest = tempNoteCount[i];
                highestIdx = i;
            }
        }
        
        int realMidiNote = tempNoteBuf[highestIdx];
        
        //pitchesAdd(prevPitch, lastNoteLen, prevMidiNote);
        pitchesAdd(prevPitch, lastNoteLen, realMidiNote);

        strcpy(prevPitch, "N/A");
        prevMidiNote = 0;
    }
}

// Interpolate 3 values to get a better peak estimate (won't have a huge impact - but good enough for now)
float interpolate(float first, float last)
{
    //float result = 0.5f * (last - first) / (2 * second - first - last);
    
    float result = first + 0.66f * (last - first);

    return (result);
}

// Gets the peak magnitude from the computed FFT output
/*void getPeak(fftwf_complex* result, int fftLen, float* avgFreq, int* count)
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
}*/

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

// Save 50% of the samples from previous run for the next run
// for 50% window overlap
void saveOverlappedSamples(const float* samples, float* overlap, int len)
{
    for (int i = 0; i < len / 2; i++)
    {
        overlap[i] = samples[len/2 + i];
    }
}

// Overlap the windows at 50%
void overlapWindow(const float* samples, const float* overlap, float* newSamples, int len)
{
    for (int i = 0; i < len / 2; i++)
    {
        //newSamples[i] = overlap[i];
        newSamples[i] = (float)(overlap[i] + samples[i]) / 2.0f;
    }
    
    for (int i = len / 2; i < len; i++)
    {
        //newSamples[i] = samples[i - len/2];
        newSamples[i] = samples[i];
    }
}

// Main function for processing microphone data.
void* record(void* args)
{
    bool firstRun = true;
    
    // Buffer to store audio samples
    float samples[WINDOW_SIZE];
    
    // Buffer to store samples to be overlapped with the next
    // buffer of samples
    float overlap[WINDOW_SIZE/2];
    
    // Buffer to store samples with overlap applied
    float newSamples[WINDOW_SIZE];
    
    float lowPassedSamples[WINDOW_SIZE];
    float window[WINDOW_SIZE];
    
    // FFTW3 input and output array definitions, initialisation
    fftwf_complex*   inp;
    fftwf_complex*   outp;
    fftwf_plan       plan;   // Contains all data needed for computing FFT
    
    inp = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * WINDOW_SIZE);
    outp = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * WINDOW_SIZE);
    plan = fftwf_plan_dft_1d(WINDOW_SIZE, inp, outp, FFTW_FORWARD, FFTW_ESTIMATE);
    
    // Allocate memory for ODS - onset detection
    float* odsData = (float*)malloc(onsetsds_memneeded(ODS_ODF_RCOMPLEX, WINDOW_SIZE, MEDIAN_SPAN));
    onsetsds_init(&ods, odsData, ODS_FFT_FFTW3_HC, ODS_ODF_RCOMPLEX, WINDOW_SIZE, MEDIAN_SPAN, SAMPLE_RATE);
    

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
    
    if (inpDevice == paNoDevice)
    {
        printf("No default input device.\n");
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

    printf("--- Recording... ---\n");
    
    int     dsSize  = 0;        // Size of buffer after downsampling (for HPS)
    bool    onset   = false;    // Onset flag

    // --- Main processing loop ---
    
    // Open .wav file to write to
    tinywav_open_write(&tw,
                        CHANNELS,
                        SAMPLE_RATE,
                        TW_FLOAT32,
                        TW_INLINE,  
                        "recording.wav");
    
    int numFrames = 0;  // Number of times samples are collected
                        // (total number of frames processed)
    
    while (running)
    {
        // Read samples from microphone.
        err = Pa_ReadStream(pStream, samples, WINDOW_SIZE);
        checkError(err);
        
        // Write samples to a .wav for FFT/other processing afterwards
        tinywav_write_f(&tw, samples, WINDOW_SIZE);
        
        numFrames++;
    }
    
    tinywav_close_write(&tw);
    
    printf("\nSample collection stopped.\n");

    err = Pa_StopStream(pStream);
    checkError(err);

    err = Pa_CloseStream(pStream);
    checkError(err);
    
    printf("Stream closed.\n");

    /*********************************************************
     * 
     * PROCESSING THE AUDIO DATA
     * -------------------------
     * 
     * 1.  Read in the .wav file the user just recorded.
     * 2.  Acquire set of FP samples - overlapping by 50%.
     *     This reduces data loss from windowing (step 4).
     * 3.  Low pass the data to help filter out higher 
     *     frequencies.
     * 4.  Apply a Hann window to the data. This helps to
     *     reduce spectral leakage.
     * 5.  Format the data into complex numbers for the FFT.
     * 6.  Carry out the FFT to acquire frequency data.
     * 7.  Downsample and apply harmonic product spectrum for
     *     a better fundamental frequency estimate.
     * 8.  Calculate any onsets (from raw FFT output)
     * 9.  Calculate fundamental pitch from downsampled HPS
     *     output magnitudes, by looking for the peak, and
     *     relating the frequency at this peak to a pitch.
     * 10. Repeat for as long as the recording continues.
     * 11. Combine to collect pitches and note lengths that
     *     can then be processed into note on/off signals to
     *     create the MIDI file, which can then be used by
     *     the end user.
     *    
     ********************************************************/
     
    // This gives us the total number of samples in our .wav
    int totalSamples = numFrames * WINDOW_SIZE;
     
    // Duration of the recording is equal to the total number of
    // samples, divided by the sample rate
    float timeSecs = (float)totalSamples / (float)SAMPLE_RATE;
    
    // Amount of time each frame accounts for
    float frameTime = timeSecs / numFrames;
    
    // Read from .wav file and carry out all processing
    tinywav_open_read(&tw, "recording.wav", TW_SPLIT);

    // Loop through all of the samples, frame by frame
    for (int i = 0; i < numFrames; i++)
    {
        // Set up pointers to samples, separated by channels
        // (only one in our case however)
        float* samplePtrs[CHANNELS];
        
        for (int j = 0; j < CHANNELS; ++j)
        {
            samplePtrs[j] = samples + j * WINDOW_SIZE;
        }
        
        tinywav_read_f(&tw, samplePtrs, WINDOW_SIZE);
        
        if (firstRun)
        {
            // If first run - simply use the first set of samples
            memcpy(newSamples, samples, sizeof(newSamples));
            firstRun = false;
        }
        else
        {            
            /*Overlap the window
            * ------------------
            * Use a 50% overlap by taking the latter half of the samples
            * from the previous window, and averaging it with the first 
            * half of the new window of samples continuously each FFT cycle
            * to reduce potential data loss brought about by windowing.
            */
            overlapWindow(samples, overlap, newSamples, WINDOW_SIZE);
        }
        
        // Save the second half of the samples to be used in the next FFT
        // cycle for overlapping
        saveOverlappedSamples(samples, overlap, WINDOW_SIZE);
        
        /*Low-pass the data
        * -----------------
        * Remove unwanted/higher frequencies or noise from the sample
        * collected from the microphone.
        * 
        * For now, limit the range to three octaves from C3-C6, so a frequency
        * range of 130.8 Hz - 1108.73 Hz
        */
        //lowPassData(samples, lowPassedSamples, WINDOW_SIZE, MAX_FREQUENCY);
        lowPassData(newSamples, lowPassedSamples, WINDOW_SIZE, MAX_FREQUENCY);
        
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
        * to reduce the amplitude of the discontinuities in the waveform (the edges).
        */
        setWindow(window, lowPassedSamples, WINDOW_SIZE);
        
        // Convert to FFTW3 complex array
        convertToComplexArray(lowPassedSamples, inp, WINDOW_SIZE);
        
        // Carry out the FFT
        fftwf_execute(plan);
        
        // Get new array size for downsampled data - 5 harmonics considered
        dsSize = getArrayLen(WINDOW_SIZE, 5);
        float dsResult[dsSize];
        
        // Carry out onset detection from FFT output, using a complex-domain deviation
        // onset detection function
        onset = onsetsds_process(&ods, outp);
        
        // Get HPS
        harmonicProductSpectrum(outp, dsResult, WINDOW_SIZE);
        
        // Find peaks
        hps_getPeak(dsResult, dsSize, onset);
    }
    
    tinywav_close_read(&tw);
    
    
    // Pa_Terminate() causes a segmentation fault when called on a separate thread...

    // Terminate PortAudio stream
    /*err = Pa_Terminate();
    printf("\nStream terminated.\n");
    checkError(err);*/
    
    // Outputs everything in the buffer
    displayBufferContent();
    printf("\n(Each frame takes %f secs)\n", frameTime);
    
    // In progress: output to MIDI file
    outputMidi(frameTime);
    
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

    // Text boxes & labels
    GtkWidget* time             = gtk_entry_new();
    GtkWidget* tempo            = gtk_entry_new();
    GtkWidget* key              = gtk_entry_new();
    GtkWidget* fileLoc          = gtk_entry_new();

    GtkWidget* timeLbl          = gtk_label_new("Time signature (N/N): ");
    GtkWidget* tempoLbl         = gtk_label_new("Tempo (BPM): ");
    GtkWidget* keyLbl           = gtk_label_new("Key signature: ");
    GtkWidget* fileLocLbl       = gtk_label_new("File location: ");

    gtk_entry_set_max_length(GTK_ENTRY(time), 0);
    gtk_entry_set_max_length(GTK_ENTRY(tempo), 0);
    gtk_entry_set_max_length(GTK_ENTRY(key), 0);
    gtk_entry_set_max_length(GTK_ENTRY(fileLoc), 0);

    gtk_label_set_xalign(GTK_LABEL(timeLbl), 1.0);
    gtk_label_set_xalign(GTK_LABEL(tempoLbl), 1.0);
    gtk_label_set_xalign(GTK_LABEL(keyLbl), 1.0);
    gtk_label_set_xalign(GTK_LABEL(fileLocLbl), 1.0);
    
    setMidiNotes();
    
    recBtn = gtk_button_new_with_label("Record");

    // Create containing box & grid for layout
    GtkWidget* pBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget* pGrid = gtk_grid_new();

    // Add grid to box with 20px padding from the edge
    gtk_box_pack_start(GTK_BOX(pBox), pGrid, TRUE, TRUE, 16);

    // Set the window position & default size
    gtk_window_set_position(GTK_WINDOW(pWindow), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(pWindow), 1000, 800);
    gtk_window_set_title(GTK_WINDOW(pWindow), "Sheet Music Automation");
    gtk_window_set_resizable(GTK_WINDOW(pWindow), FALSE); // Non-resizable for now

    gtk_grid_set_column_spacing(GTK_GRID(pGrid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(pGrid), 16);
    gtk_grid_set_column_homogeneous(GTK_GRID(pGrid), TRUE); // Expands to full width of window

    gtk_grid_set_row_spacing(GTK_GRID(pGrid), 50);

    // Add grid to the created window
    gtk_container_add(GTK_CONTAINER(pWindow), pBox);
    gtk_container_add(GTK_CONTAINER(pWindow), pGrid);

    // Attach textboxes, labels and button to grid
    gtk_grid_attach(GTK_GRID(pGrid), timeLbl, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(pGrid), tempoLbl, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(pGrid), keyLbl, 1, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(pGrid), fileLocLbl, 1, 4, 1, 1);

    gtk_grid_attach(GTK_GRID(pGrid), time, 3, 1, 1, 1);    
    gtk_grid_attach(GTK_GRID(pGrid), tempo, 3, 2, 1, 1);    
    gtk_grid_attach(GTK_GRID(pGrid), key, 3, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(pGrid), fileLoc, 3, 4, 1, 1);

    gtk_grid_attach(GTK_GRID(pGrid), recBtn, 2, 5, 1, 1);

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
