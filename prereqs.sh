#!/bin/bash -   
#title          :prereqs.sh
#description    :Installs everything you need for the caching project, and more
#author         :tberckma
#date           :20120907
#version        :0.1    
#usage          :./prereqs.sh
#notes          :       
#copyright      :Copyright SanDisk Corporation 2012
#============================================================================

if [ `uname` == "Darwin" ] ; then
	
	# Check if MacPorts is installed
	port version >/dev/null 2>/dev/null
	if [ $? != 0 ] ; then 
		printf "MacPorts is not installed or not configured properly\n"
		printf "Get it from http://www.macports.org\n"
		exit 1
	fi
	
	if [ `/usr/bin/id -u` != 0 ] ; then
		printf "You need to run this script as root\n"
		exit 1
	fi
	
	# Glib, used for data structures in SSARC implementation
	# To compile the app, use "gcc `pkg-config --cflags glib-2.0` foo.c"
	port install glib2
	
	# LaTeX installations
	# Needed for generating paper
	port install texlive texlive-basic texlive-bibtex-extra
	
elif [ `uname` == "Linux" ] ; then
	
	# Make sure aptitude is installed
	apt-get -v >/dev/null
	if [ $? != 0 ] ; then
		printf "You do not have aptitude installed\n"
		printf "Try installing it or use Ubuntu, where it is installed by default\n"
		exit 1
	fi


	if [ `/usr/bin/id -u` != 0 ] ; then
		printf "You need to run this script as root\n"
		exit 1
	fi

	AptGetFlags="-q -y"
	# Glib
	apt-get $AptGetFlags install libglib2.0-0 libglib2.0-dev
	
	# All LaTeX installations
	apt-get $AptGetFlags install texlive-latex-recommended

	# Asynchronous IO interfaces
	# Compile via "gcc -laio foo.c"
	apt-get $AptGetFlags install libaio1 libaio-dev
	
else
	printf "Only OSX and Linux are currently supported\n"
fi
