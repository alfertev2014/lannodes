TARGET = lannodes


all: $(TARGET)

main.o : main.cpp
	gcc $(CFLAGS) -c -o $@ $<


$(TARGET) : main.o
	gcc $(CFLAGS) -o $@ $<


.PHONY: all clean

clean:
	rm -f *.o $(TARGET)
