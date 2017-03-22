TARGET = lannodes


all: $(TARGET)

%.cpp : %.h

%.o : %.cpp
	gcc $(CFLAGS) -c -o $@ $<


$(TARGET) : timers.o networking.o main.o
	gcc $(CFLAGS) -o $@ $<


.PHONY: all clean

clean:
	rm -f *.o $(TARGET)
