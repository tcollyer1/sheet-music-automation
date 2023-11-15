EXEC = p

CLIB = -lportaudio -lrt -pthread -lm -lasound `pkg-config --cflags gtk+-3.0 --libs gtk+-3.0`

$(EXEC): main.c
	gcc -o $@ $^ $(CLIB)

setup:
	sudo apt-get install libasound-dev
	mkdir -p lib

	sudo apt install libgtk-3-dev
	sudo curl https://files.portaudio.com/archives/pa_stable_v190700_20210406.tgz -o "pa.tgz"
	tar -xvzf ./pa.tgz -C lib
	cd lib/portaudio && ./configure && $(MAKE) -j
	rm -rf pa.tgz
	rm -rf lib/portaudio
	rmdir lib
.PHONY: setup

uninstall-pa:
	cd lib/portaudio && $(MAKE) uninstall
	rm -rf lib/portaudio
.PHONY: uninstall-pa

clean:
	rm -f $(EXEC)
.PHONY: clean
