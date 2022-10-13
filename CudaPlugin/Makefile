
NVCC = nvcc -c -O2 -Xcompiler -fPIC -x cu

main:
	-mkdir _build1
	cd _build1 && qmake ../CudaPlugin.pro && make -j4

build: prepare _build1/cudalib.cu.o

prepare:
	-mkdir _build1

_build1/cudalib.cu.o: src/cudalib.cu.cpp
	${NVCC} $^ -o $@

clean:
	-rm -fr _bin 2>/dev/null
	-rm -fr _build1 2>/dev/null
	-rm -fr _build2 2>/dev/null
