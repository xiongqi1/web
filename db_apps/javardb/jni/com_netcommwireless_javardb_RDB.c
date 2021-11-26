#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/queue.h>

#include "com_netcommwireless_javardb_RDB.h"
#include "rdb_ops.h"

//#define CHAR_BUFF_SIZE 256
/// buffer used to retrieve matching var names from driver
#define GET_NAMES_SIZE 4096
/// character that separates names in that buffer
#define GET_NAMES_SEP_CHAR '&'
/// Gets the RDB session from the java object. Also dereferences the
/// environment pointer to avoid the (*env)-> you normally see.
/// JE - the Java environment passed in (usually env)
/// JP - the name of a var that's created to hold the dereferenced env
/// SP - the name of a var that's created to hold the RDB session
/// IFNULL - what to do if the session is null (usually return)
#define GET_SESSION(JE, JP, SP, IFNULL)  JNIEnv JP = *JE; \
		struct rdb_session *SP = (struct rdb_session *) \
		(intptr_t) JP->GetLongField(env, obj, rdbSession_fid); \
		if (SP==NULL) {IFNULL;}

	/* The FIDs are supposed to be consistent for as long as the class
	 * is loaded so it should be safe to cache it, even if we have
	 * multiple RDB objects
	 * By storing the session pointer in the object's instance data
	 * and using Java's synchronising all native methods using the
	 * the same object, multi-threading issues should be handled.
	 * Unfortunately Java has no datatype for a pointer, so it's
	 * converted to a correctly-sized C int and then to a Java long
	 * (64 bits) - rumours say this how their own libraries do it.
	 */
static jfieldID rdbSession_fid, rdbvar_name_fid, rdbvar_value_fid,
				rdbvar_flags_fid, rdbvar_perms_fid;
/*
struct StringListEntry {
    jstring data;
    LIST_ENTRY(StringListEntry) next;
};*/

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    getNames
 * Signature: (Ljava/lang/String;II)[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL Java_com_netcommwireless_javardb_RDB_getNames
  (JNIEnv *env, jobject obj, jstring key, jint flagsSet, jint flagsClear) {
	GET_SESSION(env, JNI, rdb_session_p, return NULL)

    int i, count, buf_len = GET_NAMES_SIZE, flag_test;
    if (flagsSet == flagsClear) {
    	flag_test = flagsSet;
    } else {
    	flag_test = ((flagsSet | flagsClear) << 16) | flagsSet;
    }

    char name_buf[GET_NAMES_SIZE];
    const char *varname = JNI->GetStringUTFChars(env, key, 0);
    if (varname == NULL)
    	return NULL;

	int res = rdb_getnames(rdb_session_p, varname, name_buf, &buf_len, flag_test);
	JNI->ReleaseStringUTFChars(env, key, varname);

	if (((res < 0) && (res != -ENOENT)) || (buf_len >= GET_NAMES_SIZE)) {
#ifdef DEBUG
		printf("rdb_getnames(%d)=%d\n", buf_len, res);
#endif
		return NULL;
	}
	name_buf[buf_len] = 0;			//just in case

	char *cursor = name_buf;

	if (!*cursor) {					// edge case: no vars returned
		cursor = NULL;
	}

	count = 0;
	while (cursor != NULL) {
		cursor = strchr(cursor + 1, GET_NAMES_SEP_CHAR);
		count++;
	}
#ifdef DEBUG
	printf("Found %d vars for %08x\n", count, flag_test);
#endif
	jobjectArray valout = JNI->NewObjectArray(env, count,
			JNI->GetObjectClass(env, key), NULL);

	cursor = name_buf;
	for (i=0; i < count; i++) {
		char *next = strchr(cursor, GET_NAMES_SEP_CHAR);
		if (next != NULL)		// if not the last one
			*next = 0;
		JNI->SetObjectArrayElement(env, valout, i,
				JNI->NewStringUTF(env, cursor));
		cursor = next + 1;		// if next is null the loop will exit anyway
	}
	return valout;
}

  /*
   * Class:     com_netcommwireless_javardb_RDB
   * Method:    subscribe
   * Signature: (Ljava/lang/String;)I
   */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_subscribe
  (JNIEnv *env, jobject obj, jstring var_name) {
	GET_SESSION(env, JNI, rdb_session_p, return -EFAULT)

	const char *var_chars = JNI->GetStringUTFChars(env, var_name, 0);
	int res = rdb_subscribe(rdb_session_p, var_chars);
#ifdef DEBUG
	printf("Subscribe res %d to %s\n", res, var_chars);
#endif
	JNI->ReleaseStringUTFChars(env, var_name, var_chars);
	return res;
}

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    set
 * Signature: (Lcom/netcommwireless/javardb/RDBVar;ZZ)I
 */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_set
  (JNIEnv *env, jobject obj, jobject var, jboolean doVal, jboolean doFlags) {
	GET_SESSION(env, JNI, rdb_session_p, return -EFAULT)

	int res1 = 1;
	jstring var_name = (jstring) JNI->GetObjectField(env, var, rdbvar_name_fid);
	const char *var_chars = JNI->GetStringUTFChars(env, var_name, 0);
	if (doVal) {
		jbyteArray val = (jbyteArray) JNI->GetObjectField(env, var, rdbvar_value_fid);
		jbyte *chars = JNI->GetByteArrayElements(env, val, NULL);
		res1 = rdb_set(rdb_session_p, var_chars, (char *) chars,
				JNI->GetArrayLength(env, val));
		// JNI_ABORT because we didn't actually change anything
		JNI->ReleaseByteArrayElements(env, val, chars, JNI_ABORT);
	}
	if (doFlags) {
		int res2 = rdb_setflags(rdb_session_p, var_chars,
				JNI->GetIntField(env, var, rdbvar_flags_fid));
		if (res2 < res1)
			res1 = res2;
	}
	JNI->ReleaseStringUTFChars(env, var_name, var_chars);
	return res1;
}

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    create
 * Signature: (Lcom/netcommwireless/javardb/RDBVar;)I
 */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_create
  (JNIEnv *env, jobject obj, jobject var) {
	GET_SESSION(env, JNI, rdb_session_p, return -EFAULT)

	jstring var_name = (jstring) JNI->GetObjectField(env, var, rdbvar_name_fid);
	const char *var_chars = JNI->GetStringUTFChars(env, var_name, 0);
	jbyteArray val = (jbyteArray) JNI->GetObjectField(env, var, rdbvar_value_fid);
	jbyte *chars = JNI->GetByteArrayElements(env, val, NULL);
	int res = rdb_create(rdb_session_p, var_chars,
			(char *) chars, JNI->GetArrayLength(env, val),
			JNI->GetIntField(env, var, rdbvar_flags_fid),
			JNI->GetIntField(env, var, rdbvar_perms_fid));
	// JNI_ABORT because we didn't actually change anything
	JNI->ReleaseByteArrayElements(env, val, chars, JNI_ABORT);
	JNI->ReleaseStringUTFChars(env, var_name, var_chars);
	return res;
}

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    update
 * Signature: (Lcom/netcommwireless/javardb/RDBVar;)I
 */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_update
  (JNIEnv *env, jobject obj, jobject var) {
	GET_SESSION(env, JNI, rdb_session_p, return -EFAULT)

	jstring var_name = (jstring) JNI->GetObjectField(env, var, rdbvar_name_fid);
	const char *var_chars = JNI->GetStringUTFChars(env, var_name, 0);
	jbyteArray val = (jbyteArray) JNI->GetObjectField(env, var, rdbvar_value_fid);
	jbyte *chars = JNI->GetByteArrayElements(env, val, NULL);
	int res = rdb_update(rdb_session_p, var_chars,
			(char *) chars, JNI->GetArrayLength(env, val),
			JNI->GetIntField(env, var, rdbvar_flags_fid),
			JNI->GetIntField(env, var, rdbvar_perms_fid));
	// JNI_ABORT because we didn't actually change anything
	JNI->ReleaseByteArrayElements(env, val, chars, JNI_ABORT);
	JNI->ReleaseStringUTFChars(env, var_name, var_chars);
	return res;
}

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    get
 * Signature: (Lcom/netcommwireless/javardb/RDBVar;)I
 */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_get
  (JNIEnv *env, jobject obj, jobject var) {
	GET_SESSION(env, JNI, rdb_session_p, return -EFAULT)

	int size, alloc, flags, perms=0;
	jbyteArray valout = NULL;
	jstring var_name = (jstring) JNI->GetObjectField(env, var, rdbvar_name_fid);
	const char *var_chars = JNI->GetStringUTFChars(env, var_name, 0);
	int res = rdb_getinfo(rdb_session_p, var_chars, &alloc, &flags, &perms);
	//printf("Var name: %s, res %d, size %d\n", var_chars, res, alloc);
	// if success get the value
	if (((res >= 0) || (res == -EOVERFLOW)) && (alloc > 0)) {
		jbyte * chars = NULL;
		while (1) {
			size = alloc;
			valout = JNI->NewByteArray(env, alloc);
			if (valout == NULL) {
				res = -ENOMEM;
				break;
			}
			chars = JNI->GetByteArrayElements(env, valout, NULL);
			if (chars == NULL) {
				res = -ENOMEM;
				break;
			}
			res = rdb_get(rdb_session_p, var_chars, (char *) chars, &size);
			if (res == -EOVERFLOW) {
				/* if it grew, double the size of the buffer
				 * This will probably lead to an extra allocation after the loop,
				 * but that's better than the worst case if it grows by one byte
				 * every time.
				 */
				if (size <= alloc)	// don't think this is possible
					break;
				JNI->ReleaseByteArrayElements(env, valout, chars, 0);
				chars = NULL;
				alloc = size * 2;
			}
			else
				break;
		}
		if ((res >= 0) && (size < alloc)) {
			/* If it shrank then reallocate and copy the start of the original.
			 * Java will garbage collect the old array because we never
			 * created a global ref
			 */
			jbyteArray realloc = JNI->NewByteArray(env, size);
			jbyte * realloc_chars = JNI->GetByteArrayElements(env, valout, NULL);
			memcpy(realloc_chars, chars, size);
			JNI->ReleaseByteArrayElements(env, valout, chars, 0);
			chars = realloc_chars;
			valout = realloc;
		}
		if (chars != NULL) {
			JNI->ReleaseByteArrayElements(env, valout, chars, 0);
		}
	}
#ifdef DEBUG
	printf("DONE: %s, res %d, size %d\n", var_chars, res, size);
#endif
	JNI->ReleaseStringUTFChars(env, var_name, var_chars);

	// java code will deal with any errors
	if (res < 0) {
		return res;
	}

	// update the passed-in RDBVar with the new array and flags
	JNI->SetObjectField(env, var, rdbvar_value_fid, valout);
	JNI->SetIntField(env, var, rdbvar_flags_fid, flags);
	JNI->SetIntField(env, var, rdbvar_perms_fid, perms);
	return 0;
}

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    delete
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_delete
  (JNIEnv *env, jobject obj, jstring var_name) {
	GET_SESSION(env, JNI, rdb_session_p, return -EFAULT)

	const char *var_chars = JNI->GetStringUTFChars(env, var_name, 0);
	int res = rdb_delete(rdb_session_p, var_chars);
	JNI->ReleaseStringUTFChars(env, var_name, var_chars);
	return res;
}

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    lock
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_lock
  (JNIEnv *env, jobject obj) {
	GET_SESSION(env, JNI, rdb_session_p, return -EFAULT)

	// Should this be NONBLOCK? Doesn't seem to have much effect anyway...
	return rdb_lock(rdb_session_p, 0);
}

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    unlock
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_netcommwireless_javardb_RDB_unlock
  (JNIEnv *env, jobject obj) {
	GET_SESSION(env, JNI, rdb_session_p, return)

	rdb_unlock(rdb_session_p);
}

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    rdb_open
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_openLib
  (JNIEnv *env, jobject obj, jstring dev) {
	GET_SESSION(env, JNI, rdb_session_p, /*NOT AN ERROR*/)
	// NOTE: at this point rdb_session_p is not set!
	int res;

	// Open the database. If dev == "" then pass NULL
	if (JNI->GetStringLength(env, dev) > 0) {
		const char *nativeDev = JNI->GetStringUTFChars(env, dev, 0);
#ifdef DEBUG
		printf("opening rdb dev '%s'...", nativeDev);
#endif
		res = rdb_open(nativeDev, &rdb_session_p);
		JNI->ReleaseStringUTFChars(env, dev, nativeDev);
	} else {
#ifdef DEBUG
		printf("opening default rdb dev...");
#endif
		res = rdb_open(NULL, &rdb_session_p);
	}
	// java code will deal with any errors
	if (res < 0) {
		//printf("fail%d\n", res);
		return res;
	}
#ifdef DEBUG
	int fd = rdb_fd(rdb_session_p);
	printf("success %p, %d\n", rdb_session_p, fd);
#endif

	// doc for rdbSession_fid at top of file is useful for rest of this

	// we need to set several items in 'this' object and cache fid
	jclass rdb = JNI->GetObjectClass(env, obj);
	rdbSession_fid = JNI->GetFieldID(env, rdb, "rdbSession", "J");
	//printf("rs is %s\n", (rdbSession_fid==NULL)?"null":"OK");
	// store the session pointer in this object instance
	JNI->SetLongField(env, obj, rdbSession_fid, (jlong) (intptr_t) rdb_session_p );
	// set the error codes to be the same as our header in case they change
	jfieldID ENOENT_fid = JNI->GetStaticFieldID(env, rdb, "ENOENT", "I");
	JNI->SetStaticIntField(env, rdb, ENOENT_fid, ENOENT );
	jfieldID EBUSY_fid = JNI->GetStaticFieldID(env, rdb, "EBUSY", "I");
	JNI->SetStaticIntField(env, rdb, EBUSY_fid, EBUSY );
	jfieldID EPERM_fid = JNI->GetStaticFieldID(env, rdb, "EPERM", "I");
	JNI->SetStaticIntField(env, rdb, EPERM_fid, EPERM );
	jfieldID EOVERFLOW_fid = JNI->GetStaticFieldID(env, rdb, "EOVERFLOW", "I");
	JNI->SetStaticIntField(env, rdb, EOVERFLOW_fid, EOVERFLOW );
	jfieldID EFAULT_fid = JNI->GetStaticFieldID(env, rdb, "EFAULT", "I");
	JNI->SetStaticIntField(env, rdb, EFAULT_fid, EFAULT );
	// much easier to do a find than via fields
	jclass rdbvar = JNI->FindClass(env, "com/netcommwireless/javardb/RDBVar");
	// more caching
	rdbvar_name_fid = JNI->GetFieldID(env, rdbvar, "name", "Ljava/lang/String;");
	rdbvar_value_fid = JNI->GetFieldID(env, rdbvar, "value", "[B");
	rdbvar_flags_fid = JNI->GetFieldID(env, rdbvar, "flags", "I");
	rdbvar_perms_fid = JNI->GetFieldID(env, rdbvar, "perms", "I");
	// success
	return 0;
}

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    rdb_close
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_netcommwireless_javardb_RDB_closeLib
  (JNIEnv *env, jobject obj) {
	GET_SESSION(env, JNI, rdb_session_p, return )

	rdb_close(&rdb_session_p);
	JNI->SetLongField(env, obj, rdbSession_fid, (jlong) (intptr_t) NULL);
}

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    doSelect
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_doSelect
  (JNIEnv *env, jobject obj, jint seconds, jint microseconds) {
	GET_SESSION(env, JNI, rdb_session_p, return -EFAULT)

	fd_set fdr;
	int fd=rdb_fd(rdb_session_p);
	struct timeval timeout = { .tv_sec = seconds, .tv_usec = microseconds};
	FD_ZERO(&fdr);
	FD_SET(fd, &fdr);
#ifdef DEBUG
	printf("Calling select with fd %d tout %d, %d\n", fd, seconds, microseconds);
#endif
	return select(fd+1, &fdr, NULL, NULL, &timeout);
}
