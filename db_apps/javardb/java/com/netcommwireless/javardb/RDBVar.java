package com.netcommwireless.javardb;

/**
 * Stores all information about a single RDB variable.
 * @author Bill Bennett william.bennett@netcommwireless.com
 *
 */
public class RDBVar {
	/**
	 * Variable name - always filled out before use
	 */
	public String name;
	/**
	 * Variable value - filled before RDB.set() or during RDB.get()
	 */
	public byte[] value;
	/**
	 * Variable flags - filled before RDB.setFlags() or during RDB.get()
	 */
	public int flags;
	/**
	 * Variable perms - filled during RDB.get()
	 */
	public int perms;
	
	public RDBVar(String name, byte[] value, int flags, int perms) {
		this.name = name;
		this.value = value;
		this.flags = flags;
		this.perms = perms;
	}
	public RDBVar(String name, byte[] value) {
		this(name, value, 0, 0);
	}
    public RDBVar(String name, String value) {
        this(name);
        setString(value);
	}
	public RDBVar(String name) {
		this(name, null, 0, 0);
	}
	/**
	 * Convert value to a string.
	 * Value must be either ASCII or Java's modified UTF8.
	 * @return result of conversion
	 */
	public String getString() {
		return new String(value);
	}
	/**
	 * Set value from string.
	 * Value will be Java's modified UTF8.
	 */
	public void setString(String str) {
        byte[] stringBytes = str.getBytes();
        int len = stringBytes.length;
        byte[] ntBytes = new byte[len+1];
        System.arraycopy(stringBytes, 0, ntBytes, 0, len);
        value = ntBytes;
	}
	/**
	 * Convert value to an integer.
	 * Value must be a sequence of digits.
	 * @return result of conversion
	 * @throws NumberFormatException if it's not an integer
	 */
	public int getInt() throws NumberFormatException {
		return Integer.parseInt(getString());
	}
	/**
	 * Convert value to an integer.
	 * Value should be a sequence of digits.
	 * @param def default value to use if conversion's not possible
	 * @return result of conversion or def if it's not an integer
	 */
	public int getIntDef(int def) {
		try {
			return Integer.parseInt(getString());
		} catch (NumberFormatException e) {
			return def;
		}
	}
	
	public String flagsToString() {
		StringBuffer res = new StringBuffer();
		if ((flags & RDB.PERSIST) != 0)
			res.append(",PERSIST");
		if ((flags & RDB.CRYPT) != 0)
			res.append(",CRYPT");
		if ((flags & RDB.HASH) != 0)
			res.append(",HASH");
		int start = (res.length() > 0)? 1:0;
		return res.substring(start);
	}
	
	public String toString() {
		if (value == null)
			return name + "<NOT READ>";
		return name + "(" + flagsToString() + ")=" + getString();
	}
}
