include ../Makefile.common

ifeq ($(OS),windows)
RUBY = ../ruby/windows/bin/ruby
endif

test: small

all: small medium large

small: selftest patp4_1x1_small pitt4_2x1_dpp_small

medium: patp10_1x1

large: patp100_1x1_medium

split: patp4_1x1_small_split2 patp10_1x1_split4

# These rules require copying or linking the definition file, but not the source images

selftest:
	$(RUBY) ./ct.rb --selftest

patp4_1x1_small carnival4_2x2_listed_small patp4_1x1_small_split2: 
	$(call MKDIR_P,testresults/$@.tmc)
	$(call CP,$@.tmc,testresults/$@.tmc/definition.tmc)
	$(RUBY) ./ct.rb testresults/$@.tmc testresults/$@.timemachine

# Same as above, but larger job with more concurrency

patp10_1x1 patp100_1x1_medium patp10_1x1_split4:
	$(call MKDIR_P,testresults/$@.tmc)
	$(call CP,$@.tmc,testresults/$@.tmc/definition.tmc)
	$(RUBY) ./ct.rb -j 8 testresults/$@.tmc testresults/$@.timemachine

# These rules require copying/linking the definition file and also the source images

pitt4_2x1_dpp_small carnival4_2x2_small:
	$(call MKDIR_P,testresults/$@.tmc)
	$(call CP,$@.tmc,testresults/$@.tmc/definition.tmc)
	$(call CP,g10.response,testresults/$@.tmc/g10.response)
	$(call RM_R,testresults/$@.tmc/0100-unstitched)
	$(call CP_R,../datasets/$@,testresults/$@.tmc/0100-unstitched)
	$(RUBY) ./ct.rb testresults/$@.tmc testresults/$@.timemachine

clean:
	$(call RM_R,testresults)

ifeq ($(OS),windows)
show:
	cmd /c start testresults.html
else
show:
	open testresults.html
endif
