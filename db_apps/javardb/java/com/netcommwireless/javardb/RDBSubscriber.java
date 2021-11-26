package com.netcommwireless.javardb;

/**
 * Used as a callback when a subscribed variable changes. Use AsyncRDB.subscribe()
 * and AsyncRDB.unsubscribe() to manage callbacks.
 * <p>
 * Each RDB object runs subscribed callbacks sequentially (though in an inderterminate
 * order), so if there's only 1 RDB object in an application it's impossible for more
 * than 1 to be running at a time. You wont need synchronisation if all your
 * application does is wait for a callback, but be careful if you have other threads
 * working in the background and you access their objects.
 * <p>
 * The callback will only be run (at most) once for each write. In the case of
 * multiple quick writes to a single var it's possible that some of the
 * intermediate values won't cause callbacks, but the final value always will.
 * @author Bill Bennett william.bennett@netcommwireless.com
 *
 */
public interface RDBSubscriber {
	/**
	 * Called when a subscribed variable changes.
	 * @param newVar The variable that changed, filled out with the new value.
	 * @param oldValue The variable's last known value - not necessarily the most
	 * 					recent value - see discussion in the interface desc
	 * @param hasChanged True if the old value and new value differ
	 */
	public void callback(RDBVar newVar, byte[] oldValue, boolean hasChanged);
}
