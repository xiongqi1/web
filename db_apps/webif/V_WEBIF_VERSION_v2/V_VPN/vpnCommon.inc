<%
// The following function scans the RDB link profiles for the requested information
// and place it into a javascript suitable format.
// The inputs are
//     devices - an array of devices of interest "dev1","dev2","dev3"
//      args - the RDB variables of interest. In the simplest case can be just a string of the RDB variable.
//             It can also be an array of three strings - "RDB variable", "javascript alias", "default value"
// The function returns a javascript string

vpnRdbBase="";

function createVpnElements(devices, args, encodingArgs ) {

	var elements="";

	var firstAvailableProfile = 7;  // Profiles 0-6 are reserved.
	var lastAvailableProfile = 32;
	var delFlagUsed = "0";          // Profile in use.

	function getRdb(varName,default){
		varName=vpnRdbBase+varName;
		var val=get_single(varName);
		if ( val == "N/A" ) val = default;
		return val;
	}

	function writeJavaVar(varName,val,suffix) {
		return '"'+varName+'":"'+val+'"'+suffix+"\n";
	}

	function writeRdbProfile(rdbVar,javaVar,default,suffix) {
		return writeJavaVar(javaVar,getRdb(rdbVar,default),suffix);
	}

	function writeEncodingRdbProfile(rdbVar, javaVar, suffix) {
		var varRdbName = vpnRdbBase + rdbVar;
		var val;
		val = get_single(varRdbName, "", "--base64");
		if ( val == "N/A" ) {
			val = "";
		}
		return '"'+javaVar+'":Base64.decode("'+val+'")'+suffix+"\n";
	}

//	Loop through all profiles, as they may be non-contiguous.
	var j=0;
	for (var i = firstAvailableProfile; i <= lastAvailableProfile; i++) {
		vpnRdbBase = "link.profile."+i+".";
		var dev = getRdb("dev", "N/A");
		if ( dev == "N/A" ) {
			continue;
		}
		var rdb_var = getRdb("delflag", "N/A");
		if (rdb_var != delFlagUsed) {
			continue;
		}
//		check that this profile is what we are looking for
		var matchDev=0;
		for (var devNo=0; devNo<devices.length; devNo++) {
			if (dev==devices[devNo]) {
				matchDev++;
			}
		}
		if(matchDev==0) {
			continue;
		}
		var type = dev;
		
		if(j>0) // put seperator if any previous exists
			elements=elements+",";
		j++;
		elements=elements+"{\n"+ writeJavaVar("type",type,",");
		// iterate through all the variables requested
		for (var k = 0; k < args.length; k++) {
			var arg=args[k];
			if (arg != undefined && arg != "") {
				var defaultVal;
				var rdbName;
				var javaName;
				if ( arg.length == 3 ) {
					rdbName=arg[0];
					defaultVal=arg[2];
					javaName=arg[1];
					if ( javaName == "" ) javaName = rdbName;
				} else {
					rdbName=arg;
					defaultVal="";
					javaName=arg;
				}
				elements=elements+writeRdbProfile(rdbName,javaName,defaultVal,",");
			}
		}
		if (encodingArgs != undefined) {
			for (var encodingIterator = 0; encodingIterator < encodingArgs.length; encodingIterator++) {
				var encodingVar = encodingArgs[encodingIterator];
				if (encodingVar != undefined && encodingVar != "") {
					elements = elements + writeEncodingRdbProfile(encodingVar, encodingVar, ",");
				}
			}
		}

		elements=elements+writeJavaVar("profilenum",i,"")+"}\n";
	}
	return elements;
}

%>

