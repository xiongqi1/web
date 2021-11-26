#!/usr/bin/env bash

#:	   Title:	install_datamodelxml.sh - generate data model xml file
#:	Synopsis:	install_datamodelxml.sh variant.sh input_tr069_conf_file output_xml_file
#:

debugMsg() {
	printf "$@" >&2
}
## Script metadata
scriptname=${0##*/}

## Common Function Definitions

#@ DESCRIPTION: print error message and exit with supplied return code
#@ USAGE: die STATUS [MESSAGE]
die()
{
	error="$1"
	shift
	[ -n "$*" ] && printf "%s - %s\n" "$scriptname" "$*" >&2
	exit "$error"
}

#@ DESCRIPTION: print usage information
#@ USAGE: usage
usage() {
cat << EOF
#:
#:	   Title:	install_datamodelxml.sh - generate data model xml file
#:	Synopsis:	install_datamodelxml.sh variant.sh input_tr069_conf_file output_xml_file
#:
EOF
}

## Parse command-line options and check validation
_variantFile="$1"
_inputFile="$2"
_outputFile="$3"

if [ "$_variantFile" == '-h' -o "$_variantFile" == '--help' ]; then
	usage
	die 0
fi

test -f "$_variantFile" || die 2 "ERROR: Invalid variant.sh"
test -f "$_inputFile" || die 2 "ERROR: Invalid input_tr069_conf_file"
touch "$_outputFile" 2> /dev/null ||  die 2 "ERROR: Cannot create output_xml_file"


shopt -s extglob

## Local Variable Definitions
declare -i depth
declare -a objNames
declare -a objBuf
declare -a parmBuf
declare -a collStates

## Local Function Definitions

#@ DESCRIPTION: grab a comment from STRING and store in $_COMMENT
#@ USAGE: get_comment STRING
get_comment()
{
	local _content=${@:-}

	_COMMENT="${_content%%#*}"
	_COMMENT="${_content#$_COMMENT}"
	_COMMENT="${_COMMENT##+(#)*([[:space:]])}" # remove leading # and whitespace
}

#@ DESCRIPTION: grab a comment from STRING and store in $_p_NAME, $_p_TYPE, $_p_NOTIFY and $_p_ATTRIB
#@ USAGE: parse_parameter STRING
parse_parameter()
{
	local _content=${@:-}
	local _remaining=""

	_p_NAME=${_content%%+( |	)*}
	_remaining=${_content##$_p_NAME}
	_remaining=${_remaining##+( |	)}

	_p_TYPE=${_remaining%%+( |	)*}
	_remaining=${_remaining##$_p_TYPE}
	_remaining=${_remaining##+( |	)}

	_p_NOTIFY=${_remaining%%+( |	)*}
	_remaining=${_remaining##$_p_NOTIFY}
	_remaining=${_remaining##+( |	)}

	_p_ATTRIB=${_remaining%%+( |	)*}
}

convert_conf_to_xml()
{
	NEWLINE=$'\n'
	collName=
	depth=0
	while read _type _remain
	do
		case $_type in
			object)
				depth=$((depth+1))
				_name="${_remain%%+( |	)*{*}"
				if [ "${collStates[$depth]}" = 1 ]; then # we are in a collection
					objNames[$depth]="${collName}.{i}"
					collStates[$depth]=2
					maxEntries=unbounded
					access="$collAccess"
				else # normal object
					objNames[$depth]=$_name
					maxEntries=1
					access=readOnly
				fi
				get_comment "$_remain"
				full_path=$(printf "%s." "${objNames[@]}")
				objBuf[$depth]=${objBuf[$depth]:-}
				objBuf[$depth]+="    <object ref=\"${full_path}\" access=\"${access}\" minEntries=\"1\" maxEntries=\"${maxEntries}\">${NEWLINE}"
                objBuf[$depth+1]=
				parmBuf[$depth]=
				# debugMsg "Object starts: $depth"
				;;

			collection)
				collName="${_remain%%+( |	)*{*}"
				get_comment "$_remain"
				# comment following a collection will state if it is read-only
				if [ "$_COMMENT" = "readonly" ]; then
					collAccess=readOnly
				else # collection is read-write by default
					collAccess=readWrite
				fi
				collStates[$depth+1]=1 # mark we are in a collection
				# debugMsg "Collection starts: $depth"
				;;

			}*)
				if [ "${collStates[$depth]}" = 2 ]; then # we are in an object within a collection
					collStates[$depth]=1
				else
					collStates[$depth]=
					objBuf[$depth]+="${parmBuf[$depth]}" # collect all parameters within this object
					objBuf[$depth]+="    </object>${NEWLINE}"
					objBuf[$depth]+="${objBuf[$depth+1]}" # collect all subobjects within this object
					unset objNames[$depth]
					depth=$((depth-1))
				fi
				# debugMsg "CLOSE:  $depth"
				;;

			default|default{)
				depth=$((depth+1))
				objNames[$depth]="${collName}.{i}"
				collStates[$depth]=2
				# debugMsg "DEFAULT starts: $depth"
				get_comment "$_remain"
				full_path=$(printf "%s." "${objNames[@]}")
				objBuf[$depth]=${objBuf[$depth]:-}
				objBuf[$depth]+="    <object ref=\"${full_path}\" access=\"${collAccess}\" minEntries=\"0\" maxEntries=\"unbounded\">${NEWLINE}"
				objBuf[$depth+1]=
				parmBuf[$depth]=
				;;

			param)
				# debugMsg "PARAMETER:\t$_remain"
				parse_parameter $_remain
				get_comment "$_remain"
				if [ "$_p_ATTRIB" = "readonly" ]; then
					access="readOnly"
				else
					access="readWrite"
				fi
				if [ -n "$_COMMENT" ]; then
					parmBuf[$depth]+="      <parameter ref=\"${_p_NAME}\" access=\"${access}\">${NEWLINE}"
					parmBuf[$depth]+="        <annotation>${_COMMENT}</annotation>${NEWLINE}"
					parmBuf[$depth]+="      </parameter>${NEWLINE}"
				else
					parmBuf[$depth]+="      <parameter ref=\"${_p_NAME}\" access=\"${access}\"/>${NEWLINE}"
				fi
				;;

		esac
	done < "$_inputFile"
	echo "${objBuf[1]}"
}

. $_variantFile

{
cat << EOF
<dt:document xmlns:dt="urn:broadband-forum-org:cwmp:devicetype-1-1"
             deviceType="urn:netcommwireless-com:${V_PRODUCT}">
  <annotation>
    ${V_IDENTITY}
  </annotation>
  <import file="tr-181-2-9-0.xml" spec="urn:broadband-forum-org:tr-181-2-9-0">
    <model name="Device:2.9"/>
  </import>

  <model ref="Device:2.9">

$(convert_conf_to_xml)

  </model>

</dt:document>

EOF
} > $_outputFile


shopt -u extglob
