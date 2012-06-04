include ../Makefile.common

ifeq ($(OS),windows)
RUBY = ../ruby/windows/bin/ruby
endif

test: small medium

small: patp4_1x1_small pitt4_2x1_dpp_small

medium: patp10_1x1

# These rules require copying or linking the definition file, but not the source images

patp4_1x1_small carnival4_2x2_listed_small:
	$(call MKDIR_P,testresults/$@.timemachinedefinition)
	$(call CP,$@.timemachinedefinition,testresults/$@.timemachinedefinition/definition.timemachinedefinition)
	$(RUBY) ./ct.rb -l testresults/$@.timemachinedefinition --create testresults/$@.timemachine

# Same as above, but larger job with more concurrency

patp10_1x1:
	$(call MKDIR_P,testresults/$@.timemachinedefinition)
	$(call CP,$@.timemachinedefinition,testresults/$@.timemachinedefinition/definition.timemachinedefinition)
	$(RUBY) ./ct.rb -l -j 8 testresults/$@.timemachinedefinition --create testresults/$@.timemachine

# These rules require copying/linking the definition file and also the source images

pitt4_2x1_dpp_small carnival4_2x2_small:
	$(call MKDIR_P,testresults/$@.timemachinedefinition)
	$(call CP,$@.timemachinedefinition,testresults/$@.timemachinedefinition/definition.timemachinedefinition)
	$(call CP,g10.response,testresults/$@.timemachinedefinition/g10.response)
	$(call RM_R,testresults/$@.timemachinedefinition/0100-unstitched)
	$(call CP_R,../datasets/$@,testresults/$@.timemachinedefinition/0100-unstitched)
	$(RUBY) ./ct.rb -l testresults/$@.timemachinedefinition --create testresults/$@.timemachine

clean:
	$(call RM_R,testresults)

ifeq ($(OS),windows)
show:
	cmd /c start testresults.html
else
show:
	open testresults.html
endif
