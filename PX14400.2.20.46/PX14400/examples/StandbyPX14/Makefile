# Makefile for StandbyPX14

TARGET   := StandbyPX14

.PHONY : clean

$(TARGET) : StandbyPX14.cpp
	@echo "> Building example: "$(TARGET)"..."
	@g++ -Wall -g $^ -o $(TARGET) -lsig_px14400

clean :
	@echo "> Cleaning example: "$(TARGET)"..."
	@rm -f *~ *.o $(TARGET)


