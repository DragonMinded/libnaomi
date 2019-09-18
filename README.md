# Tools for working with a NetDimm

This repository started when I looked at triforcetools.py and realized that it was ancient (Python2) and contained a lot of dead code. So, as one of the first projects I did when setting up my NNC to netboot was to port triforcetools.py to Python3, clean up dead code and add type hints. I also added percentage display for transfer and `--help` to the command-line. This has been tested on a Naomi with a NetDimm, but has not been verified on Triforce/Chihiro. There is no reason why it should not work, however.

## Setup Requirements

This requires at least Python 3.6, and a few packages installed. To install the required packages, run the following. You may need to preface with sudo if you are installing into system Python.

```
python3 -m pip install -r requirements.txt
```

## Script Invocation

### netdimm_send

This script handles sending a single binary to a single cabinet, assuming it is on and ready for connection. Invoke the script like so to see options:

```
python3 -m scripts.netdimm_send --help
```

You can invoke it identically to the original triforcetools.py as well. Assuming your NetDimm is at 192.168.1.1, the following will load the ROM named `my_favorite_game.bin` from the current directory:

```
python3 -m scripts.netdimm_send 192.168.1.1 my_favorite_game.bin
```

### netdimm_ensure

This script will monitor a cabinet, and send a single binary to that cabinet whenever it powers on. It will run indefinitely, waiting for the cabinet to power on before sending, and then waiting again for the cabinet to be powered off and back on again before sending again. Invoke the script like so to see options:

```
python3 -m scripts.netdimm_ensure --help
```

It works identically to netdimm_send, except for it only supports a zero PIC, and it tries its best to always ensure the cabinet has the right game. Run it just like you would netdimm_send.

## Developing

The tools here are fully typed, and should be kept that way. To verify type hints, run the following:

```
mypy --strict .
```

The tools are also lint clean (save for line length lints which are useless drivel). To verify lint, run the following:

```
flake8 --ignore E501 .
```
