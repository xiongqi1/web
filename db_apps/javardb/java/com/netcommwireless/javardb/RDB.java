package com.netcommwireless.javardb;

/**
 * Manages the RDB session and interfaces to librdb. * Multithread-safe (only
 * one thread can waitForTriggers()) * Supports binary values * Can choose to
 * use either Java Exceptions or errno return codes to handle errors
 * @author Bill Bennett william.bennett@netcommwireless.com
 *
 */
public class RDB {
	/**
	 * Stores the session pointer for the underlying library.
	 */
	private long rdbSession = 0;
	/**
	 * Just used to make sure RDBVar isn't unloaded before the library.
	 */
	private RDBVar cache = new RDBVar("cacheonly");

	/**
	 * Error codes to compare with function return values. Note the return
	 * values are negative to compare like: if (get() == -ENOENT) These can't be
	 * final because the JNI code sets them at load.
	 */
	public static int ENOENT = 0, EBUSY = 0, EPERM = 0, EOVERFLOW = 0, EFAULT = 0;
	/**
	 * Flags (bitmask constants) used for getNames(), getVars() and
	 * RDBVar.flags. Combine them with the bitwise OR operator, e.g. PERSIST |
	 * CRYPT. These can't be final because the JNI code sets them at load.
	 */
	public static int PERSIST = 0x0020, CRYPT = 0x0040, HASH = 0x0080;

	static int TRIGGERED = 0x0008;

	// load the JNI-wrapper for librdb
	static {
		System.loadLibrary("javardb");
	}

	/**
	 * Create session using default device. Equivalent to RDB("")
	 * @throws RDBException
	 *             if session can't be created
	 */
	public RDB() throws RDBException {
		this("");
	}

	/**
	 * Create RDB session using given device.
	 * @param dev
	 *            Path to device file, "" means default
	 * @throws RDBException
	 *             if session can't be created
	 */
	public RDB(String dev) throws RDBException {
		// System.out.println("Java constructor");
		int res = openLib(dev);
		if (res < 0) {
			throw new RDBException(this, "Could not open RDB", -res);
		}
	}

	/**
	 * Searches all variable names from database that have a specific flag.
	 * @param varNamePart
	 *            part of an ASCII variable name
	 * @param flagsSet
	 *            specific flags that must be set ORed together
	 * @param flagsClear
	 *            specific flags that must be clear ORed together
	 * @return array of ASCII variable names that match
	 */
	public synchronized native String[] getNames(String varNamePart, int flagsSet, int flagsClear);

	/**
	 * Searches all variable names from database that have a specific flag.
	 * @param varNamePart
	 *            part of an ASCII variable name
	 * @param flagsSet
	 *            specific flags that must be set ORed together
	 * @param flagsClear
	 *            specific flags that must be clear ORed together
	 * @return array of matched variables
	 */
	public RDBVar[] getVars(String varNamePart, int flagsSet, int flagsClear) {
		String[] matched = getNames(varNamePart, flagsSet, flagsClear);
		if (matched == null) {
			return null;
		}
		RDBVar[] res = new RDBVar[matched.length];
		for (int i = 0; i < matched.length; i++) {
			res[i] = new RDBVar(matched[i]);
			get(res[i]);
		}
		return res;
	}

	/**
	 * Waits for at least one subscribed var to trigger and returns that list.
	 * If not using 'clear' then you must call complete() yourself.
	 * @param clear
	 *            if true the returned vars will be marked complete()
	 *            automatically
	 * @param milliseconds
	 *            Time to wait - 0 means forever
	 * @return Vars that triggered (could be none or a few depending on
	 *         scheduling and timeout)
	 */
	public RDBVar[] waitForTriggers(long milliseconds) {
		synchronized (cache) {
			// System.out.println("SelectLoop");
			long end = System.currentTimeMillis() + milliseconds;
			while (doSelect((int) milliseconds / 1000, (int) (milliseconds % 1000) * 1000) <= 0) {
				milliseconds = end - System.currentTimeMillis();
				//System.out.println("SelectRemaining:" + milliseconds);
				if (milliseconds <= 0) {
					return new RDBVar[0];
				}
			}
			//System.out.println("SelectDone");
			return getVars("", TRIGGERED, TRIGGERED);
		}
	}

	/**
	 * Subscribes for notifications if the given EXISTING variable is written or
	 * deleted. A process can only subscribe to notifications if the variable is
	 * readable by that process.
	 * @param varName
	 *            ASCII variable name, less than NAMESIZE characters long
	 * @param callback
	 *            Object to be notified when the variable is written, or null if
	 *            none
	 * @return 0 on success, or a negative value (-ENOENT, -EBUSY, -EPERM)
	 */
	public synchronized native int subscribe(String varName);

	/**
	 * Writes to a single EXISTING variable. Can write value, flags or both.
	 * @param var
	 *            Data to write. In particular the .name must be an ASCII
	 *            variable name, less than NAMESIZE characters long
	 * @param doValue
	 *            True if var.value is valid and should be updated
	 * @param doFlags
	 *            True if var.flags is valid and should be updated
	 * @return 0 on success, or a negative value (-ENOENT, -EBUSY, -EPERM,
	 *         -EOVERFLOW)
	 */
	public synchronized native int set(RDBVar var, boolean doValue, boolean doFlags);

	/**
	 * Writes a single EXISTING variable's value.
	 * @param varName
	 *            ASCII variable name, less than NAMESIZE characters long
	 * @param varValue
	 *            value to write - will be converted to (modified) UTF8
	 * @throws RDBException
	 *             on failure (-ENOENT, -EBUSY, -EPERM, -EOVERFLOW)
	 */
	public void setValueE(String varName, String varValue) throws RDBException {
		int res = set(new RDBVar(varName, varValue), true, false);
		if (res < 0) {
			throw new RDBException(this, "setE", -res);
		}
	}

	/**
	 * Creates a new variable in database with given flags and perms. Variable
	 * must NOT exist.
	 * @param var
	 *            Data to write. In particular the .name must be an ASCII
	 *            variable name, less than NAMESIZE characters long
	 * @return 0 on success, or a negative value (-ENOENT, -EBUSY, -EPERM,
	 *         -EOVERFLOW)
	 */
	public synchronized native int create(RDBVar var);

	/**
	 * Creates a new variable in database with 0 flags and perms. Variable must
	 * NOT exist.
	 * @param varName
	 *            ASCII variable name, less than NAMESIZE characters long
	 * @param varValue
	 *            value to write - will be converted to (modified) UTF8
	 * @throws RDBException
	 *             on failure (-ENOENT, -EBUSY, -EPERM, -EOVERFLOW)
	 */
	public void createE(String varName, String varValue) throws RDBException {
		int res = create(new RDBVar(varName, varValue));
		if (res < 0) {
			throw new RDBException(this, "createE", -res);
		}
	}

	/**
	 * Write a variable's value if it already exists or creates a new variable
	 * if it doesn't. If created the flags and perms will also be set, but not
	 * if it exists.
	 * @param var
	 *            Data to write. In particular the .name must be an ASCII
	 *            variable name, less than NAMESIZE characters long
	 * @return 0 if existing, 1 if created or a negative value (-ENOENT, -EBUSY,
	 *         -EPERM, -EOVERFLOW)
	 */
	public synchronized native int update(RDBVar var);

	/**
	 * Write a variable's value if it already exists or creates a new variable
	 * if it doesn't. If created the flags and perms will be set to 0, but not
	 * if it exists.
	 * @param varName
	 *            ASCII variable name, less than NAMESIZE characters long
	 * @param varValue
	 *            value to write - will be converted to (modified) UTF8
	 * @return 0 if existing, 1 if created or a negative value (-ENOENT, -EBUSY,
	 *         -EPERM, -EOVERFLOW)
	 */
	public int updateValue(String varName, String varValue) {
		return update(new RDBVar(varName, varValue));
	}

	/**
	 * Write a variable's value if it already exists or creates a new variable
	 * if it doesn't. If created the flags and perms will be set to 0, but not
	 * if it exists.
	 * @param varName
	 *            ASCII variable name, less than NAMESIZE characters long
	 * @param varValue
	 *            value to write - will be converted to a string
	 * @return 0 if existing, 1 if created or a negative value (-ENOENT, -EBUSY,
	 *         -EPERM, -EOVERFLOW)
	 */
	public int updateValue(String varName, int varValue) {
		return updateValue(varName, Integer.toString(varValue) );
	}

	/**
	 * Write a variable's value if it already exists or creates a new variable
	 * if it doesn't. If created the flags and perms will be set to 0, but not
	 * if it exists.
	 * @param varName
	 *            ASCII variable name, less than NAMESIZE characters long
	 * @param varValue
	 *            value to write - will be converted to (modified) UTF8
	 * @throws RDBException
	 *             on failure (-ENOENT, -EBUSY, -EPERM, -EOVERFLOW)
	 */
	public int updateValueE(String varName, String varValue) throws RDBException {
		int res = update(new RDBVar(varName, varValue));
		if (res < 0) {
			throw new RDBException(this, "updateE", -res);
		}
		return res;
	}

	/**
	 * Write a variable's value if it already exists or creates a new variable
	 * if it doesn't. If created the flags and perms will be set to 0, but not
	 * if it exists.
	 * @param varName
	 *            ASCII variable name, less than NAMESIZE characters long
	 * @param varValue
	 *            value to write - will be converted to a string
	 * @throws RDBException
	 *             on failure (-ENOENT, -EBUSY, -EPERM, -EOVERFLOW)
	 */
	public int updateValueE(String varName, int varValue) throws RDBException {
		return updateValueE(varName, "" + varValue);
	}

	/**
	 * Reads a single EXISTING variable from the database. NOTE: var is used for
	 * both input and output - the .name field is used for lookup and then the
	 * other fields are filled with data.
	 * @param var
	 *            Must have .name set to an ASCII variable name (less than
	 *            NAMESIZE characters long), other fields are updated in method
	 * @return 0 on success, or a negative value (-ENOENT, -EBUSY, -EPERM,
	 *         -ENOMEM)
	 */
	public synchronized native int get(RDBVar var);

	/**
	 * Reads a single EXISTING variable from the database.
	 * @param varName
	 *            ASCII variable name, less than NAMESIZE characters long
	 * @return retrieved data
	 * @throws RDBException
	 *             on failure (-ENOENT, -EBUSY, -EPERM, -ENOMEM)
	 */
	public RDBVar getE(String varName) throws RDBException {
		RDBVar v = new RDBVar(varName);
		int res = get(v);
		if (res < 0) {
			throw new RDBException(this, "getE", -res);
		}
		return v;
	}

	/**
	 * Reads a single variable from the database, or the given default value if
	 * it doesn't exist or some other error occurs.
	 * @param var
	 *            ASCII variable name, less than NAMESIZE characters long
	 * @return The read value or the default value
	 */
	public String getStringDef(String varName, String def) {
		// System.out.println("Java getStringDef");
		RDBVar v = new RDBVar(varName);
		int res = get(v);
		if (res < 0) {
			return def;
		}
		return v.getString();
	}

	/**
	 * Reads a single variable from the database, or the given default value if
	 * it doesn't exist, it's not an integer or some other error occurs.
	 * @param var
	 *            ASCII variable name, less than NAMESIZE characters long
	 * @return The read value or the default value
	 */
	public int getIntDef(String varName, int def) {
		RDBVar v = new RDBVar(varName);
		int res = get(v);
		if (res < 0) {
			return def;
		}
		return v.getIntDef(def);
	}

	/**
	 * Writes a single variable's flags in database.
	 * @param varName
	 *            ASCII variable name, less than NAMESIZE characters long
	 * @param nFlags
	 *            flags to write
	 * @throws RDBException
	 *             on failure (-ENOENT, -EBUSY, -EPERM, -EOVERFLOW)
	 */
	public void setFlagsE(String varName, int nFlags) throws RDBException {
		int res = set(new RDBVar(varName, new byte[0], nFlags, 0), false, true);
		if (res < 0) {
			throw new RDBException(this, "setE", -res);
		}
	}

	/**
	 * Read a single variable's flags from the database.
	 * @param varName
	 *            ASCII variable name, less than NAMESIZE characters long
	 * @return The flags
	 * @throws RDBException
	 *             on failure (-ENOENT, -EBUSY, -EPERM, -EOVERFLOW)
	 */
	public int getFlagsE(String varName) throws RDBException {
		return getE(varName).flags;
	}

	/**
	 * Deletes an existing variable from database.
	 * @param varName
	 *            ASCII variable name, less than NAMESIZE characters long
	 * @return 0 on success, or a negative value (-ENOENT, -EBUSY, -EPERM,
	 *         -EOVERFLOW)
	 */
	public synchronized native int delete(String varName);

	/**
	 * Acquires the database lock. Any RDB operations between lock and unlock
	 * are atomic for other RDB users. Be sure to call unlock() ASAP.
	 * 
	 * <b>WARNING: Do not perform any extensive computation or blocking
	 * operations (e.g. file access) while the lock is held.</b> The driver may
	 * kill the process if the lock is held for an excessive amount of time, and
	 * that will take out the whole Java VM!
	 * @param nFlag
	 *            Database flags. e.g. NONBLOCK
	 * @return 0 on success, or a negative value (-EBUSY)
	 */
	public synchronized native int lock();

	/**
	 * Release the database lock obtained with lock().
	 */
	public synchronized native void unlock();

	/**
	 * Open session to RDB - must be called before any other native calls.
	 * Currently called from constructors.
	 * @param dev
	 *            Device file or "" for default
	 * @return Negative value on error
	 */
	synchronized native int openLib(String dev);

	/**
	 * Close RDB session - must not make any more native calls after this.
	 */
	public synchronized native void closeLib();

	/**
	 * does a single select() for asynchronous callbacks. This may block - that
	 * also means it can't be synchronized.
	 * @return 0 if data is available
	 */
	native int doSelect(int seconds, int microseconds);

	
}
