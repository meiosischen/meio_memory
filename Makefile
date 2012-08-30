meio_memory : main.o meio_memory.o 
	cc -m32 -o meio_memory main.o meio_memory.o 
     
main.o : main.c 
	cc -c -m32 -g -Wall main.c

meio_memory.o : meio_memory.c interface.h define.h 
	cc -c -m32 -g -Wall meio_memory.c

clean :
	rm main.o meio_memory.o 

