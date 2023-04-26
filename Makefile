CC = gcc
#DFLAG = -D__VERBOSE__

all:
	$(CC) src/tcasm.c -std=c17 -Wall -O2 -o bin/tcasm $(DFLAG)
	$(CC) src/tcsim.c -std=c17 -Wall -O2 -o bin/tcsim $(DFLAG)
	mkdir -p tce
	mkdir -p bin

clean:
	rm -rf bin/*
	rm -rf tce/*

examples:
	bin/tcasm asm/sanity.asm tce/sanity.tce
	bin/tcasm asm/addup.asm tce/addup.tce
	bin/tcasm asm/alu.asm tce/alu.tce
