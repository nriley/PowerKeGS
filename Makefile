PowerKeGS.po: PowerKeGS
	./cadius createvolume PowerKeGS.po PowerKeGS 800kb
	./cadius addfile PowerKeGS.po /PowerKeGS ./PowerKeGS
	./cadius catalog PowerKeGS.po

tnfs: PowerKeGS.po
	scp PowerKeGS.po osric:src/tnfsd/root/

transfer: PowerKeGS.po
	cp PowerKeGS.po ~/Transfer
	appswitch -i com.utmapp.UTM

PowerKeGS: make.s powerkey.s
	./Merlin32 -V ./Library make.s

clean:
	rm -f PowerKeGS _FileInformation.txt *_Output.txt *_Symbols.txt
