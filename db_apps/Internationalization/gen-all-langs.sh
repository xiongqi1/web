#!/bin/bash

log() {
	echo "$@"
}

fatal() {
	echo "$@" >&2
	exit 255
}

log "clearing xml directories"
rm -fr ./lang
mkdir ./lang

if [ "$1" = "OPTIMISE" ]; then
	log "generating xml files - OPTIMISE"
	rm -fr lang/*
	if ! ./BuildFiles -O $2/util.js > /dev/null 2> /dev/null; then
		fatal "failed to generate $2/util.js"
	fi
	ls $2/*.html | while read i; do
		echo "generate xml file for $i ..."
		if ! ./BuildFiles -O $i > /dev/null 2> /dev/null; then
			echo "failed to generate $i"
		fi
	done

	# The UI_V2b has created .htmlv2b files. These do not need translation files
	# We can now rename them back to .html
	# There's always at least one file there so no need for empty check
	pushd $2
	for i in *.htmlv2b; do
			fn="${i%%.*}"
			echo "rename $i to $fn.html"
			mv "$i" "$fn.html"
	done
	popd

	for l in "en" "fr" "ar" "de" "it" "es" "pt" "cz" "nl" "tw" "cn" "jp"; do
		mkdir ./lang/$l > /dev/null 2> /dev/null
		log "generating xml files - $l"
		if ! rm ./lang/$l/util.xml > /dev/null 2> /dev/null; then
			echo "remove ./lang/$l/util.xml"
		fi
	done

	if [ "$V_WEBIF_SERVER" = 'turbo' ]; then
		echo "create lowercase symlinks to all files in $2 due to turbo path handling issue"
		for src in $(find "$2" -type f); do
			dest=$(dirname "$src")/"$(basename "$src" | tr '[:upper:]' '[:lower:]')"
			[ "$src" != "$dest" ] && ln -sf "$(basename "$src")" "$dest" || true
		done
	fi
else
	for l in "en" "fr" "ar" "jp"; do
		case $l in
			'en')	lang="English";;
			'ar')	lang="Arabic";;
			'fr')	lang="French";;
			'de')	lang="German";;
			'it')	lang="Italian";;
			'es')	lang="Spanish";;
			'pt')	lang="Portugese";;
			'cz')	lang="Czech";;
			'nl')	lang="Dutch";;
			'cn')	lang="SimpChinese";;
			'tw')	lang="TradChinese";;
			'jp')	lang="Japanese";;
		esac
		if ! mkdir ./lang/$l; then
			fatal "failed to create ./lang/$l"
		fi
		log "generating xml files - $l"
		if ! ./BuildFiles -B $lang > /dev/null 2> /dev/null; then
			fatal "failed to generate $lang"
		fi
		if ! mv *.xml ./lang/$l/; then
			fatal "failed to move xml files into ./lang/$l/"
		fi
	done
fi

# if the html file has 2 lines of the "setTextDomain", the line 23 above will be failed. This issue can be found form the build time log message.

#FIXME: Short term hack to make UI work
#for f in "DHCP" "DMZ" "LAN" "NAT" "pinsettings" "PPPoE" "Reboot" "RIP" "routing" "VPN" "VRRP"; do
 #   touch ./lang/en/$f.xml
#done
