all:
	gcc -lm -Wall -lX11 -lasound dwmstatusbar.c -o dwmstatusbar
	@echo "current directory: ${shell pwd} "
	@echo "PATH: ${PATH}"
