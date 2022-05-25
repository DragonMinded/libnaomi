all: pyenv libnaomi libnaomimessage libnaomisprite libnaomisramfs examples tests

${NAOMI_BASE}/tools/pyenv:
	mkdir -p ${NAOMI_BASE}/tools
	cd ${NAOMI_BASE}/tools && python3 -m venv pyenv
	${NAOMI_BASE}/tools/pyenv/bin/python3 -m pip install wheel pillow naomiutils netdimmutils

.PHONY: pyenv
pyenv: ${NAOMI_BASE}/tools/pyenv

.PHONY: libnaomi
libnaomi:
	$(MAKE) -C libnaomi

.PHONY: libnaomimessage
libnaomimessage:
	$(MAKE) -C libnaomi/message

.PHONY: libnaomisprite
libnaomisprite:
	$(MAKE) -C libnaomi/sprite

.PHONY: libnaomisramfs
libnaomisramfs:
	$(MAKE) -C libnaomi/sramfs

.PHONY: examples
examples: libnaomi libnaomimessage libnaomisprite libnaomisramfs
	$(MAKE) -C examples

.PHONY: tests
tests: libnaomi libnaomimessage libnaomisprite libnaomisramfs
	$(MAKE) -C tests

.PHONY: copy
copy: libnaomi libnaomimessage libnaomisprite libnaomisramfs examples
	$(MAKE) -C examples copy

.PHONY: wipe-install-venv
wipe-install-venv:
	mkdir -p ${NAOMI_BASE}/tools
	cd ${NAOMI_BASE}/tools && rm -rf pyenv
	cd ${NAOMI_BASE}/tools && python3 -m venv pyenv
	${NAOMI_BASE}/tools/pyenv/bin/python3 -m pip install wheel pillow naomiutils netdimmutils

.PHONY: install
install: libnaomi libnaomimessage libnaomisprite libnaomisramfs wipe-install-venv
	$(MAKE) -C libnaomi install
	$(MAKE) -C libnaomi/message install
	$(MAKE) -C libnaomi/sprite install
	$(MAKE) -C libnaomi/sramfs install
	mkdir -p ${NAOMI_BASE}/tools
	cp naomi.ld ${NAOMI_BASE}/tools
	cp aica.ld ${NAOMI_BASE}/tools
	cp Makefile.external.base ${NAOMI_BASE}/tools/Makefile.base
	cp Makefile.shared ${NAOMI_BASE}/tools/Makefile.shared
	cp tools/*.py tools/gdbserver tools/peekpoke tools/stdioredirect ${NAOMI_BASE}/tools

.PHONY: clean
clean:
	$(MAKE) -C libnaomi clean
	$(MAKE) -C libnaomi/message clean
	$(MAKE) -C libnaomi/sprite clean
	$(MAKE) -C libnaomi/sramfs clean
	$(MAKE) -C examples clean
	$(MAKE) -C tests clean
