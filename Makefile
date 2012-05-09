all: build-and-test-tilestacktool

clean:
	make -C tilestacktool clean

build-and-test-tilestacktool:
	make -C tilestacktool
