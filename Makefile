TARGET = lannodes
OBJS = logging.o timers.o networking.o identity.o nodes.o main.o

CFLAGS = --std=c++11 -g

LIBS = -lrt

all: $(TARGET)

# pull in dependency info for *existing* .o files
-include $(OBJS:.o=.d)


%.o : %.cpp
	gcc $(CFLAGS) -c -o $@ $<
	gcc -MM $(CFLAGS) $< > $*.d


$(TARGET) : $(OBJS)
	gcc $(CFLAGS) $^ $(LIBS) -o $@


.PHONY: all clean

clean:
	rm -f *.o *.d $(TARGET)


