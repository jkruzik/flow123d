# 
# Copyright (C) 2007 Technical University of Liberec.  All rights reserved.
#
# Please make a following refer to Flow123d on your project site if you use the program for any purpose,
# especially for academic research:
# Flow123d, Research Centre: Advanced Remedial Technologies, Technical University of Liberec, Czech Republic
#
# This program is free software; you can redistribute it and/or modify it under the terms
# of the GNU General Public License version 3 as published by the Free Software Foundation.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with this program; if not,
# write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 021110-1307, USA.
#
# $Id$
# $Revision$
# $LastChangedBy$
# $LastChangedDate$
#
# This makefile just provide main rules for: build, documentation and testing
# Build itself takes place in ../<branch>-build
#

# following depends on git_post_checkout_hook
# every target using var. BUILD_DIR has to depend on 'update-build-tree'
BUILD_DIR=$(shell cd -P ./build_tree && pwd)
SOURCE_DIR=$(shell pwd)

# reference manual directory
DOC_DIR=$(SOURCE_DIR)/doc/reference_manual


.PHONY : all
all:  install-hooks build-flow123d 

# this is prerequisite for every target using BUILD_DIR variable
update-build-tree:
	@-bin/git_post_checkout_hook	# do not print command, ignore return code


build-flow123d: update-build-tree cmake
	@cd $(BUILD_DIR) && $(MAKE) bin/flow123d


# This target only configure the build process.
# Useful for building unit tests without actually build whole program.
.PHONY : cmake
cmake:  update-build-tree
	@if [ ! -d "$(BUILD_DIR)" ]; then mkdir -p $(BUILD_DIR); fi
	@cd $(BUILD_DIR); cmake "$(SOURCE_DIR)"

	
# add post-checkout hook
install-hooks:
	if [ ! -e .git/hooks/post-checkout ];\
	then cp bin/git_post_checkout_hook .git/hooks/post-checkout;\
	fi	
		

# Save config.cmake from working dir to the build dir.
save-config: update-build-tree
	cp -f $(SOURCE_DIR)/config.cmake $(BUILD_DIR)
	
# Restore config.cmake from build dir, possibly overwrite the current one.	
load-config: update-build-tree
	cp -f $(BUILD_DIR)/config.cmake $(SOURCE_DIR)

	
# Remove all generated files
.PHONY: clean
clean: update-build-tree cmake
	make -C $(BUILD_DIR) clean

# Remove all links in source and whole build tree
.PHONY: clean-all
clean-all: update-build-tree
	# remove all symlinks in the source tree
	rm -f `find . -type l` 
	rm -rf $(BUILD_DIR)

# Make all tests
.PHONY: test-all
test-all: build-flow123d
	make -C tests test-all

# Make only certain test (e.g. "make 01.tst" will make first test)
%.tst : build-flow123d
	make -C tests $*.tst

# Clean test results
.PHONY: clean-tests
clean-tests:
	make -C tests clean


# Create doxygen documentation; use makefile generated by cmake
.PHONY: doxy-doc
doxy-doc: cmake update-build-tree
	make -C $(BUILD_DIR)/doc doxy-doc

# Create user manual using LaTeX sources and generated input reference; use makefile generated by cmake
# It does not generate new input reference file.
.PHONY: ref-doc
ref-doc: cmake update-build-tree
	make -C $(BUILD_DIR)/doc/reference_manual pdf


############################################################################################
#Input file generation.

# creates the file that defines additional information 
# for input description generated by flow123d to Latex format
# this file contains one replace rule per line in format
# \add_doc{<tag>}<replace>
# 
# this replace file is applied to input_reference.tex produced by flow123d
#

# call flow123d and make file flow_version.tex
$(DOC_DIR)/flow_version.tex: update-build-tree build-flow123d
	$(BUILD_DIR)/bin/flow123d --version | grep "This is Flow123d" | head -n1 | cut -d" " -f4-5 \
	  > $(DOC_DIR)/flow_version.tex

# call flow123d and make raw input_reference file
$(DOC_DIR)/input_reference_raw.tex: update-build-tree build-flow123d	 	
	$(BUILD_DIR)/bin/flow123d --latex_doc | grep -v "DBG" | \
	sed 's/->/$$\\rightarrow$$/g' > $(DOC_DIR)/input_reference_raw.tex

# make empty file with replace rules if we do not have one
$(DOC_DIR)/add_to_ref_doc.txt: 
	touch $(DOC_DIR)/add_to_ref_doc.txt
	
# update file with replace rules according to acctual patterns appearing in input_refecence		
update_add_doc: $(DOC_DIR)/input_reference_raw.tex $(DOC_DIR)/add_to_ref_doc.txt
	cat $(DOC_DIR)/input_reference_raw.tex \
	| grep 'AddDoc' |sed 's/^.*\(\\AddDoc{[^}]*}\).*/\1/' \
	> $(DOC_DIR)/add_to_ref_doc.list
	$(DOC_DIR)/add_doc_replace.sh $(DOC_DIR)/add_to_ref_doc.txt $(DOC_DIR)/add_to_ref_doc.list	
	
# make final input_reference.tex, applying replace rules
inputref: $(DOC_DIR)/flow_version.tex $(DOC_DIR)/input_reference_raw.tex update_add_doc
	$(DOC_DIR)/add_doc_replace.sh $(DOC_DIR)/add_to_ref_doc.txt $(DOC_DIR)/input_reference_raw.tex $(DOC_DIR)/input_reference.tex	
	
	
################################################################################################
# Help Target
help:
	@echo "The following are some of the valid targets for this Makefile:"
	@echo "... all (builds the whole library)"
	@echo "... cmake (configures the build process, useful for running unit_tests without building the whole library)"
	@echo "... test-all (runs all tests)"
	@echo "... %.test (runs selected test, e.g. 01.test)"
	@echo "... clean (removes generated files in build directory)"
	@echo "... clean-all (removes the whole build directory)"
	@echo "... clean-tests (cleans all generated files in tests, including results)"
	@echo "... doxy-doc (creates html documentation of the source using Doxygen)"
	@echo "... ref-doc (creates reference manual using LaTeX sources and generated input reference file)"
	@echo "... inputref (generates input reference file)"
#	@echo "packages:"
#	@echo "... linux-pack"
.PHONY : help

