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

test-local: test-patp4s test-patp10

test-patp4s: tilestacktool/tilestacktool
	mkdir -p testresults/patp4s.timemachinedefinition
	ln -sf ../../patp4s.timemachinedefinition testresults/patp4s.timemachinedefinition/definition.timemachinedefinition
	./ct.rb -l testresults/patp4s.timemachinedefinition --create testresults/patp4s.timemachine

test-patp10: tilestacktool/tilestacktool
	mkdir -p testresults/patp10.timemachinedefinition
	ln -sf ../../patp10.timemachinedefinition testresults/patp10.timemachinedefinition/definition.timemachinedefinition
	./ct.rb -l -j 8 testresults/patp10.timemachinedefinition --create testresults/patp10.timemachine

test-pitt2x2: tilestacktool/tilestacktool
	mkdir -p testresults/pitt2x2.timemachinedefinition
	ln -sf ../../pitt2x2.timemachinedefinition testresults/pitt2x2.timemachinedefinition/definition.timemachinedefinition
	./ct.rb -l testresults/pitt2x2.timemachinedefinition --create testresults/pitt2x2.timemachine

show: show-patp4s show-patp10

show-patp4s: test-patp4s
	open testresults/patp4s.timemachine/view.html

show-patp10: test-patp10
	open testresults/patp10.timemachine/view.html
