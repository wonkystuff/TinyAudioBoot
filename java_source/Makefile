# Makefile created based on the old java_compile.sh
# Targets Java release 8 for a reasonable stab at
# backward compatibility.
#
# Change History:
#    24/02/2021  John Tuffen -- Created
#

JFLAGS = --release 8 -classpath .
JC = javac
.SUFFIXES: .java .class

CLASSES=hexTools/IntelHexFormat.java controllPanel/Main_WavBootLoader.java \
		controllPanel/Model_ProgrammParameters.java wavCreator/WavCodeGenerator.java \
		wavCreator/BootFrame.java wavCreator/HexToSignal.java waveFile/WavFile.java \
		waveFile/WriteExample.java waveFile/WavFileException.java \
		waveFile/ReadExample.java waveFile/AePlayWave.java

# Usage:
#    java -cp . controllPanel/Main_WavBootLoader
#    java -jar hex2wav.jar <hexfilename>

hex2wav.jar: $(CLASSES:.java=.class)
	jar cvfm $@ MANIFEST.MF */*.class

clean:
	rm -fr */*.class
	rm -fr *.jar

.java.class:
	@echo $@
	@echo $<
	$(JC) $(JFLAGS) $<

deploy: hex2wav.jar
	cp $< ../tools/hex2wav/
