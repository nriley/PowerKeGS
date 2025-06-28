PowerKeGS: make.s powerkey.s
	./Merlin32 -V ./Library make.s

clean:
	rm -f PowerKeGS _FileInformation.txt *_Output.txt *_Symbols.txt
