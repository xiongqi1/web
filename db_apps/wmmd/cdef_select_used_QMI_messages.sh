#!/bin/sh
#
# Copyright (C) 2021 Casa Systems.
#
# Remove some not used message entires to save WMMD's memory
#

lua_qmi_msg_file="lua_qmi_msg_file"
qmi_msg_tmp_file="qmi_msg.pch.tmp"

if [ -f "./qmi_msg.pch" ]; then
	#backup qmi_msg.pch to qmi_msg_total.pch
	cp -f qmi_msg.pch qmi_msg_total.pch

	#create qmi used m.QMI_ message file
	grep 'm\.QMI_' *.lua | sed -rn 's/.*(m\.QMI_[0-9A-Za-z_]+).*/\1/p' | sort | uniq > ${lua_qmi_msg_file}

	#add qmi used i.QMI_ messages into file
	grep 'QMI_.*_IND' *.lua | sed -nr 's/.*(QMI_.*_IND)[^A-Za-z0-9].*/i.\1/p' | sort | uniq >> ${lua_qmi_msg_file}

	#select wmmd used m.QMI_ msg entires based on lua_qmi_msg_file
	awk -F"=" 'NR==FNR {a[$0]} NR>FNR&&($1 in a) {print $0}' ${lua_qmi_msg_file} qmi_msg_total.pch > ${qmi_msg_tmp_file}

	#re-create qmi_msg.pch and remove temporary file
	mv -f ${qmi_msg_tmp_file} qmi_msg.pch
	rm -f ${lua_qmi_msg_file}
fi
