package com.netcommwireless.javardb;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;

/**
 * Implements RDB subscriptions with a callback mechanism.
 * This uses generics so it requires at least Java 1.5 - the other classes work fine with Java 1.4.
 * It should be possible to make it backward compatible with 'javac -source 1.5 -target jsr14'
 */
public class AsyncRDB extends RDB {
	HashMap<String, RDBSubs> subscribed = new HashMap<String, RDBSubs>();
	Thread selectThread;
	boolean selectRunning = true;
	/**
	 * Create session using default device. Equivalent to AsyncRDB("")
	 * @throws RDBException
	 *             if session can't be created
	 */
	public AsyncRDB() throws RDBException {
		this("");
	}

	/**
	 * Create RDB session using given device and start select loop.
	 * @param dev
	 *            Path to device file, "" means default
	 * @throws RDBException
	 *             if session can't be created
	 */
	public AsyncRDB(String dev) throws RDBException {
		super(dev);
		selectThread = new Thread() { public void run() { selectLoop(); } };
		selectThread.start();
	}

	/**
	 * Subscribes for notifications if the given EXISTING variable is written or deleted. 
	 * A process can only subscribe to notifications if the variable is readable by
	 * that process.
	 * <p>
	 * The callback will only be run (at most) once for each write. In the case of
	 * multiple quick writes to a single var it's possible that some of the
	 * intermediate values won't cause callbacks, but the final value always will.
	 * @param varName ASCII variable name, less than NAMESIZE characters long
	 * @param sub Object to be notified when the variable is written
	 * @return 0 on success, or a negative value (-ENOENT, -EBUSY, -EPERM)
	 */
	public synchronized int subscribe(String varName, RDBSubscriber sub) {
		if (sub==null)
			throw new NullPointerException("Async subscribe() needs a valid callback");
		RDBSubs container = subscribed.get(varName);
		if (container == null) {
			container = new RDBSubs();
			RDBVar v = new RDBVar(varName);
			get(v);
			container.last = v;
			container.subs = new ArrayList<RDBSubscriber>(1);
			subscribed.put(varName, container);
		}
		container.subs.add(sub);
		return super.subscribe(varName);
	}

	/**
	 * Undo a previous call to subscribe() with the given varName and sub.
	 * If there's no match with a previous call nothing is done.
	 * If there's more than one match, only one of them is removed.
	 * <p>
	 * PERFORMANCE NOTE: technically the underlying library has no unsubscibe;
	 * this method simply removes the mapping to a callback, which causes 
	 * the notification to be ignored.
	 * @param varName ASCII variable name, less than NAMESIZE characters long
	 * @param sub Object to be notified when the variable is written
	 */
	public synchronized void unsubscribe(String varName, RDBSubscriber sub) {
		RDBSubs container = subscribed.get(varName);
		if (container != null) {
			container.subs.remove(sub);
		}
	}

	/**
	 * Undo all previous calls to subscribe().
	 * <p>
	 * PERFORMANCE NOTE: technically the underlying library has no unsubscibe;
	 * this method simply removes the mapping to the callbacks, which causes 
	 * the notification to be ignored.
	 * @param varName ASCII variable name, less than NAMESIZE characters long
	 * @param sub Object to be notified when the variable is written
	 */
	public synchronized void unsubscribeAll(String varName) {
		subscribed.remove(varName);
	}

	/**
	 * NOT AVAILABLE - will throw Error().
	 * Use RDB instead if you want this interface.
	 */
	@Override
	public RDBVar[] waitForTriggers(long milliseconds) {
		throw new Error("Use callbacks or RDB instead.");
	}
	
	/**
	 * NOT AVAILABLE - will throw Error().
	 * Use RDB instead if you want this interface.
	 */
	@Override
	public synchronized int subscribe(String varName) {
		throw new Error("Use callbacks or RDB instead.");
	}
	
	/**
	 * Stop select loop then close session.
	 */
	@Override
	public synchronized void closeLib() {
		synchronized (selectThread) {
			selectRunning = false;
			try {
				selectThread.wait();
			} catch (InterruptedException e) {
			}
		}
		super.closeLib();
	}
	
	/**
	 * Wait for notifications for subscribed variables and run callbacks.
	 * Won't return until closeLib() is called - call from a new thread.
	 */
	private void selectLoop() {
		while (selectRunning) {
			if (doSelect(0, 500) > 0) {
				for (String vn : getNames("", TRIGGERED, TRIGGERED)) {
					RDBSubs found = subscribed.get(vn);
					if (found == null)
						continue;
					byte[] old = found.last.value;
					found.last.value = null;
					get(found.last);
					boolean hasChanged = (found.last.value!=null) && !Arrays.equals(old, found.last.value);
					for (RDBSubscriber sub : found.subs ) {
						sub.callback(found.last, old, hasChanged);
					}
				}
			}
		}
		synchronized (selectThread) {
			selectThread.notify();
		}
	}
	
	/**
	 * Structure for the map
	 * @author bill
	 *
	 */
	private class RDBSubs {
		RDBVar last;
		ArrayList<RDBSubscriber> subs;
	}

}
