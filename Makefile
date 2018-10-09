CSRC=dwmstatusbar.c
TARGET=dwmstatusbar
DSTDIR=${HOME}/.local/bin/
ALSA_FLAGS=-lasound
X11_FLAGS=-lX11
GCC_FLAGS=-lm -Wall

compile:
	gcc ${CSRC} -o ${TARGET} ${GCC_FLAGS} ${X11_FLAGS} ${ALSA_FLAGS}

move: compile
	mv ${TARGET} ${DSTDIR}/${TARGET}

clean:
	rm -rf ${TARGET} ${DSTDIR}/${TARGET}

all: clean move compile
