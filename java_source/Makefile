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

SRC=wavCreator/WavCodeGenerator.java

# Usage:
#    java -cp . controllPanel/Main_WavBootLoader
#    java -jar hex2wav.jar <hexfilename>

hex2wav.jar: $(SRC:.java=.class)
	@echo $<
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
