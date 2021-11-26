#!/bin/bash

#:	   Title:	confMerger.sh - Add or delete tr069 objects or parameters to/from tr069 configuration file
#:	Synopsis:	confMerger.sh -a|-d  originalConfFile  mergedConfFile  [resultFile]
#:	  Option:	-a - add objects or parameters given by mergedConfFile to originalConfFile
#:			-d - delete objects or parameters given by mergedConfFile from originalConfFile
#:			-h - print help message
#:
#:			[resultFile] - If [resultFile] is not given, the result is stored in the "originalConfFile".


## Script metadata
scriptname=${0##*/}
description='Add or delete tr069 objects or parameters to/from tr069 configuration file'
synopsis="$scriptname -a|-d  originalConfFile  mergedConfFile  [resultFile]"


## Common Function Definitions

#@ DESCRIPTION: print error message and exit with supplied return code
#@ USAGE: die STATUS [MESSAGE]
die()
{
	error="$1"
	shift
	[ -n "$*" ] && printf "%s: %s\n" "$scriptname" "$*" >&2
	exit "$error"
}

#@ DESCRIPTION: print usage information
#@ USAGE: usage
usage() {

cat << EOF

#:
#: Title:	confMerger.sh - Add or delete tr069 objects or parameters to/from tr069 configuration file
#: Synopsis:	confMerger.sh -a|-d  originalConfFile  mergedConfFile  [resultFile]
#: Option:	-a - add objects or parameters given by mergedConfFile to originalConfFile
#:		-d - delete objects or parameters given by mergedConfFile from originalConfFile
#:		-h - print help message
#:
#:		[resultFile] - If [resultFile] is not given, the result is stored in the "originalConfFile".


#: Example MergedConfFile>
#:	* mergedConfFile to add objects or parameters
#:		- This has the same formation with tr-069.conf
#:		- If given object does not exist in the original configuration file,
#:			all of sub elements of the object are added.
#:		- If given parameter does not exist in the original configuration file,
#:			only the parameter is added to given node.
#:		- If given parameter already exists in the original configuration file,
#:			previous parameter is replaced with given parameter.
#:
#:	-------------------------------------------------------------------------------------------------
#:	| object InternetGatewayDevice {								|
#:	|	object DeviceInfo {									|
#:	|		object newAddedDepth01 {							|
#:	|			param test1 string notify(0,0,2) readonly const("newAdded TEST1");	|
#:	|			param test2 string notify(0,0,2) readonly const("newAdded TEST2");	|
#:	|			param test3 string notify(0,0,2) readonly const("newAdded TEST3");	|
#:	|			param test4 string notify(0,0,2) readonly const("newAdded TEST4");	|
#:	|			param test5 string notify(0,0,2) readonly const("newAdded TEST5");	|
#:	|		};										|
#:	|		param ModelName string notify(0,0,2) readonly const("TEST");			|
#:	|	};											|
#:	| };												|
#:	|												|
#:	| // END: Do Not delete this line								|
#:	-------------------------------------------------------------------------------------------------
#:
#:	* mergedConfFile to delete objects or parameters
#:		- First field - Type of entity [obj|par].
#:		- Second and Third fields - Dummy data for future use.
#:		- Fourth field - The full directory name of the entity to delete
#:				The path should be placed in square brackets,
#:				and each node should be separated with whitespace.
#:	-------------------------------------------------------------------------------------------------
#:	| obj	0	0	[InternetGatewayDevice DeviceInfo newAddedDepth01]			|
#:	| par	0	0	[InternetGatewayDevice DeviceInfo ModelName]				|
#:	|												|
#:	| // END: Do Not delete this line								|
#:	-------------------------------------------------------------------------------------------------
#:
#: Notice>
#:	1. Always "deletion" is applied first and then "addition" applied
#:	2. There is no way to replace existed object directly.
#:	   In this case, existed object should be deleted first, and then add the object to be replace.
#:
EOF
}



## Parse command-line options and check validation
_option="$1"
_origConfFile="$2"
_vConfFile="$3"

_sortTblForward="-f"

if [ "$_option" = "-a" ]; then
	_sortTblForward="-f"
elif [ "$_option" = "-d" ]; then
	_sortTblForward="-b"
else
	usage
	die 1
fi



test -f "$_origConfFile" || die 2 "originalConfFile is not given"
test -f "$_vConfFile" || die 2 "mergedConfFile is not given"

if [ "$#" -eq 3 ]; then
	_resultFile="$_origConfFile"
else
	_resultFile="$4"
fi


## Create Temporary Files and Local Variable Definitions
_vConfTbl="$(mktemp ./temp_vConfTbl_XXXXXX)"
_mergedSour="$(mktemp ./temp_mergedSour_XXXXXX)"
_mergedDest="$(mktemp ./temp_mergedDest_XXXXXX)"
_mergedTbl="$(mktemp ./temp_mergedTbl_XXXXXX)"

declare -a mergedObjs


## Local Function Definitions

deleteTempFile()
{
	rm -f "$_vConfTbl" "$_mergedSour" "$_mergedDest" "$_mergedTbl"
}

#@ DESCRIPTION: create parsed tableFile with tr069 configuration file
#@ USAGE: buildTbl confFile tableFile [-f|-b]
#@ OPTION:	-f - sort forward (default)
#@		-b - sort backward
buildTbl()
{
	_inputFile="$1"
	_outputFile="$2"
	_sort_forward=${3:--f}
	_sort_option="-nk 2"

	test -f "$_inputFile"  || { deleteTempFile; die 2 "function buildTbl() - invalid inputFile"; }
	test -f "$_outputFile" || { deleteTempFile; die 2 "function buildTbl() - invalid outputFile"; }

	if [ "$_sort_forward" = "-f" ]; then
		_sort_option="-nk 2"
	elif [ "$_sort_forward" = "-b" ]; then
		_sort_option="-nrk 2"
	else
		{ deleteTempFile; die 2 "function buildTbl() - invalid option($_sort_forward)"; }
	fi

	declare -i depth=0
	declare -i linenum=0
	declare -a objname
	declare -a startline
	local paramName=""

	{
	while read field1 field2 remaining
	do
		linenum=$((linenum+1))
		case "$field1" in
			object|collection)
				depth=$((depth+1))
				objname[$depth]="$field2"
				startline[$depth]=$linenum
				;;

			default|default{)
				depth=$((depth+1))
				objname[$depth]="default"
				startline[$depth]=$linenum
				;;

			}*)
				printf "obj %5d %5d [%s]\n" "${startline[$depth]}" "$linenum" "${objname[*]}"
				unset objname[$depth]
				unset startline[$depth]
				depth=$((depth-1))
				;;

			param)
				paramName="$field2"
				printf "par %5d %5d [%s %s]\n" "$linenum" "$linenum" "${objname[*]}"  "$paramName"
				;;

		esac
	done < "$_inputFile"
	} | sort "$_sort_option" > "$_outputFile"
}


#@ DESCRIPTION: replace from sline1(exclusive) to eline1(exclusive) of file1 with from sline2(inclusive) to eline2(inclusive) of file2
#@ USAGE: insertfile file1 file2 sline1 eline2 sline2 eline2
insertfile() {
	local _file1="", _file2=""
	declare -i _sline1=0
	declare -i _eline1=0
	declare -i _sline2=0
	declare -i _eline2=0

	_file1=${1}
	_file2=${2}
	_sline1=${3:-$_sline1}
	_eline1=${4:-$_eline1}
	_sline2=${5:-$_sline2}
	_eline2=${6:-$_eline2}

	if [ ! -f "$_file1" -o ! -f "$_file2" ]; then
		deleteTempFile
		die 2 "function insertfile() - invalid file"
	fi

	sed -n '1,'"$_sline1"'p ' "$_file1"
	sed -n "$_sline2"','"$_eline2"'p ' "$_file2"
	sed -n "$_eline1"',$p ' "$_file1"
}

#@ DESCRIPTION: Check whether given argument is root object or not
#@ USAGE: isRoot STRING
isRoot()
{
	case "$1" in
		*\\]\\]|"") true;;
		*) false;;
	esac
}

#@ DESCRIPTION: Check whether given argument is root object or not
#@ USAGE: isRoot STRING
deletePart()
{
	local _file1=""
	declare -i _sline1=0
	declare -i _eline1=0

	_file1=${1:?ERROR: not given file1}
	_sline1=${2:-$_sline1}
	_eline1=${3:-$_eline1}

	if [ ! -f "$_file1" ]; then
		deleteTempFile
		die 2 "function deletePart() - invalid file"
	fi
	sed -n '1,'"$_sline1"'p ' "$_file1"
	sed -n "$_eline1"',$p ' "$_file1"
}

cp -f "$_origConfFile" "$_mergedSour"
cp -f "$_mergedSour" "$_mergedDest"

buildTbl "$_mergedSour" "$_mergedTbl" "$_sortTblForward"

if [ "$_option" = "-a" ]; then
	buildTbl "$_vConfFile" "$_vConfTbl" "$_sortTblForward"
fi


case $_option in
	-a)  # add
		while read _type _sline _eline _name
		do
			case "$_type" in
				obj)
		# To improve performance, "Parameter Expansion" is used instead of "sed". The improvement is more than 35%.
		#			pattern=$(echo "$_name"| sed -e 's/\[/\\[/g' -e 's/\]/\\]/g')
					pattern=${_name/\[/\\[}
					pattern=${pattern/\]/\\]}

					searchResult=$(grep "$pattern" "$_mergedTbl" 2>/dev/null)
					if [ -z "$searchResult" ];
					then
						while ! isRoot "$pattern";
						do
							pattern="${pattern% *]}\\]"
							searchResult=$(grep "$pattern" "$_mergedTbl" 2>/dev/null)

							if [ -n "$searchResult" ];
							then
								while read orig_type orig_sline orig_eline remaining
								do
									mergedObjs[${#mergedObjs[@]}]="$_name"
									insertfile "$_mergedSour" "$_vConfFile" "$((orig_eline-1))" "$orig_eline" "$_sline" "$_eline"  > "$_mergedDest"
									cp -f "$_mergedDest" "$_mergedSour"
									buildTbl "$_mergedSour" "$_mergedTbl"
								done < <(echo "$searchResult" | sort -nrk 2)
								break;
							fi
						done
					fi
					;;
				par)
				# To check whether the parameter is already applied or not.
					for _mergedObj in "${mergedObjs[@]}"
					do
						_mergedObj=${_mergedObj//\]/\*}
						case $_name in
							$_mergedObj) continue 2;;
						esac
					done

				# To improve performance, "Parameter Expansion" is used instead of "sed". The improvement is more than 35%.
				#	pattern=$(echo "$_name"| sed -e 's/\[/\\[/g' -e 's/\]/\\]/g')
					pattern=${_name/\[/\\[}
					pattern=${pattern/\]/\\]}

					searchResult=$(grep "$pattern" "$_mergedTbl" 2>/dev/null)
					if [ -z "$searchResult" ]; # does Not Exist
					then
						while ! isRoot "$pattern";
						do
							pattern="${pattern% *]}\\]"
							searchResult=$(grep "$pattern" "$_mergedTbl" 2>/dev/null)

							if [ -n "$searchResult" ];
							then
								echo "$searchResult" | sort -nrk 2 | while read orig_type orig_sline orig_eline remaining
								do
									insertfile "$_mergedSour" "$_vConfFile" "$((orig_eline-1))" "$orig_eline" "$_sline" "$_eline"  > "$_mergedDest"
									cp -f "$_mergedDest" "$_mergedSour"
									buildTbl "$_mergedSour" "$_mergedTbl"
								done
								break;
							fi
						done
					else # Exist
						echo "$searchResult" | sort -nrk 2 | while read orig_type orig_sline orig_eline remaining
						do
							insertfile "$_mergedSour" "$_vConfFile" "$((orig_sline-1))" "$((orig_eline+1))" "$_sline" "$_eline"  > "$_mergedDest"
							cp -f "$_mergedDest" "$_mergedSour"
				# do not need in this case			buildTbl "$_mergedSour" "$_mergedTbl"
						done
					fi
					;;
			esac
		done < "$_vConfTbl"
		;;

	-d) # delete
		while read _type _sline _eline _name
		do
		# This is to take out multiple whitespaces and leading and trailing whitespaces.
		# The reason using pure "tab" character instead of using "[[:space:]]" is for performance enhancement. The improvement is around 40%.
			shopt -s extglob
			_name="[${_name##[*( |	)}"
			_name="${_name%%*( |	)]}]"
			_name="${_name//+( |	)/ }"
		#	_name="${_name//+([[:space:]])/ }"
			shopt -u extglob

			case "$_type" in
				obj|par)
		# To improve performance, "Parameter Expansion" is used instead of "sed". The improvement is more than 35%.
		#			pattern=$(echo "$_name"| sed -e 's/\[/\\[/g' -e 's/\]/\\]/g')
					pattern=${_name/\[/\\[}
					pattern=${pattern/\]/\\]}

					searchResult=$(grep "$pattern" "$_mergedTbl" 2>/dev/null)
					if [ -n "$searchResult" ];
					then
						echo "$searchResult" | while read orig_type orig_sline orig_eline remaining
						do
							deletePart "$_mergedSour" "$((orig_sline-1))" "$((orig_eline+1))" > "$_mergedDest"
							cp -f "$_mergedDest" "$_mergedSour"
							buildTbl "$_mergedSour" "$_mergedTbl"
						done

					fi
					;;
			esac
		done < "$_vConfFile"
		;;
esac



cp -f "$_mergedDest" "$_resultFile"
deleteTempFile

