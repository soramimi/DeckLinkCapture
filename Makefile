
all:
	cd CudaPlugin && make
	cp CudaPlugin/_bin/libcudaplugin.so _bin/

