build: tilestacktool/tilestacktool

tilestacktool/tilestacktool:
	make -C tilestacktool build

clean:
	make -C tilestacktool clean
	rm -rf testresults

build-and-test-tilestacktool:
	make -C tilestacktool

test: test-tilestacktool test-local

test-tilestacktool:
	make -C tilestacktool test

test-local: test-patp4s

test-patp4s: tilestacktool/tilestacktool
	mkdir -p testresults/patp4s.timemachinedefinition
	ln -sf ../../patp4s.timemachinedefinition testresults/patp4s.timemachinedefinition/definition.timemachinedefinition
	./ct.rb --tilestacktool tilestacktool/tilestacktool testresults/patp4s.timemachinedefinition --create testresults/patp4s.timemachine

