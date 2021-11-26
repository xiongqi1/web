package com.netcommwireless;

import java.util.Arrays;

import com.netcommwireless.javardb.*;

/**
 * Used to test the package com.netcommwireless.javardb.
 * Does not need to be included on target.
 * @author Bill Bennett william.bennett@netcommwireless.com
 *
 */
public class TestRDB implements RDBSubscriber {

	RDB testObj;
	int unchanged, changed;
	RDBVar lastCallback = null;
	StringBuffer results = new StringBuffer();
	
	public TestRDB() {
		try {
			testObj = new RDB();
		} catch (RDBException e) {
			e.printStackTrace();
			System.exit(1);
		}
	}

	// NOTE: @Override for interfaces was (officially) added in V1.7 - if this gives you an error just delete the annotation
	@Override
	public synchronized void callback(RDBVar newVar, byte[] oldValue, boolean hasChanged) {
		if (hasChanged)
			changed++;
		else
			unchanged++;
		lastCallback = newVar;
		results.append(String.format("%d-%d: '%s' was '%s'\n", unchanged, changed, newVar.toString(), new String(oldValue)));
	}

	/**
	 * Used to test java-rdb functionality - first arg is the mode, others
	 * depend on which mode. NOTE: because this is only for testing it doesn't
	 * bother to check its arguments - if you get it wrong Java will throw an
	 * exception. Use no arguments for help
	 * @param args
	 */
	public static void main(String[] args) {
		TestRDB tpo = new TestRDB();
		if (args.length == 0) {
			System.out.println("Need test name and args, exit status 0 if test OK. E.g.:");
			System.out.println("read <varname>");
			System.out.println("set <varname> <value>");
			System.out.println("update <varname> <value>");
			System.out.println("setcreate <var1> <var2>");
			System.out.println("find <string>");
			System.out.println("benchmark <varname>");
			System.out.println("wait <varname>");
			System.out.println("threads <var1> <var2>");
			System.out.println("crossthread <var1> <var2>");
			System.out.println("closed <varname>");
		}
		else if (args[0].equalsIgnoreCase("read"))
			tpo.runRead(args);
		else if (args[0].equalsIgnoreCase("set"))
			tpo.runSet(args);
		else if (args[0].equalsIgnoreCase("create"))
			tpo.runCreate(args);
		else if (args[0].equalsIgnoreCase("setcreate"))
			tpo.runSetCreate(args);
		else if (args[0].equalsIgnoreCase("find"))
			tpo.runFind(args);
		else if (args[0].equalsIgnoreCase("benchmark"))
			tpo.runBenchmark(args);
		else if (args[0].equalsIgnoreCase("wait"))
			tpo.runWait(args);
		else if (args[0].equalsIgnoreCase("threads"))
			tpo.runThreads(args);
		else if (args[0].equalsIgnoreCase("crossthread"))
			tpo.runCrossthread(args);
		else if (args[0].equalsIgnoreCase("closed"))
			tpo.runClosed(args);
		else {
			System.out.println("Test not implemented");
			System.exit(1);
		}
	}
	
	
	private void runRead(String[] args) {
		System.out.println("Java read: " + testObj.getStringDef(args[1], "mydefault"));
	}
	private void runSet(String[] args) {
		System.out.println("Set returns: " + testObj.set(new RDBVar(args[1], args[2].getBytes()), true, false));
	}
	private void runCreate(String[] args) {
		System.out.println("Create returns: " + testObj.create(new RDBVar(args[1], args[2].getBytes())));
	}
	private void runFind(String[] args) {
		System.out.println("Java read: " + Arrays.asList(testObj.getVars(args[1], 0, 0)));
	}
	private void runBenchmark(String[] args) {
		int count = Integer.parseInt(args[1]);
		long start = System.currentTimeMillis();
		System.out.println("Doing " + count + " reads");
		for (int i = 0; i < count; i++) {
			testObj.getStringDef("wwan.0.radio.information.signal_strength", "none");
		}
		long delay = System.currentTimeMillis() - start;
		System.out.println("Done in " + delay + "ms, " + 1000 * count / delay + "/s");
		System.out.println("Doing " + count + " writes");
		start = System.currentTimeMillis();
		for (int i = 0; i < count; i++) {
			testObj.updateValue(args[2], "none");
		}
		delay = System.currentTimeMillis() - start;
		System.out.println("Done in " + delay + "ms, " + 1000 * count / delay + "/s");
		AsyncRDB async;
		try {
			async = new AsyncRDB();
		} catch (RDBException e) {
			e.printStackTrace();
			System.exit(1);
			return;
		}
		async.subscribe(args[2], this);
		RDBVar now = new RDBVar(args[2]);
		now.setString("unchanged");
		changed = unchanged = 0;
		start = System.currentTimeMillis();
		int cutover = count / 3;
		System.out.println("Doing " + cutover + " unchanging writes/async reads");
		for (int i = 0; i < cutover; i++) {
			testObj.set(now, true, false);
		}
		System.out.println("Doing " + (count-cutover) + " changing writes/async reads");
		for (int i = cutover; i < count; i++) {
			now.setString("i="+i);
			testObj.set(now, true, false);
		}
		delay = System.currentTimeMillis() - start;
		try {Thread.sleep(100);	} catch (InterruptedException e) {}
		synchronized (this) {
			System.out.println("Sent in " + delay + "ms, " + 1000 * count / delay + "/s; unchanged=" + unchanged + " changed=" + changed);
		}
		async.closeLib();
		
		System.out.print(results);
		if (!Arrays.equals(lastCallback.value, ("i="+(count - 1)).getBytes())) {
			System.out.println("ERROR: Last callback had wrong value");
			System.exit(1);
		}
	}
	private void runWait(String[] args) {
		testObj.subscribe(args[1]);
		System.out.println(((args.length > 2) ? "Java read: " : args[2]) + Arrays.asList(testObj.waitForTriggers(30000)));
	}
	private void runThreads(String[] args) {
		// false means they don't wait before running
		CrossThread t1 = new CrossThread(false, args[1], "Thread 1:");
		CrossThread t2 = new CrossThread(false, args[2], "Thread 2:");
		// not actually crossed!
		testThreads(args, t1, t2);
	}
	private void runCrossthread(String[] args) {
		// true means they wait to be told what to do
		CrossThread t1 = new CrossThread(true, args[1], "Thread 1:");
		CrossThread t2 = new CrossThread(true, args[2], "Thread 2:");
		// cross them - point each at the other before starting and wait for
		// them to subscribe
		synchronized (t1) {	synchronized (t2) {
			t1.other = t2;
			t2.other = t1;
			t1.start();
			t2.start();
			try {
				t1.wait();
				t2.wait();
			} catch (InterruptedException e) {
				e.printStackTrace();
				System.exit(1);
			}
			// at this point we know that both threads have subscribed.
			// tell each thread the other is ready
			t1.notify();
			t2.notify();
		}}
		testThreads(args, t1, t2);
	}
	private void runClosed(String[] args) {
		testObj.updateValue(args[1], "OK: OPEN");
		System.out.println("Java read: " + testObj.getStringDef(args[1], "ERROR"));
		testObj.updateValue(args[1], "ERROR: OPEN");
		testObj.closeLib();
		System.out.println("Java read: " + testObj.getStringDef(args[1], "OK: CLOSED"));
		testObj.closeLib();
		try {
			System.out.println("ERROR - this should have thrown an error, instead it read: " + testObj.getE(args[1]));
			System.exit(1);
		} catch (RDBException e) {
			System.out.println("TEST OK (supposed to throw error): " + e.toString());
		}
	}
	private void runSetCreate(String[] args) {
		
		System.out.println("First update returns: " + testObj.updateValue(args[1], "Val1"));
		System.out.println("Second update returns: " + testObj.updateValue(args[2], "Val2"));
		System.out.println("Delete returns: " + testObj.delete(args[2]));
		System.out.println("First set returns: " + testObj.set(new RDBVar(args[1], "Val1again".getBytes()), true, false));
		System.out.println("Second set returns: " + testObj.set(new RDBVar(args[2], "Val2again".getBytes()), true, false));
		System.out.println("Vars are now: " + testObj.getStringDef(args[1], "GONE") + ", " + testObj.getStringDef(args[2], "GONE"));
		System.out.println("First create returns: " + testObj.create(new RDBVar(args[1], "Val1final".getBytes())));
		System.out.println("Second create returns: " + testObj.create(new RDBVar(args[2], "Val2final".getBytes())));
		System.out.println("Vars are now: " + testObj.getStringDef(args[1], "GONE") + ", " + testObj.getStringDef(args[2], "GONE"));
		testObj.delete(args[1]);
		testObj.delete(args[2]);
	}
	

	private void testThreads(String[] args, CrossThread t1, CrossThread t2) {
		// small delay so we can see they actually block
		try {
			Thread.sleep(2000);
		} catch (InterruptedException e) {
			e.printStackTrace();
			System.exit(1);
		}
		if ((t1.pref == null) || (t2.pref == null)) {
			System.out.println("One of the threads returned too early");
			System.exit(1);
		}
		if (testObj.set(new RDBVar(args[2], "ValB".getBytes()), true, false) < 0) {
			System.out.println("Failed to set");
			System.exit(1);
		}
		try {
			Thread.sleep(1000);
		} catch (InterruptedException e) {
			e.printStackTrace();
			System.exit(1);
		}
		if ((t1.pref != null) || (t2.pref == null)) {
			System.out.println("Notification failed");
			System.exit(1);
		}
		testObj.delete(args[1]);
	}

	private class CrossThread extends Thread {
		CrossThread other;
		RDB ses;
		String var, pref;
		boolean cross;

		public CrossThread(boolean cross, String var, String pref) {
			super();
			this.cross = cross;
			this.var = var;
			this.pref = pref;
			if (!cross) {
				other = this;
				start();
			}
		}

		public void run() {
			try {
				ses = new RDB();
				ses.subscribe(var);
				if (cross) {
					synchronized (this) {
						notify(); // tell main that we are ready
						wait(); // for main to tell us it's ready
					}
					// at this point we know that the other thread is ready
				}
				System.out.println(pref + Arrays.asList(other.ses.waitForTriggers(60000)));
				pref = null;
			} catch (Exception e) {
				e.printStackTrace();
			}
		}
	}
}
