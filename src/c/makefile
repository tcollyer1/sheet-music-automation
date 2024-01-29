EXEC = p

CLIB = -lportaudio -lrt -pthread -lasound `pkg-config --cflags gtk+-3.0 --libs gtk+-3.0` -lfftw3f -lm

$(EXEC): ../include/onsetsds.c ../include/tinywav.c ../include/midifile.c main.c
	gcc -o $@ $^ $(CLIB)

setup:
	sudo apt-get install libasound-dev
	mkdir -p lib

	sudo apt install libgtk-3-dev
	sudo curl https://files.portaudio.com/archives/pa_stable_v190700_20210406.tgz -o "pa.tgz"
	tar -xvzf ./pa.tgz -C lib
	cd lib/portaudio && ./configure && $(MAKE) -j
	
	sudo curl https://www.fftw.org/fftw-3.3.10.tar.gz -o "fftw3.gz"
	tar -xvzf ./fftw3.gz -C lib
	cd lib/fftw-3.3.10 && ./configure --enable-threads --enable-float --enable-openmp && sudo make && sudo make install
	rm -rf fftw3.gz
	
	rm -rf pa.tgz
.PHONY: setup

uninstall-pa:
	cd lib/portaudio && $(MAKE) uninstall
	rm -rf lib/portaudio
.PHONY: uninstall-pa

clean:
	rm -f $(EXEC)
.PHONY: clean
