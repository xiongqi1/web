The file quectel-qmi-proxy.c is from Quectel.
This file implements a proxy server that allows multiple instances of
call manager (qmimgr) to share a single /dev/cdc-wdm0 device.
Quectel modules (EC21, EC25 etc..) in QMAP mode (see kernel/drivers/net/usb/qmi_wwan.c)
can provide multiple wwan interfaces but has only one QMI control channel.
