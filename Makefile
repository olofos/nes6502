SOURCES=nes_cpu.c nsf.c nes_apu_channel.c nes_apu.c alsa_sound.c dat_file.cpp
CFLAGS=-Wall -std=c11
#-DDEBUG_INSTRUCTION_LOG -DDEBUG_STACK
CXXFLAGS=-Wall -std=c++1z
LDFLAGS=-lasound

LDFLAGS_TEST=-lcmocka


CSOURCES=$(filter %.c, $(SOURCES))
CPPSOURCES=$(filter %.cpp, $(SOURCES))
OBJECTS=$(CSOURCES:.c=.o) $(CPPSOURCES:.cpp=.o)

nsf_test: $(OBJECTS)
	g++ -o $@ $^ $(LDFLAGS)

test_nes_cpu: test_nes_cpu.o nes_cpu.o
	gcc -o $@ $^ $(LDFLAGS_TEST)

test: test_nes_cpu
	@./test_nes_cpu

%.o: %.cpp
	g++ $(CXXFLAGS) -c -o $@ $^

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $^

clean:
	rm -f $(OBJECTS) nsf_test test_nes_cpu.o test_nes_cpu
