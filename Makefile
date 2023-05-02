.PHONY: all clean

PROJECT=ssc
SRC=main.c soulfu_script.c
INC=soulfu_script.h

all: $(PROJECT)

$(PROJECT): $(SRC) $(INC)
	gcc -o $(PROJECT) $(SRC)

clean:
	-rm -f $(PROJECT)
