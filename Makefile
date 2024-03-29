CC = gcc
# DFLAG = -D__VERBOSE__

all:
	mkdir -p tce
	mkdir -p bin
	mkdir -p mem
	$(CC) src/tcasm.c -std=c17 -Wall -O2 -o bin/tcasm $(DFLAG)
	$(CC) src/tcsim.c -std=c17 -Wall -O2 -o bin/tcsim $(DFLAG)

clean:
	rm -rf bin/*
	rm -rf tce/*
	rm -rf mem/*

examples:
	bin/tcasm asm/sanity.asm tce/sanity.tce
	bin/tcasm asm/addup.asm tce/addup.tce
	bin/tcasm asm/alu.asm tce/alu.tce
	bin/tcasm asm/count.asm tce/count.tce
	bin/tcasm asm/collatz.asm tce/collatz.tce
