package com.netcommwireless.javardb;

/**
 * Thrown by the RDB.*E() functions to pass the library's error code.
 * @author Bill Bennett william.bennett@netcommwireless.com
 *
 */
public class RDBException extends Exception {
	private static final long serialVersionUID = -1015904874767335274L;
	/**
	 * The POSITIVE error code from the C library; compare with RDB.E* vars.
	 * The values actually come from errno.h.
	 */
	public int errorCode;
	/**
	 * The instance/session that generated the error.
	 */
	public RDB generator;

	public RDBException(RDB from, String message, int code) {
		super(message);
		errorCode = code;
		generator = from;
	}
/*	public RDBException() {
		// TODO Auto-generated constructor stub
	}

	public RDBException(String message) {
		super(message);
		// TODO Auto-generated constructor stub
	}

	public RDBException(Throwable cause) {
		super(cause);
		// TODO Auto-generated constructor stub
	}

	public RDBException(String message, Throwable cause) {
		super(message, cause);
		// TODO Auto-generated constructor stub
	}

	public RDBException(String message, Throwable cause,
			boolean enableSuppression, boolean writableStackTrace) {
		super(message, cause, enableSuppression, writableStackTrace);
		// TODO Auto-generated constructor stub
	}*/

	@Override
	public String toString() {
		String serr = super.toString();
		if (errorCode == RDB.EFAULT)
			return "RDB session already closed - " + serr;
		else if (errorCode == RDB.ENOENT)
			return "variable not found - " + serr;
		else if (errorCode == RDB.EPERM)
			return "Permission problem - " + serr;
		else if (errorCode == RDB.EOVERFLOW)
			return "Too much data - " + serr;
		else if (errorCode == RDB.EBUSY)
			return "EBUSY - " + serr;
		else
			return "Unkown error code (" + errorCode + ") - " + serr;
	}

}
