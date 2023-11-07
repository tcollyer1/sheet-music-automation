EXEC = p

CLIB = -lportaudio -lrt -pthread -lm -lasound

$(EXEC): main.c
	gcc -o $@ $^ $(CLIB)

install-pa:
	sudo apt-get install libasound-dev
	mkdir -p lib

	sudo curl https://files.portaudio.com/archives/pa_stable_v190700_20210406.tgz -o "pa.tgz"
	tar -xvzf ./pa.tgz -C lib
	cd lib/portaudio && ./configure && $(MAKE)
.PHONY: install-pa

uninstall-pa:
	cd lib/portaudio && $(MAKE) uninstall
	rm -rf lib/portaudio
.PHONY: uninstall-pa

clean:
	rm -f $(EXEC)
.PHONY: clean
