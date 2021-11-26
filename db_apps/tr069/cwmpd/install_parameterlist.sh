#!/usr/bin/env bash

#:	   Title:	install_parameterlist.sh - generate parameter list
#:	Synopsis:	install_parameterlist.sh variant.sh FirmwareRevision input_tr069_conf_file output_html_file
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
#:	   Title:	install_parameterlist.sh - generate parameter list
#:	Synopsis:	install_parameterlist.sh variant.sh FirmwareRevision input_tr069_conf_file output_html_file
#:
EOF
}

## Parse command-line options and check validation
_variantFile="$1"
_releaseVer="$2"
_inputFile="$3"
_outputFile="$4"

if [ "$_variantFile" == '-h' -o "$_variantFile" == '--help' ]; then
	usage
	die 0
fi

test -f "$_variantFile" || die 2 "ERROR: Invalid variant.sh"
test -f "$_inputFile" || die 2 "ERROR: Invalid input_tr069_conf_file"
touch "$_outputFile" 2> /dev/null ||  die 2 "ERROR: Cannot create output_html_file"


shopt -s extglob

## Local Variable Definitions
declare -i depth
declare -a objIndexes
declare -a objNames

## Local Function Definitions

#@ DESCRIPTION: grab a comment from STRING and store in $_COMMENT
#@ USAGE: get_comment STRING
get_comment()
{
	local _content=${@:-}

	_COMMENT="${_content%%#*}"
	_COMMENT="${_content#$_COMMENT}"
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

#@ DESCRIPTION: print STRING with leading tab characters
#@ USAGE: html_printf STRING
html_printf()
{
	declare -i cnt=0
	declare -i numOfTabs=0

	numOfTabs=${depth:-$numOfTabs}

	while [ $cnt -lt $numOfTabs ]
	do
		printf "\t"
		cnt=$((cnt+1))
	done

	printf "$@"
}

add_index()
{
	depth=$((depth+1))
	objIndexes[$depth]=${objIndexes[$depth]:-0}
	objIndexes[$depth]=$((objIndexes[$depth]+1))
}

delet_index()
{
	unset objIndexes[$depth+1]
	depth=$((depth-1))
}

convert_conf_to_html()
{
	depth=0

	while read _type _remain
	do
		case $_type in
			object|collection)
				add_index
				_name="${_remain%%+( |	)*{*}"
				objNames[$depth]=$_name
				_id=$(printf "_%s" "${objIndexes[@]}")
				get_comment "$_remain"
				_tooltip=$(printf "%s." "${objNames[@]}")
				if [ -z "$_COMMENT" ]; then
					html_printf "<li><div id=\"Object${_id}\" class=\"ExpandCollapse\">-</div><div class=\"Object\"><span><a id=\"ObjName${_id}\" class=\"tooltip\">$_name<span class=\"classic\">$_tooltip</span></a></span> ${_type}</div></li>\n"
				else
					html_printf "<li><div id=\"Object${_id}\" class=\"ExpandCollapse\">-</div><div class=\"Object\"><span><a id=\"ObjName${_id}\" class=\"tooltip\">$_name<span class=\"classic\">$_tooltip</span></a></span> ${_type}&nbsp;&nbsp;&nbsp;&nbsp;<span class=\"comment\">$_COMMENT<span></div></li>\n"
				fi
				html_printf "<ul id=\"ExpandCollapseObject${_id}\">\n"
				;;

			}*)
				html_printf "</ul>\n"
				unset objNames[$depth]
				delet_index
# 				debugMsg "CLOSE  $depth:"
				;;

			default|default{)
				add_index
				_name="1"
				objNames[$depth]=$_name
# 				debugMsg "DEFAULT $depth:\t$_name, ID:$(printf "_%s" "${objIndexes[@]}")"
				_id=$(printf "_%s" "${objIndexes[@]}")
				get_comment "$_remain"
				_tooltip=$(printf "%s." "${objNames[@]}")
				if [ -z "$_COMMENT" ]; then
					html_printf "<li><div id=\"Object${_id}\" class=\"ExpandCollapse\">-</div><div class=\"Object\"><span><a id=\"ObjName${_id}\" class=\"tooltip\">$_name<span class=\"classic\">$_tooltip</span></a></span> object</div></li>\n"
				else
					html_printf "<li><div id=\"Object${_id}\" class=\"ExpandCollapse\">-</div><div class=\"Object\"><span><a id=\"ObjName${_id}\" class=\"tooltip\">$_name<span class=\"classic\">$_tooltip</span></a></span> object&nbsp;&nbsp;&nbsp;&nbsp;<span class=\"comment\">$_COMMENT<span></div></li>\n"
				fi
				html_printf "<ul id=\"ExpandCollapseObject${_id}\">\n"
				;;

			param)
# 				debugMsg "PARAMETER:\t$_remain"
				parse_parameter $_remain
				get_comment "$_remain"
				_tooltip=$(printf "%s." "${objNames[@]}")
				_tooltip=${_tooltip}${_p_NAME}
				printf "\t"
				if [ -z "$_COMMENT" ]; then
					html_printf "<li>&#164;&nbsp;<span><a class=\"tooltip\">$_p_NAME<span class=\"classic\">$_tooltip</span></a></span>&nbsp;&nbsp;$_p_TYPE&nbsp;&nbsp;$_p_NOTIFY&nbsp;&nbsp;$_p_ATTRIB</li>\n"
				else
					html_printf "<li>&#164;&nbsp;<span><a class=\"tooltip\">$_p_NAME<span class=\"classic\">$_tooltip</span></a></span>&nbsp;&nbsp;$_p_TYPE&nbsp;&nbsp;$_p_NOTIFY&nbsp;&nbsp;$_p_ATTRIB&nbsp;&nbsp;<span class=\"comment\">$_COMMENT</span></li>\n"
				fi
# 				html_printf "<li> <table> <tr> <td>\"$_p_NAME\"</td> <td>$_p_TYPE</td> <td>$_p_NOTIFY</td> <td>$_p_ATTRIB</td> <td><span class=\"comment\">$_COMMENT</span></td> </tr> </table> </li>\n"
				;;


		esac
	done < "$_inputFile"
}

. $_variantFile

{
cat << EOF
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="pragma" content="nocache">
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
<title>TR069 Parameter List</title>
<%
useSession();
if ( request["SESSION_ID"] != session["sessionid"]) {
	redirect('/index.html?src=' + request["SCRIPT_NAME"]);
	exit(403);
}
%>
<style type="text/css">
	li {
		display:block;
	}
	.Object {
		color:#0000FF;
	}
	.comment {
		color:#FF0000;
		font-style:italic;
		cursor:default;
	}
	.ExpandCollapse {
		float:left;
		margin-right:5px;
		width:8px;
	     	cursor:pointer;
	}
	ul {
		list-style-type:none;
	}

	.tooltip {
		border-bottom: 1px dotted #000000; outline: none;
		cursor: pointer; text-decoration: none;
		position: relative;
	}
	.tooltip span {
		margin-left: -999em;
		position: absolute;
	}
	.tooltip:hover span {
		border-radius: 5px 5px;
		box-shadow: 5px 5px 5px rgba(0, 0, 0, 0.1);
		font-family: Calibri, Tahoma, Geneva, sans-serif;
		position: absolute; left: 1em; top: 2em; z-index: 99;
		margin-left: 0; overflow:auto;
	}
	.classic { padding: 0.3em 1em; }
	.classic {background: #FFFFAA; border: 1px solid #FFAD33; }
</style>
<script type="text/javascript">
	function takeAction(e) {
		var node = e.srcElement == undefined ? e.target : e.srcElement;
		var id = node.getAttribute("id");
		if (id != null && id.indexOf("Object") > -1) {
			if (node.innerHTML == "-") {
				node.innerHTML = "+";
				document.getElementById("ExpandCollapse" + id).style.display = "none";
			} else if (node.innerHTML == "+") {
				node.innerHTML = "-";
				document.getElementById("ExpandCollapse" + id).style.display = "block";
			}
		}

		if (id != null && id.indexOf("ObjName") > -1) {
			var objId=id.replace("ObjName","Object");
			var elem=document.getElementById(objId);

			if (elem.innerHTML == "-") {
				elem.innerHTML = "+";
				document.getElementById("ExpandCollapse" + objId).style.display = "none";
			} else if (elem.innerHTML == "+") {
				elem.innerHTML = "-";
				document.getElementById("ExpandCollapse" + objId).style.display = "block";
			}
		}
	}

	function Expand_all(node) {
		var elems=document.getElementsByTagName("ul");

		for (var i=0; i<elems.length; i++) {
			if (elems[i].id != "root")
				elems[i].style.display = 'block';
		}

		var elems_div=document.getElementsByTagName("div");

		for (var i=0; i<elems_div.length; i++) {
			if (elems_div[i].id.indexOf("Object") > -1)
				elems_div[i].innerHTML = "-";
		}
		// Due to firefox browser blur fisrt to focus.
		node.blur();
		node.focus();
	}

	function Collapse_all() {
		var elems=document.getElementsByTagName("ul");

		for (var i=0; i<elems.length; i++) {
			if (elems[i].id != "root")
				elems[i].style.display = 'none';
		}

		var elems_div=document.getElementsByTagName("div");

		for (var i=0; i<elems_div.length; i++) {
			if (elems_div[i].id.indexOf("Object") > -1)
				elems_div[i].innerHTML = "+";
		}
	}

	function init() {
		//need some delay for chrome broswer
		setTimeout(function() {window.scrollTo(0, 0);},1)
	}
</script>
</head>

<body onload="init();">

<h2 style='text-align:center'>TR069 Parameter List</h2>
<table border='1'>
<tr><th>Product:</th><th>&nbsp;&nbsp;${V_PRODUCT}</th></tr>
<tr><th>Version:</th><th>&nbsp;&nbsp;${_releaseVer}</th></tr>
<tr><th>ModelName:</th><th>&nbsp;&nbsp;${V_PRODUCT}</th></tr>
<tr><th>Description:</th><th>&nbsp;&nbsp;${V_IDENTITY}</th></tr>
<tr><th>ProductClass:</th><th>&nbsp;&nbsp;${V_CLASS}</th></tr>
</table>

<p>&nbsp;</p>

<input type="button" onclick="Expand_all(this);" value="Expand">
<input type="button" onclick="Collapse_all();" value="Collapse">

<ul id="root" onclick="takeAction(event);">

$(convert_conf_to_html)

</ul>
<p>&nbsp;</p>

<input type="button" onclick="Expand_all(this);" value="Expand">
<input type="button" onclick="Collapse_all();" value="Collapse">

</body>
</html>
EOF
} > $_outputFile


shopt -u extglob