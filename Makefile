CC = cl
CFLAGS = -o esp.exe
SRC = CS2-Glow/esp.cpp

all: esp.exe

esp.exe: $(SRC)
	$(CC) $(CFLAGS) $(SRC)

clean:
	del esp.exe

.PHONY: all clean
