/*!
 * Copyright Notice:
 * Copyright (C) 2012 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#ifndef STRCONVERTTABLE_H_20120831
#define STRCONVERTTABLE_H_20120831

#define STRCONVERT_TABLE_NAME(tableName)		_strConvTable_##tableName
#define STRCONVERT_FUNCTION_NAME(tableName) _strConvFunc_##tableName

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define STRCONVERT_TABLE_BEGIN(tableName) \
	static const char* STRCONVERT_TABLE_NAME(tableName)[] =

#define STRCONVERT_TABLE_END(tableName) \
	;\
	const char* STRCONVERT_FUNCTION_NAME(tableName)(int iIndex)\
	{\
		return cnsmgr_convertToStr(iIndex,STRCONVERT_TABLE_NAME(tableName),__countOf(STRCONVERT_TABLE_NAME(tableName)));\
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define STRCONVERT_FUNCTION_CALL(tableName,index) \
	STRCONVERT_FUNCTION_NAME(tableName)(index)

#define MAX_BAND_LIST_LENGTH	512
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// string arrays
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

STRCONVERT_TABLE_BEGIN(EnableStat)
{
	"Disabled",
	"Enabled"
}
STRCONVERT_TABLE_END(EnableStat)

STRCONVERT_TABLE_BEGIN(SysManualMode)
{
	"Automatic network selection",
	"Manual network selection"
}
STRCONVERT_TABLE_END(SysManualMode)

STRCONVERT_TABLE_BEGIN(SysMode)
{
	"None",
	"GPRS",
	"EDGE",
	"UMTS",
	"HSDPA",
	"HSUPA",
	"HSPA",
	"HSPA (E-UTRAN)",
	"DC-HSPA+",
	"LTE"
}
STRCONVERT_TABLE_END(SysMode)

STRCONVERT_TABLE_BEGIN(SysServStat)
{
	"Service available",
	"Emergency / limited service",
	"No service"
}
STRCONVERT_TABLE_END(SysServStat)

STRCONVERT_TABLE_BEGIN(ModemStat)
{
	"OffLine",
	"OnLine",
	"Low Power Mode"
}
STRCONVERT_TABLE_END(ModemStat)


STRCONVERT_TABLE_BEGIN(NetworkSelMode)
{
	"Automatic",
	"Manual"
}
STRCONVERT_TABLE_END(NetworkSelMode)

STRCONVERT_TABLE_BEGIN(RoamStat)
{
	"Not roaming",
	"Roaming"
}
STRCONVERT_TABLE_END(RoamStat)

/* add for compatibility with simple_at_manager */
STRCONVERT_TABLE_BEGIN(RoamStat2)
{
	"deactive",
	"active"
}
STRCONVERT_TABLE_END(RoamStat2)

STRCONVERT_TABLE_BEGIN(ServType)
{
	"Circuit-switched service",
	"GPRS service",
	"Combined service",
	"Invalid service"
}
STRCONVERT_TABLE_END(ServType)

// ServStat
STRCONVERT_TABLE_BEGIN(ServStat)
{
	"Normal",
	"Emergency only",
	"No service",
	"Access dificulty",
	"Forbidden PLMN",
	"Location area is forbidden",
	"national roaming is forbidden",
	"Illegal mobile station",
	"Illegal mobile equipment",
	"IMSI unknown in HLR",
	"Authentication failure",
	"GPRS failed"
}
STRCONVERT_TABLE_END(ServStat)

// OpType
STRCONVERT_TABLE_BEGIN(OpType)
{
	NULL,
	"get ",
	"getr",
	"set ",
	"setr",
	"nten",
	"ntre",
	"notf"
}
STRCONVERT_TABLE_END(OpType)

// StatMsg
STRCONVERT_TABLE_BEGIN(StatMsg)
{
	"succ",
	"fail",
	"timeout"
}
STRCONVERT_TABLE_END(StatMsg)

// SmsStatMsg
STRCONVERT_TABLE_BEGIN(SmsStatMsg)
{
	"succ",						/* 0x00 */
	"SMS not ready",			/* 0x01 */
	"SMS record not found",		/* 0x02 */
	"SIM error",				/* 0x03 */
	"PS error",					/* 0x04 */
	"Missing msg ID",			/* 0x05 */
	"Invalid record",			/* 0x06 */
	"Concatenated link error",	/* 0x07 */
	"Record not submitted",		/* 0x08 */
	"Send from SIM in progress",/* 0x09 */
	"NVRAM error",				/* 0x0A */
	"FDN mismatch",				/* 0x0B */
	"Unknown error"				/* 0x0C */
}
STRCONVERT_TABLE_END(SmsStatMsg)

// DatabaseMsg
STRCONVERT_TABLE_BEGIN(DatabaseMsg)
{
	"[error]",
	"[busy]",
	"[done]",
	"[unknown]"
}
STRCONVERT_TABLE_END(DatabaseMsg)

// RetryInfoType
STRCONVERT_TABLE_BEGIN(RetryInfoType)
{
	"CHV1",
	"CHV2",
	"Unblocking CHV1",
	"unblocking CHV2",
	"No retry information available"
}
STRCONVERT_TABLE_END(RetryInfoType)

// ResPrevUsrOp
STRCONVERT_TABLE_BEGIN(ResPrevUsrOp)
{
	"Operation succeeded",
	"Operation failed",
	"Operation failed because CHV verification is disabled",
	"Operation failed becuase the CHV starts with the same digits as an emergency number"
}
STRCONVERT_TABLE_END(ResPrevUsrOp)


// PrevUsrOpToStr
STRCONVERT_TABLE_BEGIN(PrevUsrOpToStr)
{
	"No previous operation",
	"Change CHV1",
	"Change CHV2",
	"Unused",
	"Verify CHV1",
	"Verify CHV2",
	"Verify unblocking CHV1",
	"Verify unblocking CHV2",
	"MEP unlock",
	"Enable CHV1 verification",
	"Disable CHV1 verification"
}
STRCONVERT_TABLE_END(PrevUsrOpToStr)

// SimUsrOpReq
STRCONVERT_TABLE_BEGIN(SimUsrOpReq)
{
	"No operation required",
	"Enter CHV1",
	"Enter CHV2",
	"Enter unblocking CHV1",
	"Enter unblocking CHV2",
	"enter MEP special code"
}
STRCONVERT_TABLE_END(SimUsrOpReq)

// SimStat
STRCONVERT_TABLE_BEGIN(SimStat)
{
	"SIM OK",
	"SIM not inserted",
	"SIM removed",
	"SIM initialized failure",
	"SIM general failure",
	"SIM locked",
	"SIM PUK",
	"SIM CHV2 blocked",
	"SIM CHV1 rejected",
	"SIM CHV2 rejected",
	"SIM MEP locked",
	"SIM network reject"
}
STRCONVERT_TABLE_END(SimStat)

STRCONVERT_TABLE_BEGIN(ModemModel)
{
	NULL,						// 0x00
	NULL,						// 0x01
	NULL,						// 0x02
	NULL,						// 0x03
	NULL,						// 0x04
	NULL,						// 0x05
	NULL,						// 0x06
	NULL,						// 0x07
	NULL,						// 0x08
	NULL,						// 0x09
	"AC850",				// 0x0a
	"AC860",				// 0x0b
	"MC8755",				// 0x0c
	"MC8756",				// 0x0d
	NULL,						// 0x0e
	NULL,						// 0x0f
	NULL,						// 0x10
	NULL,						// 0x11
	NULL,						// 0x12
	"AC875",				// 0x13
	"MC8775",				// 0x14
	"MC8775V",			// 0x15
	NULL,						// 0x16
	NULL,						// 0x17
	"AC880",				// 0x18
	"AC881",				// 0x19
	"MC8780",				// 0x1a
	"MC8781",				// 0x1b
	NULL,						// 0x1c
	NULL,						// 0x1d
	NULL,						// 0x1e
	NULL,						// 0x1f
	NULL,						// 0x20
	NULL,						// 0x21
	NULL,						// 0x22
	NULL,						// 0x23
	NULL,						// 0x24
	NULL,						// 0x25
	NULL,						// 0x26
	NULL,						// 0x27
	"MC8785V", 			// 0x28
	NULL,						// 0x29
	NULL,						// 0x2a
	"MC8790",				// 0x2b
	"MC8790V",			// 0x2c
	NULL,						// 0x2d
	NULL,						// 0x2e
	NULL,						// 0x2f

	NULL,						// 0x30
	NULL,
	NULL,
	"MC8792V",					// 0x33
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	NULL,						// 0x40
	NULL,
	NULL,
	"USB 306/308",
	NULL,
	NULL,
	NULL,
	"USB 301",
	NULL,
	NULL,
	NULL,
	NULL,
	"AirCard 312U", // TODO: need to change this to module name - MC8801?
	NULL,
	NULL,
	NULL,

	"MC8795V",	// 0x50
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	NULL, // 0x60
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	NULL, // 0x70
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,


	NULL, // 0x80
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	NULL, // 0x90
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	NULL, // 0xa0
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	NULL, // 0xb0
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	NULL, // 0xc0
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	NULL, // 0xd0
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	NULL, // 0xe0
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	NULL, // 0xf0
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
#ifdef PLATFORM_Platypus2
	"MC8704",
#else
	"AirCard 320U",
#endif
	NULL,
}

STRCONVERT_TABLE_END(ModemModel)

// PD error
STRCONVERT_TABLE_BEGIN(PDError)
{
	"No error",
	"Internal modem error",
	"Bad service type",
	"Bad session type",
	"Invalid privacy",
	"Invalid data download",
	"Invalid network access",
	"Invalid operation mode",
	"Invalid number of fixes",
	"Invalid server info",
	"Invalid timeout",
	"Invalid QoS parameter",
	"No session active",
	"Session already active",
	"Session busy",
	"Phone is offline",
	"CDMA lock error",
	"GPS lock error",
	"Invalid state",
	"Connection failure",
	"No buffers available",
	"Searcher error",
	"Cannot report now",
	"Resource contention",
	"Mode not supported",
	"Authentication failed"
}
STRCONVERT_TABLE_END(PDError)

// LocationFixError
STRCONVERT_TABLE_BEGIN(LocationFixError)
{
	"Offline",
	"No service",
	"No connection",
	"No data",
	"Session busy",
	"Reserved",
	"GPS disabled",
	"Connection failed",
	"Error state",
	"Client ended",
	"UI ended",
	"Network ended",
	"Timeout",
	"Privacy level",
	"Network access error",
	"Fix error",
	"PDE rejected",
	"Traffic channel exited",
	"E911",
	"Server error",
	"Stale BS information",
	"Resource contention",
	"Authentication parameter failed",
	"Authentication failed - local",
	"Authentication failed - network",
}
STRCONVERT_TABLE_END(LocationFixError)

// LocationFixError2 - 0x1000
STRCONVERT_TABLE_BEGIN(LocationFixError2)
{
	"VX LCS agent auth fail",
	"Unknown system error",
	"Unsupported service",
	"Subscription violation"
	"Desired fix method failed",
	"Antenna switch",
	"No fix reported due to no Tx confirmation",
	"Network indicated normal ending of session",
	"No error specified by the network",
	"No resources left on the network",
	"Position server not available",
	"Network reported unsupported protocol",
	"SS MOLR error - System failure",
	"SS MOLR error - Unexpected data value",
	"SS MOLR error - Data missing",
	"SS MOLR error - Facility not supported",
	"SS MOLR error - SS subscription violation",
	"SS MOLR error - Position method failure",
	"SS MOLR error - Undefined",
	"Control plane-s SMLC timeout, may or may not end PD",
	"Control plane's MT guard time expires",
	"End waiting for additional assistance"
}
STRCONVERT_TABLE_END(LocationFixError2)

#endif  /* STRCONVERTTABLE_H_20120831 */

