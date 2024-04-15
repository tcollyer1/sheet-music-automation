# Sheet Music Automation
### How effectively can a Fourier transform-based method automatically transcribe different recorded monophonic piano melodies into sheet music?
Automatic music transcription is a lucrative field with many existing approaches available – though there is still no conclusive answer to the problem. The Fourier transform on the other hand is a renowned algorithm in computing and mathematics – specifically for its capability in the area of audio signal processing, translating signals from the time domain to the frequency domain for an analysis of their frequency components. In the interest of musicians who frequently compose or simply enjoy working out music by ear at their instrument, this research has led to the creation of a lightweight Linux-based system that will be used to measure how effectively melodic ideas could be automatically transcribed, with a focus on having the Fourier transform as a basis for gauging both pitch and rhythmic content, in conjunction with other applied techniques, and monophonic piano recordings as input material to produce a sheet music representation from raw WAV data.

This research project implements a **Linux-based** program in **C** that utilises the FFT in conjunction with the harmonic product spectrum and a complex-domain deviation onset detection function, among many other researched techniques to both help prepare the data and process it post-FFT. It is then used and tested against a set of 15 monophonic piano melodies, 5 unique melodies across 3 octaves each and at 3 different sizes of FFT, to measure how effectively the FFT as a basis is able to extract both pitch and rhythmic information for use in the field of automatic music transcription. All of the essential code and implementation is located in **src/c/main.c**. Test results can be observed from the **test_suite** folder and are named in line with their test number as per the report. Each test consists of 3 MIDI output results, one per FFT size, which can be opened directly in notation software such as MuseScore, in addition to the associated WAV recording that matches the intended notation.

---
### Libraries Used
The following libraries, which fall under the GNU General Public Licence (ISC License for MIDIlib) with the right to be redistributed and/or modified, are used within the project. Any individual files from these libraries can be found in the **src/include/** directory of this repository:
- [PortAudio](https://www.portaudio.com) ([GitHub repo](https://github.com/PortAudio/portaudio)) - real-time audio recording feature (packages installed directly to system during setup)
- [FFTW3](https://www.fftw.org) ([GitHub repo](https://github.com/FFTW/fftw3)) - FFT calculation (packages installed directly to system during setup)
- [GTK 3.0+](https://www.gtk.org) - system interface (GUI) (packages installed directly to system during setup)
- [TinyWav](https://github.com/mhroth/tinywav) by Martin Roth - WAV file writing/reading for processing audio data (tinywav.h, tinywav.c)
- [MIDIlib](https://github.com/MarquisdeGeek/midilib) by Steven Goodwin - MIDI file writing (midiinfo.h, midifile.h, midifile.c)
- [OnsetsDS](https://onsetsds.sourceforge.net) ([GitHub repo](https://github.com/danstowell/onsetsds)) by Dan Stowell - provides the implementation for the complex-domain deviation onset detection method (onsetsds.h, onsetsds.c)

---
### How to Use
1. In order to run the software, a Linux-based system is required (or at the very least Windows running WSL, though this does not support sound devices in general which negates the recording feature only).
2. Git is additionally required (install using `sudo apt-get install git-all`)
3. Clone the repository:
```
git clone https://github.com/tcollyer1/sheet-music-automation.git
```
4. Navigate into the `src/c/` directory and run the makefile setup using command `make setup`. This should install all necessary packages, including PortAudio, FFTW3 and GTK.
5. Staying in this directory, enter `make p`, followed by `./p` to build and run the software. The GUI allows you to enter relevant information such as tempo, time signature, quantisation, key, file output name and output location.
