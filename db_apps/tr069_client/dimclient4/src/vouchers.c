/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include "globals.h"
#include "paramaccess.h"
#include "utils.h"
#include "option.h"
#include "optionStore.h"

int storeVouchers( struct ArrayOfVouchers *vouchers )
{
#ifdef HAVE_VOUCHERS_OPTIONS

	const char *filename;
	FILE *vFile;
	int idx;
	int ret = OK;

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_VOUCHERS, "Array of Vouchers: %d\n", vouchers->__size );
	)
	
	for ( idx = 0; idx != vouchers->__size; idx++ ) {
		filename = getVoucherFilename(idx);
		vFile = fopen( filename, "w+");
		fwrite( vouchers[idx].__ptrVoucher->__ptr, vouchers[idx].__ptrVoucher->__size, 1, vFile );
		fclose( vFile );
		ret = readVoucherFile( filename );
		if ( ret != OK )
			return ret;
	}
	return OK;
#else
	return ERR_METHOD_NOT_SUPPORTED;
#endif /* HAVE_VOUCHERS_OPTIONS */
}
