----
-- CWMP Protocol Fault Codes
----

cwmpError = {
	OK			= 0,
	NotSupported		= 9000,
	RequestDenied		= 9001,
	InternalError		= 9002,
	InvalidArgument		= 9003,
	ResourceExceeded	= 9004,
	InvalidParameterName	= 9005,
	InvalidParameterType	= 9006,
	InvalidParameterValue	= 9007,
	ReadOnly		= 9008,
	WriteOnly		= 9008,
	NotificationRejection	= 9009,
	DownloadFailure		= 9010,
	UploadFailure		= 9011,
	TransferAuthFailure	= 9012,
	NoTransferProtocol	= 9013,
	DownloadMulticastGroup	= 9014,
	DownloadFileServer	= 9015,
	DownloadAccessFile	= 9016,
	DownloadPartial		= 9017,
	DownloadCorrupt		= 9018,
	DownloadAuth		= 9019
}



local funcs = {}
for k, v in pairs(cwmpError) do
	funcs[k] = function() return cwmpError[k] end
end
cwmpError.funcs = funcs
