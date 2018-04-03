# Makefile for PciAcqPX14

TARGET   := PciAcqPX14

.PHONY : clean

$(TARGET) : PciAcqPX14.cpp
	@echo "> Building example: "$(TARGET)"..."
	@g++ -Wall -g $^ -o $(TARGET) -lsig_px14400

clean :
	@echo "> Cleaning example: "$(TARGET)"..."
	@rm -f *~ *.o $(TARGET)

