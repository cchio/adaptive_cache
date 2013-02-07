#!/bin/bash -   
#title          :generate_paper.sh
#description    :Creates a fresh copy and saves it as paper.pdf
#author         :tberckma
#date           :20120907
#version        :0.1    
#usage          :./generate_paper.sh [-w|c]
#notes          :-w = Quit on warnings (off by default)
#               :-c = Clean intermediate files only     
#copyright      :Copyright SanDisk Corporation 2012
#============================================================================

# Constants
SourceDir="Source"
PaperName="paper"
WarningCode=1
ErrorCode=2
SuccessCode=0

# Globals
StopOnWarnings=0
CleanFiles=0

check_option() {
	# Argument $1: the option
	
	if [ "-w" == $1 ] ; then
		StopOnWarnings=1
	elif [ "-c" == $1 ] ; then
		CleanFiles=1
	fi
}

latex_check_warnings() {
	# Argument $1: the pass number
	
	grep -q '^LaTeX Warning:' $PaperName.log
    if [ $? == 0 ] ; then    	
        printf " LaTeX encountered warnings in pass #$1\n"
        return $WarningCode
    fi
    
    return $SuccessCode
}

latex_run_pass() {
	# Argument $1: the pass number
	
	pdflatex $LatexArgs $PaperName >/dev/null # Creates aux file, ignore stdout
    if [ $? != 0 ] ; then 
        printf " LaTeX encountered a fatal error in pass #$1.\n"
        return $ErrorCode
    else
    	latex_check_warnings $1  # It prints a message with pass number itself
    	return $? # Pass along either success or warning code from check
    fi
}

process_tex_files() {

	
	LatexArgs="-file-line-error -interaction=batchmode" 
	QuitMsg=" Quitting. See $SourceDir/$PaperName.log for more info.\n"

	latex_run_pass 1 # Generates aux file
	if (( $? == $ErrorCode  || ( $? == $WarningCode && $StopOnWarnings == 1 ) )); then
		printf "$QuitMsg"
		return 1
	fi
    
	bibtex $PaperName >> $PaperName.log # Inserts citations into aux file
    if [ $? != 0 ] ; then 
        printf " Bibtex was not able to process your bibliography file.\n"
        printf "$QuitMsg"
        return 1
    fi
    
	latex_run_pass 2 # Resolves citation references
	if (( $? == $ErrorCode  || ( $? == $WarningCode && $StopOnWarnings == 1 ) )); then
		printf "$QuitMsg"
		return 1
	fi
    
	latex_run_pass 3 # Resolves other references
	if (( $? == $ErrorCode  || ( $? == $WarningCode && $StopOnWarnings == 1 ) )); then
		printf "$QuitMsg"
		return 1
	fi
	
	return 0
}

clean_tex_intermediates() {

	rm -f $PaperName.aux $PaperName.bbl $PaperName.blg $PaperName.log $PaperName.pdf
}

check_option $1

if [ $CleanFiles == 1 ] ; then 
	cd $SourceDir
	clean_tex_intermediates
	cd ..
else
	cd $SourceDir 
	process_tex_files
	cd ..
fi


