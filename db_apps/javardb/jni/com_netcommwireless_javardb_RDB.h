/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class com_netcommwireless_javardb_RDB */

#ifndef _Included_com_netcommwireless_javardb_RDB
#define _Included_com_netcommwireless_javardb_RDB
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    getNames
 * Signature: (Ljava/lang/String;II)[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL Java_com_netcommwireless_javardb_RDB_getNames
  (JNIEnv *, jobject, jstring, jint, jint);

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    subscribe
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_subscribe
  (JNIEnv *, jobject, jstring);

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    set
 * Signature: (Lcom/netcommwireless/javardb/RDBVar;ZZ)I
 */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_set
  (JNIEnv *, jobject, jobject, jboolean, jboolean);

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    create
 * Signature: (Lcom/netcommwireless/javardb/RDBVar;)I
 */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_create
  (JNIEnv *, jobject, jobject);

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    update
 * Signature: (Lcom/netcommwireless/javardb/RDBVar;)I
 */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_update
  (JNIEnv *, jobject, jobject);

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    get
 * Signature: (Lcom/netcommwireless/javardb/RDBVar;)I
 */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_get
  (JNIEnv *, jobject, jobject);

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    delete
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_delete
  (JNIEnv *, jobject, jstring);

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    lock
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_lock
  (JNIEnv *, jobject);

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    unlock
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_netcommwireless_javardb_RDB_unlock
  (JNIEnv *, jobject);

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    openLib
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_openLib
  (JNIEnv *, jobject, jstring);

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    closeLib
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_netcommwireless_javardb_RDB_closeLib
  (JNIEnv *, jobject);

/*
 * Class:     com_netcommwireless_javardb_RDB
 * Method:    doSelect
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_com_netcommwireless_javardb_RDB_doSelect
  (JNIEnv *, jobject, jint, jint);

#ifdef __cplusplus
}
#endif
#endif