TILESTACKTOOL_DIR=../tilestacktool
TILESTACKTOOL=$(TILESTACKTOOL_DIR)/tilestacktool

build: $(TILESTACKTOOL_DIR)/tilestacktool

$(TILESTACKTOOL):
	make -C $(TILESTACKTOOL_DIR) build

test: patp4_1x1_small

patp4_1x1_small: ../tilestacktool/tilestacktool
	mkdir -p testresults/$@.timemachinedefinition
	ln -sf ../../$@.timemachinedefinition testresults/$@.timemachinedefinition/definition.timemachinedefinition
	./ct.rb -l testresults/$@.timemachinedefinition --create testresults/$@.timemachine

test-patp10: tilestacktool/tilestacktool
	mkdir -p testresults/patp10.timemachinedefinition
	ln -sFv ../../patp10.timemachinedefinition testresults/patp10.timemachinedefinition/definition.timemachinedefinition
	./ct.rb -l -j 8 testresults/patp10.timemachinedefinition --create testresults/patp10.timemachine

pitt4_2x1_dpp_small carnival4_2x2_small: tilestacktool/tilestacktool
	mkdir -p testresults/$@.timemachinedefinition
	ln -sf ../../$@.timemachinedefinition testresults/$@.timemachinedefinition/definition.timemachinedefinition
	ln -sf ../../g10.response  testresults/$@.timemachinedefinition
	rm -f testresults/$@.timemachinedefinition/0100-unstitched
	ln -s ../../../datasets/$@ testresults/$@.timemachinedefinition/0100-unstitched
	./ct.rb -l testresults/$@.timemachinedefinition --create testresults/$@.timemachine

show: show-patp4s show-patp10

show-patp4s: test-patp4s
	open testresults/patp4s.timemachine/view.html

show-patp10: test-patp10
	open testresults/patp10.timemachine/view.html
