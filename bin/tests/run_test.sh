#!/bin/bash
# 
# Copyright (C) 2007 Technical University of Liberec.  All rights reserved.
#
# Please make a following refer to Flow123d on your project site if you use the program for any purpose,
# especially for academic research:
# Flow123d, Research Center: Advanced Remedial Technologies, Technical University of Liberec, Czech Republic
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
# Author(s): Jiri Hnidek <jiri.hnidek@tul.cz>
#


#
# Note: This script assumes that Flow123d can contain any error. It means that
# there could be never ending loop, flow could try to allocate infinity
# amount of memory, etc.
#


# Every test has to be finished in 60 seconds. Flow123d will be killed after
# 60 seconds. It prevents test to run in never ending loop, when development
# version of Flow123d contains such error.
TIMEOUT=120

# Try to use MPI environment variable for timeout too. Some implementation
# of MPI supports it and some implementations doesn't.
export MPIEXEC_TIMEOUT=${TIMEOUT}

# Relative path to Flow123d binary from the directory,
# where this script is placed
FLOW123D="../flow123d"
# Relative path to Flow123d binary from current/working directory
FLOW123D="${0%/*}/${FLOW123D}"

# Relative path to mpiexec from the directory, where this script is placed
MPIEXEC="../mpiexec"
# Relative path to mpiexec binary from current/working directory
MPIEXEC="${0%/*}/${MPIEXEC}"

# Relative path to ndiff checking correctness of output files from this directory
NDIFF="../ndiff/ndiff.pl"
# Relative path to ndiff binary from current/working directory
NDIFF="${0%/*}/${NDIFF}"

# Directory that flow123d usually uses for output files
OUTPUT_DIR="./output"

# Flow output is redirected to the file. This output is printed to the output
# only in situation, when Flow123d return error (not zero) exit status
FLOW123D_OUTPUT="${OUTPUT_DIR}/flow_stdout_stderr.log"

# Ndiff output is redirected to the file. This output is printed to the output
# only in situation, when output file isn't "same" as reference output file
NDIFF_OUTPUT="./ndiff_stdout_stderr.log"

# Directory containing reference output files
REF_OUTPUT_DIR="./ref_output"

# Directory ./output containing output files of one test will be copied to the
# following directory. Each sub-directory will be named according .ini file and
# number of processes
TEST_RESULTS="./test_results"


# Variable with exit status. Possible values:
# 0 - no error, all tests were finished correctly
# 1 - some important file (flow123d, ini file) doesn't exist or permission
#     are not granted
# 2 - flow123d was not finished correctly
# 3 - execution of flow123d wasn't finished in time
EXIT_STATUS=0

# Set up memory limits that prevent too allocate too much memory.
# The limit of virtual memory is 200MB (memory, that could be allocated)
ulimit -S -v 200000


# First parameter has to be list of ini files; eg: "flow.ini flow_vtk.ini"
INI_FILES="$1"

# Second parameter has to be number of processors to run on; eg: "1 2 3 4 5"
N_PROC="$2"

# The last parameter could contain additional flow parametres
FLOW_PARAMS="$3"


# Following function is used for checking output files
function check_output {
	INI_FILE="${1}"
	NP="${2}"


	# Remove all results of preview test
	rm -rf ${TEST_RESULTS}/${INI_FILE}.${NP}

	# Try to create new directory for results
	mkdir -p ${TEST_RESULTS}/${INI_FILE}.${NP} > /dev/null 2>&1

	# Was directory created successfully?
	if [ $? -eq 0 -a -d ${TEST_RESULTS}/${INI_FILE}.${NP} ]
	then
		# Move content of ./output directory to the directory containing
		# results of tests
		cp -R ${OUTPUT_DIR}/* ${TEST_RESULTS}/${INI_FILE}.${NP}/
		if [ $? -eq 0 ]
		then
                        # make comparison wtih reference output
                        
                        # Does this test contain directory for this .ini file?
                        if [ ! -d "${REF_OUTPUT_DIR}/${INI_FILE}" ]
                        then
                                echo " [Failed]"
                                echo "Error: directory with reference output files doesn't exist: ${REF_OUTPUT_DIR}/${INI_FILE}"
                                return 1
                        fi
  
			for file in `ls "${REF_OUTPUT_DIR}/${INI_FILE}"/`
			do
				# Does needed output file exist?
				if [ -f "${TEST_RESULTS}/${INI_FILE}.${NP}/${file}" ]
				then
					# Compare output file using ndiff
					${NDIFF} \
						"${REF_OUTPUT_DIR}/${INI_FILE}/${file}" \
						"${TEST_RESULTS}/${INI_FILE}.${NP}/${file}" \
						> "${TEST_RESULTS}/${INI_FILE}.${NP}/${NDIFF_OUTPUT}" 2>&1
					# Check result of ndiff
					if [ $? -eq 0 ]
					then
						echo -n "."
					else
						echo " [Failed]"
						echo "Error: file ${TEST_RESULTS}/${INI_FILE}.${NP}/${file} is too different."
						return 1
					fi
				else
					echo " [Failed]"
					echo "Error: file ${TEST_RESULTS}/${INI_FILE}.${NP}/${file} doesn't exist"
					return 1
				fi
			done
		else
			echo " [Failed]"
			echo "Error: can't copy files to: ${TEST_RESULTS}/${INI_FILE}.${NP}"
			rm -rf "${TEST_RESULTS}/${INI_FILE}.${NP}"
			return 1
		fi
	else
		echo " [Failed]"
		echo "Error: can't create directory: ${TEST_RESULTS}/${INI_FILE}.${NP}"
		echo ${PWD}
		return 1
	fi
}


# Check if Flow123d exists and it is executable file
if ! [ -x "${FLOW123D}" ]
then
	echo "Error: can't execute ${FLOW123D}"
	exit 1
fi

# Check if mpiexec exists and it is executable file
if ! [ -x "${MPIEXEC}" ]
then
	echo "Error: can't execute ${MPIEXEC}"
	exit 1
fi

# For every ini file run one test
for INI_FILE in $INI_FILES
do
	# Check if it is possible to read ini file
	if ! [ -e "${INI_FILE}" -a -r "${INI_FILE}" ]
	then
		echo "Error: can't read ${INI_FILE}"
		EXIT_STATUS = 1
                # continue with next ini file
                continue 1
	fi

	for NP in ${N_PROC}
	do
		# Clear output file for every new test. Output of passed test isn't
		# important. It is useful to see the output of last test that failed.
		echo "" > "${FLOW123D_OUTPUT}"

		# Erase content of ./output directory
		rm -rf "${OUTPUT_DIR}"/*

		# Reset timer
		TIMER="0"

		# Flow123d runs with changed priority (19 is the lowest priority)
		nice --adjustment=10 "${MPIEXEC}" -np ${NP} "${FLOW123D}" -S "${INI_FILE}" ${FLOW_PARAMS} > "${FLOW123D_OUTPUT}" 2>&1 &
		PARENT_MPIEXEC_PID=$!
		# Wait for child processes
		sleep 1
		MPIEXEC_PID=`ps -o "${PARENT_MPIEXEC_PID} %P %p" | gawk '{if($1==$2){ print $3 };}'`

		echo -n "Running flow123d [proc:${NP}] ${INI_FILE} ."
		IS_RUNNING=1

		# Wait max TIMEOUT seconds, then kill mpiexec, that executed flow123d processes
		while [ ${TIMER} -lt ${TIMEOUT} ]
		do
			TIMER=`expr ${TIMER} + 1`
			echo -n "."
			#ps -o "%P %p"
			sleep 1

			# Is mpiexec and flow123d still running?
			ps | gawk '{ print $1 }' | grep -q "${PARENT_MPIEXEC_PID}"
			if [ $? -ne 0 ]
			then
				# set up, that flow123d was finished in time
				IS_RUNNING="0"
				break 1
			fi
		done

		# Was Flow123d finished during TIMEOUT or is it still running?
		if [ ${IS_RUNNING} -eq 1 ]
		then
			echo " [Failed:loop]"
			# Send SIGTERM to mpiexec. It should kill child processed
			kill -s SIGTERM ${MPIEXEC_PID} #> /dev/null 2>&1
			EXIT_STATUS=2
			# No other test will be executed
			break 2
		else
			# Get exit status variable of mpiexec executing mpiexec executing flow123d
			wait ${PARENT_MPIEXEC_PID}
			MPIEXEC_EXIT_STATUS=$?

			# Was Flow123d finished correctly?
			if [ ${MPIEXEC_EXIT_STATUS} -eq 0 ]
			then
				echo " [Success:${TIMER}s]"
				echo -n "Checking output files ."

				# Check correctness of output files
				check_output "${INI_FILE}" "${NP}"

				# Were all output files correct?
				if [ $? -eq 0 ]
				then
					echo " [Success]"
				else
					# Do not run this test for more processes, when output is
					# different for current number of processes
					break 1 
				fi
			else
				echo " [Failed:error]"
				EXIT_STATUS=1
				# continue with next ini file 
				continue 2
			fi
		fi
	done
done

# Print redirected stdout to stdout only in situation, when some error occurred
if [ $EXIT_STATUS -gt 0 -a $EXIT_STATUS -lt 10 ]
then
	echo "Error in execution: ${FLOW123D} -S ${INI_FILE} -- ${FLOW_PARAMS}"
	cat "${FLOW123D_OUTPUT}"
fi

rm -f "${FLOW123D_OUTPUT}"

exit ${EXIT_STATUS}

