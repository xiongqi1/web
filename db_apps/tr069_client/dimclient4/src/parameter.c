/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include <stdio.h>
#include <dirent.h>
#include <string.h>

#include "globals.h"
#include "utils.h"
#include "parameter.h"
#include "paramaccess.h"
#include "eventcode.h"
#include "parameterStore.h"

#include "luaParameter.h"

#define NO_DEFAULT_PARAMETER(e)     (e->type < DefStringType )
#define IS_DEFAULT_PARAMETER(e) 	(e->type >= DefStringType )
#define IS_OBJECT(e)                (e->type == DefaultType || \
                                     e->type == ObjectType || \
                                     e->type == MultiObjectType )
#define IS_CHAR_PTR_TYPE(e)        ((e->type == StringType || e->type == Base64Type )  \
                                  ||(e->type == DefStringType || e->type == DefBase64Type ))

#ifdef EXTENDED_BOOLEAN
static int TRUE_VALUE = 1;
static int FALSE_VALUE = 0;
#endif

// store the value bootstrap flag
// for usage in initial parameter load.
//
static bool paramBootstrap = false;

// flag to handle new parameters during an add object call
// used in copy
static bool inAddObject = false;

typedef union
	{
		char *cval;	/* Pointer  Characters, Bytes and Base64 */
		unsigned int uival;	/* Value    unsigned Integers */
		int ival;				/* Value    Integers and Boolean */
		time_t tval;			/* Value    Date times */
	} ParamValue;

/** \brief       Structure for storing a Parameter
*/
typedef struct parameterEntry
{
	bool isLeaf;                      /*  true = leaf      false = node */
	ParameterType type;               /*  kind of Parameter see enum ParameterType */
	unsigned int instance;           /*  instance number default = 0 */
	unsigned int noChilds;           /*  number of children in this object */
	char name[MAX_PARAM_PATH_LENGTH];
	AccessType writeable;             /*  AccessTyp readOnly,writeOnly, readWrite  */
	NotificationType notification;    /*  0 = off ( Default ) 1 = Passive 2 = Active 3 = Allays */
	NotificationMax notificationMax; /*  0 = off ( Default ) 1 = Passive 2 = Active 3 = Allays 4 = NotChangeable by ACS */
	RebootType rebootOnChange;        /*  Reboot the System on change of value */
	StatusType status;
        /* Access functions to get / set data value */
	int initDataIdx;
	int getDataIdx;
	int setDataIdx;
	/* Because we have to store different kind of values
	 * // use a union
	 */
	ParamValue value;

	char **accessList;                   /*! Access list Empty = ACS only */
	int accessListSize;
	/* Build a double linked list */
	struct parameterEntry *prev;
	struct parameterEntry *next;
	struct parameterEntry *parent;
	struct parameterEntry *child;                /*! Only used if type is Object */
	struct parameterEntry *defaultChild;   /*! Link to the default Object for the MultiObject */
} ParameterEntry;

/**     Local defined functions        */
static	ParameterEntry *findParamInObjectByName (const ParameterEntry *, const char *);
static ParameterEntry *findParameterByPath (const char *);
static int traverseParameter (ParameterEntry * entry, bool nextLevel, int (*func) (ParameterEntry *, void *), void *);
static int traverseParameterReverse (ParameterEntry * entry, bool nextLevel, int (*func) (ParameterEntry *, void *), void *);
static int setParametersAttributesHelper (ParameterEntry *, void *);
static int newParam1 (char *, char *);
static int newParam2 (char *, char *);

static int 	addInformParameterValueStructHelper (struct ArrayOfParameterValueStruct* , ParameterEntry*,int *, bool *);
static char 	*getPathname (const ParameterEntry *);
static void 	getParentPath (ParameterEntry *, char **);
static int 	insertParameter (const char *, ParameterEntry *);
static int 	insertParameterInObject (ParameterEntry *, ParameterEntry *);
static bool 	isObject (const char *);
static int 	newParameter( const char *name, ParameterType , int , RebootType, NotificationType, NotificationMax, int, int, int, char *, ParamValue *, const bool);
static int 	deleteParameter (ParameterEntry *, void *);
static int 	printParameter (ParameterEntry *, void *);
static int 	printParameterName (ParameterEntry *, void *);
static int 	saveParameter (ParameterEntry *, void *);
static int 	saveMetaParameter (ParameterEntry *, void *);
static int 	callInitFunction( ParameterEntry *, void *);
static int 	callDeleteFunction( ParameterEntry *, void *);
static bool 	compareValue ( ParameterEntry *, void * );
static void 	freeAccessList (ParameterEntry *);
static int 	copyAccessList (ParameterEntry *, int, char **);
static int 	countPieces (char *);
static int 	copyObject (ParameterEntry *, ParameterEntry *, const char *);
static int 	copyParameter (ParameterEntry *, ParameterEntry *);
static char 	*getName (const char *);
static char 	*getParentName (const char *);
static int 	setAccess2ParamValue (ParameterEntry *, ParameterValue *);
static int 	setValuePtr (ParameterEntry *, void *);
static void 	*getValue (ParameterEntry *);
static char 	*getValueAsString (ParameterEntry * );
static void 	setVoidPtr2Access( const ParameterType, void *, ParameterValue *);
static void   	setParamValue2Access( const ParameterType, ParamValue *, ParameterValue *);
static void 	setAccess2VoidPtr( const ParameterType, void *, ParameterValue *);
static int 	setParameterStatus (ParameterEntry *, void *);
static int 	countChilds (ParameterEntry *);
static int traverseTreeAndCheckPassiveNotification(ParameterEntry * entry);
static int traverseTreeAndCheckActiveNotification(ParameterEntry * entry);

/*Variables to hold the pointer of the parameter list
*/
static ParameterEntry rootEntry;
static ParameterEntry *firstParam = &rootEntry;

// Total number of parameters
static int paramCounter = 0;

// EmptyString as a return value of an empty access array
static char *emptyString = "";

extern unsigned int sendActiveNofi;

/** Creates a copy of the object named in paramName.
 *  The object must be an multi-instance object ( type = MulitObjectType )
 *
 * \param paramName name of the multi-instance object, with trailing '.'
 * \param response  pointer to the response
 *
 * \return  OK or ErrorCode
 *
 */
int
addObject (const char *paramName, struct cwmp__AddObjectResponse *response)
{
	int ret = OK;
	int newInstance = 0;

	if (response == NULL)
       	return ERR_INVALID_ARGUMENT;
	setParameterReturnStatus(PARAMETER_CHANGES_APPLIED);
	ret = addObjectIntern( paramName, &newInstance, -1 );
	/* update response
 	*/
	response->InstanceNumber = newInstance;
	response->Status = getParameterReturnStatus();

	return ret;
}

/** Add a new object ( a new instance of a existing parameter ) to the parameter list.
 There must be an object exist with the same name it must be of type MultiObjectType
 a deep copy is to be done, means copy all Child parameter of the original Object
 The new value is taken from the default value of the copy source, the access list and the attributes
 are copied one by one

               Find the source parameter. Must exist and be a MultiObjectType
               Get the actual instance number
               Call the initFunction to check the instance number is not out of bounds
               Increment the instance number
               Create a deep copy of all parameters below the old instance number node
               into the new instance number node

        \param paramName       Name of the object
        \param suggestedInstance suggested Instance ID, valid when >=0

        \param response        return Structure for Instance and Status

        \return       	OK or ErrorCode
*/
int
addObjectIntern (const char *paramName, int *newInstanceNumber, int suggestedInstance)
{
	int ret = OK;
	// cntInstance is the number of active instances of one Object
	// because of deleted objects the instance # is not equal the active objects
	int cntInstance = 1;
	// copyInstance is the instance # from which the new Object is copied
	int copyInstance = 0;
	ParameterEntry *parent, *copyEntry, *newEntry;
	char buf[MAX_PARAM_PATH_LENGTH];

	if (paramName == NULL)
		return ERR_INVALID_ARGUMENT;
	parent = findParameterByPath (paramName);
	if (parent == NULL)
		return ERR_INVALID_PARAMETER_NAME;
	if (parent->type != MultiObjectType)
		return ERR_INVALID_PARAMETER_TYPE;
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_PARAMETER, "CopyEntry found: %s %d\n", parent->name, parent->instance);
	)
	copyInstance = parent->instance;
	// Take the defaultChild as the copy instance
	copyEntry = parent->defaultChild;
	// If no default for copy available search for the last active instance
	if (copyEntry == NULL ) {
		// search the last active instance in the parents child list
		while (copyInstance >= 1)
		{
		      sprintf (buf, "%d", copyInstance);
		      /* find the source
		       */
		      copyEntry = findParamInObjectByName (parent, buf);
		      if (copyEntry == NULL && copyInstance > 1)
		      {
			     cntInstance++;
			     copyInstance--;
		      }
		      else
			     break;
		}
	}
	if (copyInstance == 1 && copyEntry == NULL)
		return ERR_INVALID_ARGUMENT;

	// and count all active ( = not deleted ) instances of the Object
	// so cntInstance is the total number of instance objects
	cntInstance = countChilds (parent);
	// Add one for the new Object which we want create
	cntInstance++;
	/* print source parameters */
	traverseParameter (copyEntry, true, &printParameterName, NULL);

#ifdef PLATFORM_BOVINE
	if ( suggestedInstance < 0) {
		int i=1;
		parent->instance++;
		for(; ;i++ ) {
			sprintf (buf, "%d", i);
			if(findParamInObjectByName (parent, buf) == NULL) {
				suggestedInstance = i;
				break;
			}
		}
	}
#else
	// Create the new object name, it's the instance number
	if ( suggestedInstance < 0)
	{
		parent->instance++;
		suggestedInstance = parent->instance;
	}
#endif
	sprintf (buf, "%d", suggestedInstance);

	ret = li_param_object_create(paramName, suggestedInstance);
	if (ret != OK)
		return ret;

	//Create a copy with the new instance number
	ret = copyObject (copyEntry, parent, buf);
	if (ret != OK)
		return ret;

	// check, we have create the new object by searching for it
	newEntry = findParamInObjectByName (parent, buf);
	if (newEntry == NULL)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_PARAMETER, "addObject:  Parameter not found: %s\n", buf);
		)
		return ERR_INTERNAL_ERROR;
	}
	// now create a deep copy of the copyEntry into newEntry
	inAddObject = true;
	ret = copyParameter (copyEntry, newEntry);
	inAddObject = false;
	if (ret != OK)
		return ret;

	// Call the parent initFunction of the new parameter to do some
	// housekeeping, update some counter
	if (parent->initDataIdx > 0)
	{
		ret = initAccess (parent->initDataIdx, getPathname (parent),
				parent->type, (ParameterValue *)&cntInstance);
		if (ret != OK)
		      return ERR_INVALID_ARGUMENT;
	}

	// Call all InitFunctions of all new parameters, if they have a initFunction
	ret = traverseParameter(newEntry, true, &callInitFunction, NULL);

	// write parent with the updated instance counter
	ret = saveMetaParameter (parent, NULL);
	if (ret != OK)
		return ret;
	/* print the newly created parameters
	 */
	traverseParameter (parent, true, &printParameter, NULL);
	// update response
	*newInstanceNumber = suggestedInstance;
#ifdef HANDLE_NUMBERS_OF_ENTRIES
	strcpy(buf, parent->name);
	strcat(buf, NUMBER_OF_ENTRIES_STR);
	newEntry = findParamInObjectByName (parent->parent, buf);
	// Ignore this feature if no NUMBER_OF_ENTRIES Parameter available
	// should not happen but can happen
	if ( newEntry != NULL ) {
		// because parent is adding 1 for default child we have to decrement
		// the number of children by 1 if parent has a default child entry != NULL.
		// to store the value we need a pointer to the value, therefore use the
		// noChilds from the counter parameter as a temporary buffer
		if ( parent->defaultChild == NULL )
			newEntry->noChilds = parent->noChilds;
		else
			newEntry->noChilds = parent->noChilds - 1;
		setParameter( getPathname(newEntry) , &newEntry->noChilds);
	}
#endif

	return ret;
}

/** Deletes an Object an all it's children. The name must end with a '.'.
 * Before the delete is executed, for the parent and for the deleted object
 * with it children, the deleteAccess function is called if an initIdx is defined.
 *
 * The returned status is
 *  0 = the object has been deleted
 *  1 = the object has been deleted and committed, but a reboot is required
 *
 * \param name	 name of the object to delete
 * \param status	 pointer to the status [0|1]
 *
 * \return  OK or ErrorCode
 *
 */
int
deleteObject (const char *name, int *status)
{
	int ret = OK;
	ParameterEntry *parent, *entry;
#ifdef HANDLE_NUMBERS_OF_ENTRIES
	char noeParamName[MAX_PARAM_PATH_LENGTH];
#endif

	if (name == NULL)
		return ERR_INVALID_ARGUMENT;
	entry = findParameterByPath (name);
	if (entry == NULL)
		return ERR_INVALID_PARAMETER_NAME;
	if (entry->type != ObjectType)
		return ERR_INVALID_PARAMETER_TYPE;

	// DefaultObjects can not be deleted
	if (entry->type == DefaultType)
		return ERR_INVALID_PARAMETER_NAME;
	parent = entry->parent;

#ifdef PLATFORM_BOVINE
	// Call the DeleteAccessFunctions of all Parameters
	ret = traverseParameter(entry, true, &callDeleteFunction, NULL);
	if (ret != OK)
		return ERR_INVALID_ARGUMENT;

	// Now delete the object and the children
	ret = traverseParameterReverse (entry, true, &deleteParameter, NULL);
#endif

	// Call the deleteAccess function for the parent of the deleted object
	// the object is not deleted already
	if (parent->initDataIdx > 0)
	{
		ret = deleteAccess (parent->initDataIdx, getPathname (parent),
							parent->type, (ParameterValue *)&parent->noChilds);
		if (ret != OK)
			return ERR_INVALID_ARGUMENT;
	}

	ret = li_param_object_delete(name);
	if (ret != OK)
		return ERR_INVALID_ARGUMENT;

	// set default return status to '0'
	setParameterReturnStatus(PARAMETER_CHANGES_APPLIED);

#ifndef PLATFORM_BOVINE
	// Call the DeleteAccessFunctions of all Parameters
	ret = traverseParameter(entry, true, &callDeleteFunction, NULL);
	if (ret != OK)
		return ERR_INVALID_ARGUMENT;

	// Now delete the object and the children
	ret = traverseParameterReverse (entry, true, &deleteParameter, NULL);
#endif

	// update the parents child counter
	parent->noChilds --;
//#if defined(PLATFORM_PLATYPUS)
	if(parent->instance > 0)
		parent->instance--;
//#endif
#ifdef HANDLE_NUMBERS_OF_ENTRIES
	strcpy(noeParamName, parent->name);
	strcat(noeParamName, NUMBER_OF_ENTRIES_STR);
	entry = findParamInObjectByName (parent->parent, noeParamName);
	// Ignore this feature if no NUMBER_OF_ENTRIES Parameter available
	// should not happen but can happen
	if ( entry != NULL ) {
		// because parent is adding 1 for default child we have to decrement
		// the number of children by 1 if parent has a default child entry != NULL.
		// to store the value we need a pointer to the value, therefore use the
		// noChilds from the counter parameter as a temporary buffer
		if ( parent->defaultChild == NULL )
			entry->noChilds = parent->noChilds;
		else
			entry->noChilds = parent->noChilds - 1;
		setParameter( getPathname(entry) , &entry->noChilds);
	}
#endif
	*status = getParameterReturnStatus();

	return ret;
}

/** Creates a copy of a parameter and insert it in to the child list of parent.
 The name of the new parameter is taken from dest.
 A '.' is appended to the name if it is not a leaf type.
 The accessList is copied from the src parameter.
 The newly created parameter is stored.

        \param src            	Sourceparameter
        \param parent			Parent of the new created parameter
        \param dest           	Name of the new parameter

        \returns int   			Error code
*/
int
copyObject (ParameterEntry * src, ParameterEntry * parent, const char *dest)
{
	int ret = OK;
	ParameterType type;
	ParameterEntry *newEntry;
	char buf[256];
	strcpy (buf, getPathname (parent));
	strcat (buf, dest);
	if (src->isLeaf == false)
		strcat (buf, ".");

	// change the type of the new parameter if the src is a defaultObject
	// and the new Parameter is not a DefaultObject( dest[0] == '0' )
	if ( dest[0] == '0' )
		type = src->type;
	else {
		if ( src->type == DefaultType && parent->name[0] != '0' )
		      type = ObjectType;
		      // if src is a DefaultObject and the parent is not a DefaultObject
		      // then the new parameter is a standard parameter
		else if ( src->type >= DefStringType && parent->type != DefaultType )
		      type = (src->type - DefaultType);
		else
		      type = src->type;
	}

	/* create a new parameter as a copy of the DefaultParameter */
	ret = newParameter (buf, type, src->instance,
			  src->rebootOnChange, src->notification, src->notificationMax,
			  src->initDataIdx, src->getDataIdx,
			  src->setDataIdx, NULL, &src->value, true);
	if (ret != OK)
		return ret;
	// Search the new parameter
	newEntry = findParameterByPath (buf);
	if (newEntry == NULL)
		return ERR_RESOURCE_EXCEED;
	// copy the access list
	if (copyAccessList (newEntry, src->accessListSize, src->accessList) != OK)
		return ERR_RESOURCE_EXCEED;

	// store the parameter
	ret = saveParameter (newEntry, NULL);
	if (ret != OK) {
		deleteParameter(newEntry, NULL);
		return ret;
	}

	return OK;
}

/**	Copies all Parameters from the src ParameterEntry to the dest ParameterEntry
 * 	It makes a deep copy. src and dest must already exists.
 */
int
copyParameter (ParameterEntry * src, ParameterEntry * dest)
{
        int ret = OK;
        ParameterEntry *newDest;
        ParameterEntry *tmp = src->child;

        while (tmp != NULL)
        {
               if (tmp->isLeaf)
               {
                      // make a copy of tmp into dest
                      ret = copyObject (tmp, dest, tmp->name);
                      if (ret != OK)
                             return ret;
               }
               else
               {
                      ret = copyObject (tmp, dest, tmp->name);
                      if (ret != OK)
                             return ret;
                      newDest = findParamInObjectByName (dest, tmp->name);
                      ret = copyParameter (tmp, newDest);
                      if (ret != OK)
                             return ret;
               }
               tmp = tmp->next;
        }

        return ret;
}

/** Count the number of instances of a MultiObject parameter.
 * Don't mix the number of instances with the instance counter up.
 * The instance counter will never decrement.
 *
 * \param path  ParmeterPath
 * \param count pointer to an integer to store the result
 *
 * \return  OK or Error Code
 */
int
countInstances( const char *path, int *count )
{
        int ret = OK;
        int tmpCnt = 0;
        ParameterEntry *ctmp;

        ParameterEntry *pe = findParameterByPath( path );
        if ( pe == NULL )
               return ERR_INVALID_PARAMETER_NAME;
        if ( pe->type != MultiObjectType )
               return ERR_INVALID_PARAMETER_TYPE;

        ctmp = pe->child;
        tmpCnt = 0;
        while ( ctmp != NULL ) {
               // Only instances greater zero counts
               if ( ctmp->name[0] != '0' )
                      tmpCnt++;
               ctmp = ctmp->next;
        }
        *count = tmpCnt;

        return ret;
}

/** Update the parameter with metadata gotten from the database
 *
 *  The following attributes are updated:
 *             notification, instance, access list
 *
 */
static int
updParameter (const char *path, int instance,
                      NotificationType notification, char *accessList )
{
        int ret = OK;
        int cnt, idx;
        char **array;
        ParameterEntry *updEntry = NULL;

        if ((updEntry = findParameterByPath (path)) != NULL)
        {
               DEBUG_OUTPUT (
            		   dbglog (SVR_INFO, DBG_PARAMETER, "UpdEntry: %s %d %d %s\n", path, instance, notification, accessList);
               )
               updEntry->notification &= NotificationAllways;
               updEntry->notification |= notification;
               updEntry->instance = instance;
               updEntry->status = ValueNotChanged;
               if (accessList != NULL && strlen (accessList) > 0)
               {
                      DEBUG_OUTPUT (
                    		  dbglog (SVR_INFO, DBG_PARAMETER, "path: %s access: %s\n", path, accessList);
                      )
                      cnt = countPieces (accessList);
                      array = (char **) emalloc ((sizeof (char *)) * cnt);
                      for (idx = 0; idx != cnt; idx++)
                      {
                             array[idx] = strsep (&accessList, "|");
                      }
                      copyAccessList (updEntry, cnt, array);
                      efree (array);
               }
               ret = OK;
        } else
               ret = ERR_INVALID_PARAMETER_NAME;

        return ret;
}

/** (NEW) This function is primarily used to setup the Parameters, normally this Parameters have to be stored in
  a RAM environment, so they don't get lost in the case of a power down or reboot.
 In case there is already a parameter there with the same name and it's a MultiObjectTyp and writable
 a new instance of this parameter is created.

 In our case the Parameterdata is stored in a file in the flash ROM file system.
        \param name                         Parameter name limited to 256 chars
        \param type                         Kind of parameter
        \param writeable              access restriction false = read only true = read/write
        \param reboot                if Reboot reboot Hostsystem after all Parameters are set
        \param notification           notification
        \param notificationMax         maximal notification value settable by ACS
        \param accessList             Access list values separated by '|' or NULL
        \param value                 value of this parameter depends on the type
        \param writeData			 if true store data in data file, otherwise only create parameter
        							 used when loading tmp.param at bootstrap or boot
        							 for bootstrap set writeData to true
        							 for boot set writeData to false
        							 for addObject set writeData to true

        \return        int                  ErrorCode
*/
static int
newParameter (const char *path, ParameterType type, int instance,
                      RebootType reboot, NotificationType notification, NotificationMax notificationMax,
                      int initParam, int getParam, int setParam, char *accessList, ParamValue *value,
                      const bool writeData)
{
        int ret = OK;
        bool isNew = true;
        ParameterEntry *newEntry = NULL;
        ParameterEntry *parentEntry = NULL;
        char *name;
        char *parent;
        char **array;
        register int idx;
        register int cnt;
        ParameterValue accessValue;
        parent = getParentName (path);
        name = getName (path);

        // If parameter already exists return OK
        if ( findParameterByPath(path) != NULL )
        	return OK;

        /* check first if parent already exists in our parameter tree
         * if not try to reload it
         */
        if (parent != NULL && findParameterByPath (parent) == NULL) {
        	DEBUG_OUTPUT (
        			dbglog (SVR_ERROR, DBG_PARAMETER, "Parent not found\n");
        	)

            return INFO_PARENT_NOT_FOUND;
        }
        if ((newEntry = findParameterByPath (path)) == NULL)
        {
               DEBUG_OUTPUT (
            		   dbglog (SVR_INFO, DBG_PARAMETER, "NewEntry:  %s %s %d\n", parent, name, type);
               )
               newEntry = (ParameterEntry *) emalloc (sizeof (ParameterEntry));
               newEntry->type = type;
               strnCopy (newEntry->name, name, strlen (name));
               if (setParam == -1 && getParam != -1)
                      newEntry->writeable = ReadOnly;
               else if (setParam != -1 && getParam == -1)
                      newEntry->writeable = WriteOnly;
               else
                      newEntry->writeable = ReadWrite;
               newEntry->next = NULL;
               newEntry->child = NULL;
               newEntry->defaultChild = NULL;
               newEntry->value.cval = NULL;
               if ( newEntry->type == DefaultType ) {
                      parentEntry = findParameterByPath(parent);
                      parentEntry->defaultChild = newEntry;
               }
               isNew = true;
        }
        else
        {
               DEBUG_OUTPUT (
            		   dbglog (SVR_INFO, DBG_PARAMETER, "OldEntry: %s\n", path);
               )
               isNew = false;
        }

        DEBUG_OUTPUT (
        		dbglog (SVR_INFO, DBG_PARAMETER, "Parent: %s Name: %s\n", parent, name);
        )
        /* The instance counter is set after this ParameterEntry is inserted in it's parent list
         */
        newEntry->instance = instance;
        newEntry->accessList = NULL;
        newEntry->accessListSize = 0;
        // Patch old NotificationAllways values to the new one
        if ( notification == 3 )
        	notification = NotificationAllways;
        newEntry->notification = notification;
        newEntry->notificationMax = notificationMax;
        newEntry->rebootOnChange = reboot;
        newEntry->status = ValueNotChanged;
        newEntry->initDataIdx = initParam;
        newEntry->getDataIdx = getParam;
        newEntry->setDataIdx = setParam;
        newEntry->noChilds = 0;
        if (accessList != NULL && strlen (accessList) > 0)
        {
               DEBUG_OUTPUT (
            		   dbglog (SVR_INFO, DBG_PARAMETER, "path: %s access: %s\n", path, accessList);
               )
               cnt = countPieces (accessList);
               array = (char **) emalloc ((sizeof (char *)) * cnt);
               for (idx = 0; idx != cnt; idx++)
               {
                      array[idx] = strsep (&accessList, "|");
               }
               copyAccessList (newEntry, cnt, array);
               efree (array);
        }
        if ( IS_OBJECT(newEntry) )
               newEntry->isLeaf = false;
        else
               newEntry->isLeaf = true;

        // If the parameter has a setDataIdx > 0 or it is a read only parameter, which has to be
        // stored in the database, and is not a part of a DefaultObject
        // and we are during a bootstrap, make an initial load into the parameter value database.
        //
        // If the parameter has no setDataIdx or is part of a DefaultObject than store
        // the data in dimclient and not in the database.
        //
        //
        if ( ( setParam > 0 || getParam == 1 || initParam > 0 )
               && NO_DEFAULT_PARAMETER(newEntry)   //type < DefaultType
               && isNew )
        {
        	if ( paramBootstrap == true || inAddObject == true ) {
        		if ( initParam > 0 ) {
					setParamValue2Access(newEntry->type, value, &accessValue);
        			ret = initAccess(initParam, path, type, &accessValue);
        		}
        	}
        }
        else if ( setParam <= 0 || type >= DefStringType ) {
               ret = setAccess2ParamValue( newEntry, (ParameterValue *)value );
        }
        if (isNew)
           insertParameter (parent, newEntry);

        return ret;
}

/** (NEW) Helper function for getParameterNames
  Recursive calls for child handling if necessary
  Use of traverse() is not possible because we have to count the parameterInfos
        \param parameters      Array of ParameterInfos, defines the input parameter
        \param entry          A parameter
        \param nextLevel       true or false
        \param idx            Ptr to the counter
        \returns int          ErrorCode
*/
int
addParameterInfoStructHelper (struct ArrayOfParameterInfoStruct *parameters,
                            ParameterEntry * entry, bool nextLevel, int *idx)
{
        int ret = OK;
        cwmp__ParameterInfoStruct *pvs;
        ParameterEntry *ctmp;
        /** Allways skip the DefaultObjects
         */
        if ( entry->type == DefaultType )
        {
               return OK;
        }
        /* Don't add MultiObjectTypes cause these are only storage for the InstanceNumber
         */
	/* 080701 0000039 Dimark update */
        if (((entry->type != MultiObjectType) && (nextLevel == false ))
            || (nextLevel == true ))
        {
               pvs = (cwmp__ParameterInfoStruct *)
                      emallocTemp (sizeof (cwmp__ParameterInfoStruct));
               if (pvs == NULL)
                      return ERR_RESOURCE_EXCEED;
               pvs->Name = getPathname (entry);
               if (pvs->Name == NULL)
                      return ERR_RESOURCE_EXCEED;
               pvs->Writable =
                      (entry->writeable ==
                       WriteOnly ? ReadWrite : entry->writeable);
               parameters->__ptrParameterInfoStruct[(*idx)++] = pvs;
        }
        /* Go to the next Level if nextLevel == False
         * or the actual ParemeterEntry is a MultiObjectType
         */
	/* 080701 0000039 Dimark update */
        if ((entry->type == MultiObjectType && nextLevel == false )
            || (nextLevel == false && entry->child != NULL))
        {
               ctmp = entry->child;
               while (ctmp != NULL)
               {
                      ret = addParameterInfoStructHelper (parameters, ctmp, nextLevel, idx);
                      if (ret != OK)
                             return ret;
                      ctmp = ctmp->next;
               }
        }

        return ret;
}

/** (NEW) Get all parameters, name starting with given name,
 If nextLevel == false, return all Levels
 If true only return the parameters of one Level below
 Example:
           ParameterList:  Top.FirstLevel.2ndLevel.3rdLevel.Leaf
               name = Top.FirstLevel  nextLevel = false  returns Top.FirstLevel.2ndLevel.3rdLevel.Leaf
               name = Top.FirstLevel  nextLevel = true  returns Top.FirstLevel.2ndLevel.
        \param name           pathname to root of the requested parameter names,
                                                  can be a Leaf type or an Object type but no MultiObjectType
        \param nextLevel       if true get only the children, if false get the whole hierarchy below
        \param parameters      Return Structure for the found parameter names, incl. name
        \returns int          Status Code
*/
int
getParameterNames (const xsd__string name, xsd__boolean nextLevel,
                                    struct ArrayOfParameterInfoStruct *parameters)
{
        int idx = 0;
        int ret = OK;
        char *tmpname = "";
        ParameterEntry *tmp, *ctmp = NULL;
        cwmp__ParameterInfoStruct *pvs;
//      if (name == NULL || parameters == NULL)
        if ( parameters == NULL)
               return ERR_INVALID_ARGUMENT;
        if ( name != NULL )
               tmpname = name;
        tmp = findParameterByPath (tmpname);
        if (tmp == NULL)
               return ERR_INVALID_PARAMETER_NAME;
//      if (tmp->type == MultiObjectType)
//             return ERR_INVALID_PARAMETER_TYPE;
        DEBUG_OUTPUT (
        		dbglog (SVR_INFO, DBG_PARAMETER, "getParameterNames: %s %s\n", tmpname, tmp->name);
        )
        if (tmp->isLeaf == true)
        {
               /* Alloc Memory for only 1 Parameter
                */
               parameters->__ptrParameterInfoStruct =
                      (cwmp__ParameterInfoStruct **)
                      emallocTemp (sizeof (cwmp__ParameterInfoStruct *));

               if (parameters->__ptrParameterInfoStruct == NULL)
                      return ERR_RESOURCE_EXCEED;
               pvs = (cwmp__ParameterInfoStruct *)
                      emallocTemp (sizeof (cwmp__ParameterInfoStruct));
               if (pvs == NULL)
                      return ERR_RESOURCE_EXCEED;
               pvs->Name = getPathname (tmp);
               if (pvs->Name == NULL)
                      return ERR_RESOURCE_EXCEED;
               pvs->Writable =
                      (tmp->writeable == WriteOnly ? true : tmp->writeable);
               parameters->__ptrParameterInfoStruct[0] = pvs;
               parameters->__size = 1;
               return OK;
        }
        else
        {
               /* Alloc Memory for all parameter pointers in the parameter list
                */
               parameters->__ptrParameterInfoStruct =
                      (cwmp__ParameterInfoStruct **)
                      emallocTemp (sizeof (cwmp__ParameterInfoStruct *) * paramCounter);

               if (parameters->__ptrParameterInfoStruct == NULL)
                      return ERR_RESOURCE_EXCEED;
               /* Ignore the Entry     if it is a MultiObjectType
                */
               if (tmp->type != MultiObjectType)
               {
                      /* add the given parameter into the     array
                       */
#if 0
                      pvs = (cwmp__ParameterInfoStruct *)
                             emallocTemp (sizeof
                                         (cwmp__ParameterInfoStruct));
                      if (pvs == NULL)
                             return ERR_RESOURCE_EXCEED;
                      pvs->Name = getPathname (tmp);
                      if (pvs->Name == NULL)
                             return ERR_RESOURCE_EXCEED;
                      pvs->Writable = tmp->writeable;
                      parameters->__ptrParameterInfoStruct[idx++] = pvs;
#endif
               }
               /* now add all children depend on the NextLevel Flag
                */
               ctmp = tmp->child;
               while (ctmp != NULL)
               {
                      ret = addParameterInfoStructHelper (parameters, ctmp, nextLevel, &idx);
                      ctmp = ctmp->next;
               }
               parameters->__size = idx;

               return OK;
        }
}

/**  Helper function for getParameterNames for Inform message
 * We only return Parameters with:
 *  1. Notification equal NotificationAllways
 *  2. Notification is
 *              ( NotificationActive or NotificationPassive )
 *             and the value has modified by the CPE
 *  3. Notification is
 *              NotificationPassive and the Parameter is ReadOnly and the value has
 *              changed until the last Inform message call
 *      4. Even if Notification is NotificationAllways DefaultObjects are excepted
 *
 */
static int
addInformParameterValueStructHelper (struct ArrayOfParameterValueStruct
                                                                *parameters, ParameterEntry * entry,
                                                                       int *idx, bool * setEventCode)
{
        int ret = OK;
        cwmp__ParameterValueStruct *pvs;
        ParameterEntry *ctmp;
        ParameterValue *accessValue;
        /* Collect all Parameters which has
         *    ( NotificationPassive + ValueModifiedExtern  or
         *      NotificationActive  + ValueModifiedExtern  or
         *      NotificationAllways ) and isLeaf
         *     and not part of a DefaultObject
         */
        /* Don't add MultiObjectTypes cause these are only storage for the InstanceNumber
         */

        if ( entry->isLeaf == true
             && entry->notification != NotificationNone
             && NO_DEFAULT_PARAMETER(entry))
        {
               if ((entry->notification & NotificationAllways) == NotificationAllways
                   || entry->status == ValueModifiedExtern
                   || ( entry->notification == NotificationPassive
                       /* && entry->writeable == ReadOnly */) )
               {
                      pvs = (cwmp__ParameterValueStruct *)
                             emallocTemp (sizeof(cwmp__ParameterValueStruct));
                      if (pvs == NULL)
                             return ERR_RESOURCE_EXCEED;
                      pvs->Name = getPathname (entry);
                      if (pvs->Name == NULL)
                             return ERR_RESOURCE_EXCEED;

#ifndef ACS_REGMAN
                      pvs->__typeOfValue = entry->type;
#endif
                      /* No Access function defined, so we take value from the parameter
                       */
                      if (entry->getDataIdx == 0)
                      {
                             pvs->Value = getValue (entry);
                      }
                      else
                      {
                             accessValue = (ParameterValue*)emallocTemp( sizeof( ParameterValue ));
                             setParamValue2Access(entry->type, &entry->value, accessValue);
                             /* we have to return a pointer to the parameter value,
                              * therefore we store the value in parameter a
                              * return a pointer to this value.
                              */
                             ret = getAccess (entry->getDataIdx, pvs->Name, entry->type, accessValue);
                             if (ret != OK)
                                    return ret;
//                           pvs->Value = &accessValue;
                             setAccess2VoidPtr(entry->type, &pvs->Value, accessValue);
//                           if ( IS_CHAR_PTR_TYPE(entry))
//                                  pvs->Value = *(char**)value;
//                           else
//                                  pvs->Value = value;
                             // Additional check if the notification is passive and
                             // the access restriction is read only:
                             // has the value changed since the last inform message?
                             if ( (entry->notification & NotificationPassive) == NotificationPassive
                                    /* && entry->writeable == ReadOnly */) {
                                    if ( compareValue( entry, &accessValue ) ) {
                                           pvs = NULL;
                                    } else {
                                           setAccess2ParamValue(entry, accessValue);
                                           entry->status = ValueModifiedExtern;
                                    }
                             }
                      }
                      if ( pvs != NULL ) {
                             parameters->__ptrParameterValueStruct[(*idx)++] = pvs;
                             if (entry->status == ValueModifiedExtern)
                                    *setEventCode = true;
                      }
               }
        }
        /* Go to the next Level if there a more child parameter
         */
        if (entry->child != NULL)
        {
               ctmp = entry->child;
               while (ctmp != NULL)
               {
                      ret = addInformParameterValueStructHelper (parameters,
                                                            ctmp, idx,
                                                            setEventCode);
                      if (ret != OK)
                             return ret;
                      ctmp = ctmp->next;
               }
        }

        return ret;
}
/** Returns an array of parameters where the following conditions are true
 Collect all Parameters which has
               ( NotificationPassive + ValueModifiedExtern  or
                 NotificationActive  + ValueModifiedExtern  or
                 NotificationAllways ) and isLeaf

        The memory for the array is allocated via emallocTemp()

        \param parameters      Array of Value structs for returning the data
        \returns              OK | ERR_RESOURCE_EXCEED
*/
int
getInformParameters (struct ArrayOfParameterValueStruct *parameters)
{
        int ret = OK;
        int idx = 0;
        bool setEventCode = false;
        ParameterEntry *tmp = NULL;
        /* Alloc Memory for all parameter pointers in the parameter list
         */
        parameters->__ptrParameterValueStruct =
               (cwmp__ParameterValueStruct **)
               emallocTemp (sizeof (cwmp__ParameterValueStruct *) * (paramCounter+2));

        if (parameters->__ptrParameterValueStruct == NULL)
               return ERR_RESOURCE_EXCEED;
        /* loop over all parameters and return all parameters which are changed
         * and has a passive or active notification
         */
        tmp = firstParam;
        ret = addInformParameterValueStructHelper (parameters, tmp, &idx, &setEventCode);
        if (ret != OK)
               return ret;

        parameters->__size = idx;

        if (setEventCode)
               addEventCodeSingle (EV_VALUE_CHANGE);

        DEBUG_OUTPUT (
        		dbglog (SVR_INFO, DBG_PARAMETER, "Number of Informparameters: %d setEventCode: %d\n", idx, setEventCode);
        )

        return OK;
}

/** Reset the Status of the Parameters to ValueNotChanged
*/
static int
setParameterStatus (ParameterEntry * entry, void *arg)
{
        int ret = OK;
        if (entry->status == ValueModifiedExtern)
        {
               entry->status = (StatusType) arg;
               ret = saveParameter (entry, NULL);
        }

        return ret;
}

void
resetParameterStatus (void)
{
        traverseParameter (firstParam, true, &setParameterStatus, ValueNotChanged);
}

/** Helper function for getParameterNames
 Recursive calls for child handling if necessary
 Use of traverse() is not possible because we have to count the parameterInfos
*/
int
addParameterValueStructHelper (struct ArrayOfParameterValueStruct *parameters, ParameterEntry * entry, int *idx)
{
        int ret = OK;
        int tmpIdx;
        char *paramPath;
        cwmp__ParameterValueStruct *pvs;
        ParameterEntry *ctmp;
        ParameterValue *accessValue;
        /* Don't add MultiObjectTypes cause these are only storage for the InstanceNumber
         */
        if (entry->type != MultiObjectType
                && entry->type != ObjectType
                && entry->type != DefaultType)
        {
			// ignore the parameter entry if it is already in the paramaterValueStruct
			// for this, search the list of all the parameters we already found for the new
			// parameter.
			tmpIdx = 0;
			paramPath = getPathname (entry);
			while( tmpIdx != *idx ) {
				if ( strCmp( parameters->__ptrParameterValueStruct[tmpIdx++]->Name, paramPath))
					return ret;
			}


			pvs = (cwmp__ParameterValueStruct *)
                   emallocTemp (sizeof (cwmp__ParameterValueStruct));
			if (pvs == NULL)
				return ERR_RESOURCE_EXCEED;
			pvs->Name = paramPath; // getPathname (entry);
			if (pvs->Name == NULL)
				return ERR_RESOURCE_EXCEED;
#ifndef ACS_REGMAN
			if( IS_DEFAULT_PARAMETER(entry) )  // ->type >= DefStringType )
				pvs->__typeOfValue = (entry->type - DefaultType);
			else
				pvs->__typeOfValue = entry->type;
#endif
			// If parameter is WriteOnly then return an empty string
			if (entry->writeable == WriteOnly)
			{
				pvs->Value = "";
			}
			else
			{
                      // Don't use access functions for DefaultParameters
				if (entry->getDataIdx == 0 || IS_DEFAULT_PARAMETER(entry))
				{
					pvs->Value = getValue (entry);
				}
				else
				{
					accessValue = (ParameterValue*)emallocTemp( sizeof( ParameterValue ));
					setParamValue2Access(entry->type, &entry->value, accessValue);
					// we have to return a pointer therefore we use the entry->value
					// as a buffer for the retrieved parameter value
//                           )
//                           value = &entry->value;
                             // this may cause an error if the parameter is deleted
                             // the memory is freed twice, therefore the pointer has to be cleared after free()
					ret = getAccess (entry->getDataIdx, pvs->Name, entry->type, accessValue);
					if (ret != OK)
						return ret;
					setAccess2VoidPtr(entry->type, &pvs->Value, accessValue);
//                           if ( IS_CHAR_PTR_TYPE(entry))
//                                  pvs->Value = *(char**)value;
//                           else
//                                  pvs->Value = value;

                      }
               	}
				parameters->__ptrParameterValueStruct[(*idx)++] = pvs;
        }
        /* Go to the next Level if nextLevel == False
         * or the actual ParemeterEntry is a MultiObjectType
         */
        if ((entry->type == MultiObjectType) || (entry->child != NULL))
        {
               ctmp = entry->child;
               while (ctmp != NULL)
               {
                      // don't return values with instance is 0 because this are only for creating a new instance
                      if ( ctmp->name[0] !=  '0' ) {
                             ret = addParameterValueStructHelper (parameters, ctmp, idx);
                             if (ret != OK)
                                    return ret;
                      }
                      ctmp = ctmp->next;
               }
        }

        return ret;
}
/** (NEW) Returns an array of parameters identified by name in the array parameters.
 If name is an ObjectType, then all parameters starting with this name are returned.
 The memory for the array is allocated via emallocTemp()
        \param name            	ParameterNameList
        \param parameters      	Returnlist
        \returns              	OK or ERR_INVALID_PARAMETER_NAME
*/
int
getParameters (const struct ArrayOfString *nameList,
               struct ArrayOfParameterValueStruct *parameters)
{
        int i = 0;
        int idx = 0;
        int ret = OK;
        int nameArraySize = 0;
        char *name;
        char **nameArray;
        ParameterEntry *tmp = NULL;
        DEBUG_OUTPUT (
        		dbglog (SVR_INFO, DBG_PARAMETER, "paramCounter: %d\n", paramCounter);
        )
        if (nameList == NULL || parameters == NULL)
               return ERR_INVALID_ARGUMENT;
        if (nameList->__ptrstring == NULL) // HH 18.2. Sphairon
               return ERR_INVALID_ARGUMENT;
        nameArraySize = nameList->__size;
        DEBUG_OUTPUT (
        		dbglog (SVR_INFO, DBG_PARAMETER, "nameListe size: %d\n", nameArraySize);
        )
        nameArray = nameList->__ptrstring; // HH 18.2. Sphairon
        /* Alloc Memory for all parameter pointers in the parameter list
         */
        parameters->__ptrParameterValueStruct =
               (cwmp__ParameterValueStruct **)
               emallocTemp (sizeof (cwmp__ParameterValueStruct *) *paramCounter);
        if (parameters->__ptrParameterValueStruct == NULL)
               return ERR_RESOURCE_EXCEED;

        /* loop over all searched names
         */
        for (i = 0; i != nameArraySize; i++)
        {
               name = (char *) nameArray[i];
               if (name == NULL)
                      return ERR_INVALID_PARAMETER_NAME;
               tmp = findParameterByPath (name);
               if (tmp == NULL)
                      return ERR_INVALID_PARAMETER_NAME;
               // HH 18.2. Sphairon MultiObjectType is a legal parameter
//             if (tmp->type == MultiObjectType)
//                    return ERR_INVALID_PARAMETER_TYPE;
//              if ( tmp->writeable     == WriteOnly )
//                      return ERR_WRITEONLY_PARAMETER;
               ret = addParameterValueStructHelper (parameters, tmp, &idx);
               if (ret != OK)
                      return ret;
        }
        parameters->__size = idx;

        return OK;
}

/** (NEW) Set all parameters, name starting with given name,
 the host function parameterValueChanged() is called to give the host a change to do the appropriate actions
 There is <b>no</b> need to inform the server of changed Parameter, which are changed by the server.
 \param parameters      Parameter name and value
 \param status         Status of the setting,
                                           0 = changes have been validated and applied
                                           1 = changes have been validated and committed, but not yet applied.

 \returns int ErrorCode or OK
*/
int
setParameters (struct ArrayOfParameterValueStruct *parameters, int *status)
{
        register int i = 0;
        int ret = OK;
        int masterRet = OK;
        int paramArraySize = 0;
        char *name, boolchar[2];
        cwmp__ParameterValueStruct **paramArray;
        cwmp__ParameterValueStruct *pvs;
        ParameterEntry *tmp = NULL;
        void *value = NULL;
        unsigned int intValue;
        time_t timeValue;
        ParameterValue accessValue;

        DEBUG_OUTPUT (
        		dbglog (SVR_INFO, DBG_PARAMETER, "paramCounter: %d\n", paramCounter);
        )
        // delete formerly created FaultMessage
        clearFault ();
        if (parameters == NULL || parameters->__ptrParameterValueStruct == NULL)
               return ERR_INVALID_ARGUMENT;
        paramArraySize = parameters->__size;
        DEBUG_OUTPUT (
        		dbglog (SVR_INFO, DBG_PARAMETER, "paramlist  size: %d\n", paramArraySize);
        )
        /* Alloc Memory for all parameter pointers in the parameter list
         */
        paramArray = parameters->__ptrParameterValueStruct;
        DEBUG_OUTPUT (
        		dbglog (SVR_INFO, DBG_PARAMETER, "parameters->__ptrParameterValueStruct: %p\n",
						parameters->__ptrParameterValueStruct);
        )
        // Set the default value for the status
        setParameterReturnStatus(PARAMETER_CHANGES_APPLIED);

        // Loop over the paramArray
        for (i = 0; i != paramArraySize; i++) {
               pvs = paramArray[i];
               name = (char *) pvs->Name;
               /* Can't change a node Object -> ERROR
                */
               if (isObject (name)) {
                      addFault ("SetParameterValuesFault", name, ERR_INVALID_PARAMETER_TYPE);
                      masterRet = ERR_INVALID_ARGUMENT;
                      continue;
               }
               tmp = findParameterByPath (name);
               if (tmp == NULL) {
                      addFault ("SetParameterValuesFault", name, ERR_INVALID_PARAMETER_NAME);
                      masterRet = ERR_INVALID_ARGUMENT;
                      continue;
               }
               if ( pvs->Value == NULL ) {
                      addFault ("SetParameterValuesFault", name, ERR_INVALID_PARAMETER_VALUE);
                      masterRet = ERR_INVALID_ARGUMENT;
                      continue;
               }
               if (tmp->writeable == ReadOnly) {
                      addFault ("SetParameterValuesFault", name, ERR_READONLY_PARAMETER);
                      masterRet = ERR_INVALID_ARGUMENT;
                      continue;
               }
	       /* Strict type checking only with STRICT_TYPE_CHECKING set */
#ifdef	STRICT_TYPE_CHECKING
               // Special handling for boolean type, because of xdr__boolean_ ( Code 20 )
               // which may be set instead of xdr__boolean ( Code 18 )
               if ( tmp->type != SOAP_TYPE_xsd__boolean ) {
#ifndef ACS_REGMAN
		 if (pvs->__typeOfValue != tmp->type ) {
		   addFault ("SetParameterValuesFault", name, ERR_INVALID_PARAMETER_TYPE);
		   masterRet = ERR_INVALID_ARGUMENT;
		   continue;
		 }
#endif
               } else {
#ifndef ACS_REGMAN  // Remove the TypeChecking
		 if ( pvs->__typeOfValue != tmp->type && pvs->__typeOfValue != SOAP_TYPE_xsd__boolean_ ) {
		   addFault ("SetParameterValuesFault", name, ERR_INVALID_PARAMETER_TYPE);
		   masterRet = ERR_INVALID_ARGUMENT;
		   continue;
		 }
#endif	ACS_REGMAN
               }
#endif	/* STRICT_TYPE_CHECKING */

	       /* If input (from ACS) is of type String, check destination type  */
	       /* and do conversion here to destination type value               */
	       if ( pvs->__typeOfValue == 6 ) {
		 switch (tmp->type) {
		   /* Accept '0|1|f|t|T|F */
		   case BooleanType:
		     /* compiler didn't like (char *) pvs->Value[0]			*/
		     /* or (char) pvs->Value[0], so changed to copy a byte		*/
		     (void) strncpy(boolchar, pvs->Value, 1);
		     if     ( boolchar[0] == 't' || boolchar[0] == 'T' )
		       intValue = TRUE_VALUE;
		     else if( boolchar[0] == 'f' || boolchar[0] == 'F' )
		       intValue = FALSE_VALUE;
		     else
		       intValue = a2i (pvs->Value);

		     DEBUG_OUTPUT (
		    		 dbglog (SVR_DEBUG, DBG_PARAMETER, "%d bool\n",intValue);
		     )

		     value = &intValue;
		     break;
		   case DefIntegerType:
		   case DefUnsignedIntType:
		   case DefBooleanType:
		   case IntegerType:
		   case UnsignedIntType:
		     intValue = a2i (pvs->Value);

		     DEBUG_OUTPUT (
		    		 dbglog (SVR_DEBUG, DBG_PARAMETER, "%d int\n",intValue);
		     )

		     value = &intValue;
		     break;
		   case DefStringType:
		   case DefBase64Type:
		   case StringType:
		   case Base64Type:
		     value = (void *) pvs->Value;

		     DEBUG_OUTPUT (
		    		 dbglog (SVR_DEBUG, DBG_PARAMETER, "%s str\n",value);
		     )

		     break;
		   case DefDateTimeType:
		   case DateTimeType:
//#ifdef DATE_AS_INT
//		     timeValue = (void *) a2l (pvs->Value);
//		     value = &timeValue;
//#else
		     ret = s2dateTime( pvs->Value, &timeValue);
		     value = &timeValue;
//#endif
		     break;
		   default:
			   break;
		   } /* switch */

	       } else {
		 /* Input type != string, therefore convert to	*/
		 /* target type later in setValuePtr()			*/
		 value = pvs->Value;
	       }
               /* if the setDataIdx == 0 we only store the data an do not call
                * the host system
                */
               if (tmp->setDataIdx == 0) {
                  ret = setValuePtr (tmp, value );  // pvs->Value);
		  if (ret != OK)
		    return ret;
		  /* store the parameter in the persistence system */
		  ret = saveParameter (tmp, NULL);
		  if (ret != OK)
		    return ret;
               } else {
		 setVoidPtr2Access(tmp->type, value, &accessValue);

		 /* inform the host system that a parameter has  changed */
		 ret = setAccess (tmp->setDataIdx, name, tmp->type, &accessValue );  // &pvs->Value);
		 if (ret != OK)
		   return ret;
               }
#ifdef NO_LOCAL_REBOOT_ON_CHANGE
               if (tmp->rebootOnChange == Reboot)
		   setParameterReturnStatus(PARAMETER_CHANGES_NOT_APPLIED);
#else
               if (tmp->rebootOnChange == Reboot)
		   setReboot ();
#endif
        } /* for */
        *status = getParameterReturnStatus();

        return masterRet;
}

/** (NEW) The internal version of setParameter.
 No notification is generated if the Parameter has changed.

        \param parameterPath   the complete Parameter name with all parents
        \param value                 pointer to the new value, a copy is made

        \returns ErrorCode
*/
int
setParameter (const char *parameterPath, void *value)
{
        int ret = OK;
        ParameterValue accessParam;
        ParameterEntry *tmp = NULL;

        if (parameterPath == NULL)
               return ERR_INVALID_ARGUMENT;
        if (isObject (parameterPath))
               return ERR_INVALID_PARAMETER_TYPE;
        tmp = findParameterByPath (parameterPath);
        if (tmp == NULL)
               return ERR_INVALID_ARGUMENT;
        /* if the parameter is read only for the ACS we use the
         * getDataIdx for accessing the data storage
         * If the ..DataIdx value == 1 the value is store permanently
         * If the ..DataIdx value == 0 the value is stored temporarily
         */
        setVoidPtr2Access(tmp->type, value, &accessParam);
        if (tmp->setDataIdx == 1
            || tmp->getDataIdx == 1 )
        {
               if ( tmp->setDataIdx > 0 )
                      ret = setAccess (tmp->setDataIdx, parameterPath, tmp->type, &accessParam);
               else
                      ret = setAccess (tmp->getDataIdx, parameterPath, tmp->type, &accessParam);
               if (ret != OK)
                      return ret;
        }
        if (tmp->setDataIdx == 0
            || tmp->getDataIdx == 0 )
        {
       		ret = setAccess2ParamValue (tmp, &accessParam);
            if (ret != OK)
            	return ret;
        }

        if (tmp->rebootOnChange == Reboot)
               setReboot ();
        /* store the parameter in the file system
         */
//      ret = saveParameter (tmp, NULL);
//      if (ret != OK)
//             return ret;

        return OK;
}

/** Get the value of a single parameter into value.
* There is no way to get the value of an object.
* The write only status of a parameter is ignored, because this function
* is only used inside the CPE and the write only status is only for the ACS
*
        \param parameterPath   Path incl. Name of the Parameter
        \param value                 Pointer to a value object which can hold the value
        \returns int                 ErrorCode
                                                  ERR_INVALID_PARAMETER_TYPE   The parameterPath points to    a Object
                                                  ERR_WRITEONLY_PARAMETER      The Parameter is not readable ( not used here )
                                                  ERR_INVALID_ARGUMENT         the Parameter is unknown
*/
int
getParameter (const char *parameterPath, void *value)
{
	int ret = OK;
	ParameterValue *accessValue;
	ParameterEntry *tmp = NULL;

	if (parameterPath == NULL) {
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_PARAMETER, "getParameter(): \"%s\" err=%d",parameterPath, ERR_INVALID_ARGUMENT);
		)
		return ERR_INVALID_ARGUMENT;
	}

	if (isObject (parameterPath)) {
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_PARAMETER, "getParameter->isObject(): \"%s\" err=%d",parameterPath, ERR_INVALID_PARAMETER_TYPE);
		)

		return ERR_INVALID_PARAMETER_TYPE;
	}

    tmp = findParameterByPath (parameterPath);

    if (tmp == NULL) {
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_PARAMETER, "getParameter->findParameterByPath(): \"%s\" err=%d",parameterPath, ERR_INVALID_ARGUMENT);
		)

	  return ERR_INVALID_ARGUMENT;
	}

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_PARAMETER, "getParameter() Pathname:  %s\n", getPathname (tmp));
	)

        /* ignore the read only flag this is only for the ACS
         */
        if (tmp->getDataIdx == 0 || tmp->type >= DefStringType
               || ( tmp->getDataIdx < 0 && tmp->setDataIdx == 0) )
        {
               switch (tmp->type)
               {
                      case DefStringType:
                      case DefBase64Type:
                      case StringType:
                      case Base64Type:
                             *(char **) value = tmp->value.cval;
                      break;
                      // TODO San 8 june 2011:
                      // It is necessary to add handling for types unsigned int & time_t. They are processed as int now.
                      default: // HH
                             *(int **) value = &tmp->value.ival;
                      break;
               }
        }
        else
        {
               accessValue = (ParameterValue*)emallocTemp( sizeof( ParameterValue ));

               // If this is a WriteOnly Parameter, we get the setDataIdx to read the value
               if ( tmp->getDataIdx > 0 )
                      ret = getAccess (tmp->getDataIdx, getPathname (tmp), tmp->type, accessValue);
               else
                      ret = getAccess (tmp->setDataIdx, getPathname (tmp), tmp->type, accessValue);
               setAccess2VoidPtr(tmp->type, value, accessValue);
        }

        return ret;
}

int save_paramstr(ParameterEntry *entry, char *value)
{
	if (entry->value.cval) efree (entry->value.cval);

	entry->value.cval = strnDup (entry->value.cval, (char *) value,
						strlen ((char *) value));

	if (entry->value.cval == NULL) return ERR_RESOURCE_EXCEED;
}
/** The external version of setParameter.
 * notification is generated if the Parameter has changed.
 *
 *      \param parameterPath	the complete Parametername with all parents
 *      \param notification		pointer to bool variable which is set to true if notification has to be done
 *      \param value			pointer to the new value, a copy is made
 *
 *      \returns ErrorCode
 */
int
setParameter2Host (const char *parameterPath, bool *notification, char *value)
{
	int ret = OK;
	ParameterEntry *param = NULL;
	/* In case of error we send no notification to the ACS */
	*notification = false;

	if (parameterPath == NULL)
		return ERR_INVALID_ARGUMENT;
	param = findParameterByPath (parameterPath);
	if (param == NULL)
		return ERR_INVALID_ARGUMENT;
	if (IS_OBJECT (param) )
		return ERR_INVALID_PARAMETER_TYPE;
	if (IS_OBJECT(param))
		return ERR_INVALID_PARAMETER_TYPE;

#if 0
	if (param->writeable == ReadOnly)
		return ERR_READONLY_PARAMETER;
#endif
	// The new value is delivered as a ASCII string
	// Therefore we first convert the string depending the parameter type
	// TODO check access method before storing data in parameter
	switch (param->type)
	{
		case StringType:
		case Base64Type:
			save_paramstr(param, value);
			setParameter( parameterPath, value );
			break;
        case BooleanType:
        case IntegerType:
			param->value.ival = a2i (value);
			setParameter( parameterPath, &param->value.ival );
			break;
        case UnsignedIntType:
			param->value.uival = (unsigned int) a2i (value);
			setParameter( parameterPath, &param->value.uival );
			break;
        case DateTimeType:
			param->value.tval = (unsigned int) a2i (value);
			setParameter( parameterPath, value );
        	break;
		default:
			break;
	}

	param->status = ValueModifiedExtern;
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_PARAMETER, "Ext Notify ACS: name = %s notification = %d\n", param->name, param->notification);
	)

	if ((param->notification & NotificationActive) == NotificationActive )
	{
		*notification = true;
	}

	return ret;
}

int setParameterForNotifications (const char *parameterPath, void *value)
{
        int ret = OK;
        ParameterValue accessParam;
        ParameterEntry *tmp = NULL;

        if (parameterPath == NULL)
               return ERR_INVALID_ARGUMENT;
        if (isObject (parameterPath))
               return ERR_INVALID_PARAMETER_TYPE;
        tmp = findParameterByPath (parameterPath);
        if (tmp == NULL)
               return ERR_INVALID_ARGUMENT;
        /* if the parameter is read only for the ACS we use the
         * getDataIdx for accessing the data storage
         * If the ..DataIdx value == 1 the value is store permanently
         * If the ..DataIdx value == 0 the value is stored temporarily
         */
        setVoidPtr2Access(tmp->type, value, &accessParam);
#if 0
        if (tmp->setDataIdx == 1
            || tmp->getDataIdx == 1 )
        {
               if ( tmp->setDataIdx > 0 )
                      ret = setAccess (tmp->setDataIdx, parameterPath, tmp->type, &accessParam);
               else
                      ret = setAccess (tmp->getDataIdx, parameterPath, tmp->type, &accessParam);
               if (ret != OK)
                      return ret;
        }
#endif
#ifndef PLATFORM_PLATYPUS
        if (tmp->setDataIdx == 0
            || tmp->getDataIdx == 0 )
#endif
        {
       		ret = setAccess2ParamValue (tmp, &accessParam);
            if (ret != OK)
            	return ret;
        }

        if (tmp->rebootOnChange == Reboot)
               setReboot ();
        /* store the parameter in the file system
         */
//      ret = saveParameter (tmp, NULL);
//      if (ret != OK)
//             return ret;

        return OK;
}


int setParameter2HostForNotifications (const char *parameterPath, bool *notification, char *value)
{
	int ret = OK;
	ParameterEntry *param = NULL;
	/* In case of error we send no notification to the ACS */
	*notification = false;

	if (parameterPath == NULL)
		return ERR_INVALID_ARGUMENT;
	param = findParameterByPath (parameterPath);
	if (param == NULL)
		return ERR_INVALID_ARGUMENT;
	if (IS_OBJECT (param) )
		return ERR_INVALID_PARAMETER_TYPE;
	if (IS_OBJECT(param))
		return ERR_INVALID_PARAMETER_TYPE;

	// The new value is delivered as a ASCII string
	// Therefore we first convert the string depending the parameter type
	// TODO check access method before storing data in parameter
	switch (param->type)
	{
		case StringType:
		case Base64Type:
			save_paramstr(param, value);
			setParameterForNotifications( parameterPath, value );
			break;
        case BooleanType:
        case IntegerType:
			param->value.ival = a2i (value);
			setParameterForNotifications( parameterPath, &param->value.ival );
			break;
        case UnsignedIntType:
			param->value.uival = (unsigned int) a2i (value);
			setParameterForNotifications( parameterPath, &param->value.uival );
			break;
        case DateTimeType:
			param->value.tval = (unsigned int) a2i (value);
			setParameterForNotifications( parameterPath, value );
        	break;
		default:
			break;
	}

	param->status = ValueModifiedExtern;
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_PARAMETER, "Notify ACS: name = %s notification = %d\n", param->name, param->notification);
	)

	if ((param->notification & NotificationActive) == NotificationActive )
	{
		*notification = true;
	}

	return ret;
}

/** Returns a named parameter which is send to the client host.
 * returns the parameter type in the type variable
 * and the write able value
 *
 *      \param parameterPath	the complete Parameter name
 * 		\param *type			pointer to returning the parameter type
 *      \param *access			pointer to return the access rights
 *      \param *value			pointer to return the value, do not free this mem pointer
 *
 *      \returns ErrorCode
*/
int
getParameter2Host (const char *parameterPath, ParameterType *type, AccessType *access, void *value)
{
        int ret = OK;
        ParameterEntry *param = NULL;

        if (parameterPath == NULL)
               return ERR_INVALID_ARGUMENT;
        param = findParameterByPath (parameterPath);
        if (param == NULL)
               return ERR_INVALID_ARGUMENT;
        if (IS_OBJECT (param))
               return ERR_INVALID_PARAMETER_TYPE;
        if (IS_DEFAULT_PARAMETER(param))
               return ERR_INVALID_PARAMETER_TYPE;
        *access = param->writeable;
        *type = param->type;
        ret = getParameter( parameterPath, value );

        return ret;
}

int
setParametersAttributesHelperGroup (ParameterEntry *param, void *arg)
{
        cwmp__SetParameterAttributesStruct *pvs =
               (cwmp__SetParameterAttributesStruct *) arg;

        // skip objects and default parameters
        if ( IS_OBJECT(param) || IS_DEFAULT_PARAMETER(param) )
               return OK;

        if (pvs->NotificationChange == true_)
        {
               if ( param->notificationMax != NotificationNotChangeable
                      && param->notificationMax >= pvs->Notification ) {
                   return OK;
               } else {
                      return ERR_NOTIFICATION_REQ_REJECT;
               }
        }

        return OK;
}

/** HelperFunction for setting parameter attributes using the traverse()
        \param entry  Ptr to the first Parameter in the hieratic,
                                    using an Object type means all below the entry
        \param arg           Pointer to the data structure
        \return        int           Error code
*/
static int
setParametersAttributesHelper (ParameterEntry *param, void *arg)
{
        bool isChanged = false;
        int ret = OK;
        char **accessList;
        int cnt;
        cwmp__SetParameterAttributesStruct *pvs =
               (cwmp__SetParameterAttributesStruct *) arg;

        // skip objects and default parameters
        if ( IS_OBJECT(param) || IS_DEFAULT_PARAMETER(param) )
               return OK;

        if (pvs->NotificationChange == true_)
        {
               if ( param->notificationMax != NotificationNotChangeable
                      && param->notificationMax >= pvs->Notification ) {
                      // clear old notification and preserve NotificationAllways if set
                      param->notification &= NotificationAllways;
                      param->notification |= pvs->Notification;
                      isChanged = true;
               } else {
                      return ERR_NOTIFICATION_REQ_REJECT;
               }
        }
        if (pvs->AccessListChange == true_)
        {
               accessList = pvs->AccessList->__ptrstring; // HH 18.2. Sphairon
               cnt = pvs->AccessList->__size;
               freeAccessList (param);
               ret = OK;
               if ((ret = copyAccessList (param, cnt, accessList)) != OK)
                      return ret;
               isChanged = true;
        }
        if (isChanged == true)
        {
               ret = saveMetaParameter(param, arg);
               if (ret != OK)
                      return ret;
        }
        return OK;
}

/** (NEW) Set parameter attributes, name starting with given name,
 the host function parameterValueChanged() is called to give the host a change to do the appropriate actions

 \param nameList       Parameternames
 \param attributes     Returns the attribute values ( Name, Notification and AccessList ) of the named parameter

 \returns int ErrorCode or OK
*/
int
setParametersAttributes (struct ArrayOfSetParameterAttributesStruct *parameters)
{
        register int i = 0;
        int ret = OK;
        int paramArraySize = 0;
        char *name;
        cwmp__SetParameterAttributesStruct **paramArray, *pvs;
        ParameterEntry *tmp = NULL;

        DEBUG_OUTPUT (
        		dbglog (SVR_INFO, DBG_PARAMETER, "paramCounter: %d\n", paramCounter);
        )

        if (parameters == NULL
            || parameters->__ptrSetParameterAttributesStruct == NULL)
               return ERR_INVALID_ARGUMENT;

        paramArraySize = parameters->__size;

        DEBUG_OUTPUT (
        		dbglog (SVR_INFO, DBG_PARAMETER, "paramlist size: %d\n", paramArraySize);
        )

        paramArray = parameters->__ptrSetParameterAttributesStruct;

        DEBUG_OUTPUT (
        		dbglog (SVR_INFO, DBG_PARAMETER, "parameters->__ptrParameterValueStruct: %p\n",
        				parameters->__ptrSetParameterAttributesStruct);
        )

        for (i = 0; i != paramArraySize; i++)
        {
               pvs = paramArray[i];
               name = (char *) pvs->Name;
               tmp = findParameterByPath (name);
               if (tmp == NULL)
                      return ERR_INVALID_PARAMETER_NAME;
               ret = traverseParameter (tmp, true, &setParametersAttributesHelperGroup, pvs);
               if (ret != OK)
                      return ret;
        }

        for (i = 0; i != paramArraySize; i++)
        {
               pvs = paramArray[i];
               name = (char *) pvs->Name;
               tmp = findParameterByPath (name);
               if (tmp == NULL)
                      return ERR_INVALID_PARAMETER_NAME;
               ret = traverseParameter (tmp, true, &setParametersAttributesHelper, pvs);
               if (ret != OK)
                      return ret;
        }

        return OK;
}

/** (NEW) Helper function for getParameterAttributes
 Recursive calls for child handling if necessary
 Use of traverse() is not possible because we have to count the parameterInfos
*/
int
addParameterAttributesStructHelper (struct ArrayOfParameterAttributeStruct
                                 *attributes, ParameterEntry *entry,
                                 int *idx)
{
        int ret = OK;
        int tmpIdx;
        char *paramPath;
        ParameterEntry *ctmp;

        /* Don't add MultiObjectTypes cause these are only storage for the InstanceNumber
         */
        if (entry->isLeaf)
        {
			// ignore the parameter entry if it is already in the paramaterValueStruct
			// for this, search the list of all the parameters we already found for the new
			// parameter.
			tmpIdx = 0;
			paramPath = getPathname (entry);
			while( tmpIdx != *idx ) {
				if ( strCmp( attributes->__ptrParameterAttributeStruct[tmpIdx++]->Name, paramPath))
					return ret;
			}

			cwmp__ParameterAttributeStruct *pas =
                      (cwmp__ParameterAttributeStruct *)
                      emallocTemp (sizeof (cwmp__ParameterAttributeStruct));
			if (pas == NULL)
				return ERR_RESOURCE_EXCEED;
			pas->Name = getPathname (entry);
			if (pas->Name == NULL)
				return ERR_RESOURCE_EXCEED;
			// Special handling for notification type: NotificationAllways
			// NotificationAllways is equal to NotificationNone, except it is allays included in
			// an InformMessage
			pas->Notification = entry->notification & ~NotificationAllways;
			pas->AccessList =
                      (struct ArrayOfString *)
                      emallocTemp (sizeof (struct ArrayOfString));
			if (pas->AccessList == NULL)
				return ERR_RESOURCE_EXCEED;
			if (entry->accessList == NULL)
			{
				pas->AccessList->__ptrstring = &emptyString; // HH 18.2. Sphairon
				pas->AccessList->__size = entry->accessListSize;
			}
			else
			{
				pas->AccessList->__ptrstring =         // HH 18.2. Sphairon
                             entry->accessList;
				pas->AccessList->__size = entry->accessListSize;
			}
			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_PARAMETER, "OrigAccessList: %p size %d\n",
							entry->accessList, entry->accessListSize);
			)

			attributes->__ptrParameterAttributeStruct[(*idx)++] = pas;

			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_PARAMETER, "Found: %s\n", pas->Name);
			)
	}

	/* Go to the actual ParemeterEntry if it is a Object with children
	*/
	if (entry->child != NULL && entry->type != DefaultType)
	{
		ctmp = entry->child;
		while (ctmp != NULL)
		{
			ret = addParameterAttributesStructHelper (attributes, ctmp, idx);
			if (ret != OK)
            	return ret;
        	ctmp = ctmp->next;
		}
	}

	return ret;
}

/** (NEW) Returns the ParameterAttributes for a list of given parameter names
 Objects are expanded to their leaf children
        \param nameList       Structure with the search Parameter pathnames
        \param attributes     Structure to return the found parameters data
        \return        int                  ErrorCode
*/
int
getParametersAttributes (const struct ArrayOfString *nameList,
                       struct ArrayOfParameterAttributeStruct
                       *attributes)
{
        int i = 0;
        int idx = 0;
        int ret = OK;
        int nameArraySize = 0;
        char **nameArray;
        char *name;
        ParameterEntry *tmp = NULL;

        DEBUG_OUTPUT (
        		dbglog (SVR_INFO, DBG_PARAMETER, "getParameterAttributes: paramCounter: %d\n", paramCounter);
        )

        if (nameList == NULL)
               return ERR_INVALID_ARGUMENT;
        if (nameList->__ptrstring == NULL)
               return ERR_INVALID_ARGUMENT;
        nameArraySize = nameList->__size;

        DEBUG_OUTPUT (
        		dbglog (SVR_INFO, DBG_PARAMETER, "nameListe size: %d\n", nameArraySize);
        )
        nameArray = nameList->__ptrstring;
        /* Alloc Memory for all parameters in the name list,
         * // handles the output parameters
         */
        attributes->__ptrParameterAttributeStruct =
               (cwmp__ParameterAttributeStruct **)
               emallocTemp (sizeof (cwmp__ParameterAttributeStruct *) *paramCounter);
        if (attributes->__ptrParameterAttributeStruct == NULL)
               return ERR_RESOURCE_EXCEED;
        /* loop over all searched names
         */
        for (i = 0; i != nameArraySize; i++)
        {
               name = (char *) nameArray[i];
               tmp = findParameterByPath (name);
               if (tmp == NULL)
                      return ERR_INVALID_PARAMETER_NAME;
//             if (tmp->type == MultiObjectType)
//                    return ERR_INVALID_PARAMETER_TYPE;
               ret = addParameterAttributesStructHelper (attributes, tmp, &idx);
               if (ret != OK)
                      return ret;
        }
        attributes->__size = idx;
        return OK;
}

/** (NEW) Helper function to append a Parameter to the parameter list
        \param *path          char * to the complete path of the entry
        \param *newEntry       Ptr to the new Entry.
*/
int
insertParameter (const char *parent, ParameterEntry *newEntry)
{
        ParameterEntry *tmp;
        /* Different handling if parent == 0 because of strtok()
         */
        if (parent != NULL)
        {
               /* create a copy of parentName before we can use it with strtok()
                */
               tmp = findParameterByPath (parent);
               DEBUG_OUTPUT (
            		   dbglog (SVR_INFO, DBG_PARAMETER, "Insert: %s into: %s\n", newEntry->name, tmp->name);
               )
               insertParameterInObject (tmp, newEntry);

               return OK;
        }
        else
        {      /* Insert a Parameter w/o a Parent which means it must be a root Parameter */
               DEBUG_OUTPUT (
            		   dbglog (SVR_INFO, DBG_PARAMETER, "Parent is Null, newEntry: %s\n", newEntry->name);
               )
               tmp = firstParam;

               return insertParameterInObject (tmp, newEntry);
        }
}

int
insertParameterInObject (ParameterEntry *object, ParameterEntry *newEntry)
{
	ParameterEntry *tmp;
	tmp = object->child;

	if (tmp == NULL)
	{
		object->child = newEntry;
		object->child->next = NULL;
		newEntry->parent = object;
		paramCounter++;
		object->noChilds ++;
		return OK;
	}
	while (tmp->next != NULL)
	{
		tmp = tmp->next;
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_PARAMETER, "InsertParameterInObject :%s %s\n", tmp->name, newEntry->name);
	)
	tmp->next = newEntry;
	newEntry->prev = tmp;
	newEntry->parent = object;
	paramCounter++;
	object->noChilds ++;

	return OK;
}

/** Get the whole pathname of a ParameterEntry.
 * Objects ends with a '.'
 The allocated Memory is automatic freed
        \param entry  ParameterEntry
        \return        char * the pathname of entry
*/
static char *
getPathname (const ParameterEntry *entry)
{
	char *pathName = (char *) emallocTemp (MAX_PARAM_PATH_LENGTH);
	if (pathName == NULL)
		return NULL;
	*pathName = '\0';

	if (entry->parent != NULL)
		getParentPath (entry->parent, &pathName);
	strcat (pathName, entry->name);
	/* Add an '.' if it is an Object or an MultiObject ( TR: Page 33 ex. NextLevel )
	 */
	if (entry->isLeaf == false)
		strcat (pathName, ".");

	return pathName;
}

/** Returns the parent path of parameter given in entry.
 The path is returned through path.

        \param entry   Parameter
        \param path           Pointer to a char *
*/
void
getParentPath (ParameterEntry *entry, char **path)
{
	/* Loop until we back in the first entry, which we ignore
	 */
	if (entry->parent != NULL)
	{
		getParentPath (entry->parent, path);
		strcat (*path, entry->name);
		strcat (*path, ".");
	}
}

/** Deletes a parameter and its parameter file if existing
        \param entry   Parameter entry to delete
        \param arg     Not used parameter
        \returns int   ErrorCode
*/
static int
deleteParameter (ParameterEntry *entry, void *arg)
{
	int ret = OK;
	ParameterEntry *next = entry->next;
	ParameterEntry *prev = entry->prev;
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_PARAMETER, "Delete Parameter: %s\n", entry->name);
	)
	if (next != NULL)
		next->prev = prev;
	if (prev != NULL)
		prev->next = next;
	/* If next == prev then all Children are deleted, though we can clear the child list in our parent
	 */
	if (next == prev)
		entry->parent->child = NULL;
	if (entry->type == StringType)
	{
		efree (entry->value.cval);
	}
	freeAccessList (entry);
	/* delete the parameter file, ignore errors maybe the file may not exist
	 */
	ret = removeParameter (getPathname (entry));
	efree (entry);
	paramCounter--;
	return OK;
}

/** Helper function to find a parameterEntry by its pathname
        \param *name                        Name of the searched parameter
        \return        *ParameterEntry      found parameter or NULL
*/
static ParameterEntry *
findParameterByPath (const char *paramPath)
{
	char *ptr = NULL;
	char *parentName;
	char *name;
	char *pathPart;
	ParameterEntry *tmp;
	/* check first for special parameter '.' which defines the root of all Parameter
	 */
	if (strlen (paramPath) == 0 || *paramPath == '.')
		return firstParam;
	ptr = NULL;
	parentName = getParentName (paramPath);
	name = getName (paramPath);
	tmp = firstParam;
	if (tmp == NULL)
		return NULL;
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_PARAMETER, "findParameterByPath: %s / %s\n", parentName, name);
	)
	if (parentName != NULL)
	{
		pathPart = strtok_r (parentName, ".", &ptr);
		while (pathPart != NULL)
		{
			tmp = findParamInObjectByName (tmp, pathPart);
			if (tmp == NULL)
				return NULL;
			pathPart = strtok_r (NULL, ".", &ptr);
		}
	}
	tmp = findParamInObjectByName (tmp, name);
	return tmp;
}

/** Helper function to find a parameterEntry which represents an object by its name
        \param *name                        Name of the searched object typed parameter
        \return        *ParameterEntry               found parameter or NULL
*/
static ParameterEntry *
findParamInObjectByName (const ParameterEntry *object, const char *name)
{
	ParameterEntry *tmp;
	if (object == NULL)
		return NULL;
	tmp = object->child;
	while (tmp != NULL)
	{
		if (strCmp (tmp->name, name))
			return tmp;
		tmp = tmp->next;
	}
	return NULL;
}

/** Helper function for traversing the tree
 * Starting at entry, all objects are visited and a given function is called
*/
static int
traverseParameter (ParameterEntry * entry, bool nextLevel,
                  int (*func) (ParameterEntry *, void *), void *arg)
{
	int ret = OK;
	ParameterEntry *tmp = entry->child;
	ret = func (entry, arg);
	if (ret != OK)
		return ret;
	while (tmp != NULL)
	{
		if (tmp->isLeaf)
		{
			ret = func (tmp, arg);
			if (ret != OK)
				return ret;
		}
		else
		{
			traverseParameter (tmp, nextLevel, func, arg);
		}
		tmp = tmp->next;
	}
	return ret;
}

/** Helper function for traversing the tree from bottom up.
 * Starting at entry, all objects are visited and a given function is called
*/
static int
traverseParameterReverse (ParameterEntry * entry, bool nextLevel,
                        int (*func) (ParameterEntry *, void *), void *arg)
{
 	int ret = OK;
	ParameterEntry *tmp = entry->child;
	while (tmp != NULL)
	{
		if (tmp->isLeaf)
		{
			ret = func (tmp, arg);
			if (ret != OK)
				return ret;
		}
		else
 		{
			traverseParameterReverse (tmp, nextLevel, func, arg);
		}
        tmp = tmp->next;
	}
	ret = func (entry, arg);
	return ret;
}

/** Returns the actual number of parameters
 */
int
getParameterCount (void)
{
	return paramCounter;
}

void
printModifiedParameterList (void)
{
        ParameterEntry *tmp = firstParam;
        while (tmp != NULL)
        {
               if (tmp->status != ValueNotChanged)
                      printParameter (tmp, NULL);
               tmp = tmp->next;
        }
}

int
printParameterName (ParameterEntry * entry, void *arg)
{
        DEBUG_OUTPUT (
        		dbglog (SVR_INFO, DBG_PARAMETER, "\n++ Parameter %s +++++++++\n", entry->name);
        )
        return OK;
}

int
printParameter (ParameterEntry * entry, void *arg)
{
	return ERR_INTERNAL_ERROR;
}

/** Copies the values of the accessList into the accessList of the ParameterEntry
 The size of the created accessList is stored in accessListSize

 \param entry  identifies the ParameterEntry
 \param size   the number of entries in the following accessList
 \param accessList     an char * Array of the new accessList

 \return        OK or ERR_RESOURCE_EXCEED ( emalloc failed )
*/
static int
copyAccessList (ParameterEntry * entry, int size, char **accessList)
{
        int idx = 0;
        /* check that the accessList of entry is empty, if not free     it,
         * the accessList is set to NULL
         */
        if (entry->accessList != NULL)
               freeAccessList (entry);
        // only allocate memory if we have to set a new access list
        if (size > 0)
        {
               entry->accessList =
                      (char **) emalloc (sizeof (char *) * size);
               if (entry->accessList == NULL)
                      return ERR_RESOURCE_EXCEED;
        }
        // loop over the new accesslist values and create a copy of it
        for (idx = 0; idx != size; idx++)
        {
               entry->accessList[idx] =
                      strnDup (entry->accessList[idx], accessList[idx],
                              strlen (accessList[idx]));
               if (entry->accessList[idx] == NULL)
                      return ERR_RESOURCE_EXCEED;
        }
        entry->accessListSize = size;
        return OK;
}

/** Frees the accessList entries of a ParameterEntry
 The number of entries are taken from accessListSize

        \param entry   identifies the ParameterEntry
*/
static void
freeAccessList (ParameterEntry * entry)
{
        int idx = 0;
        if (entry->accessList != NULL)
        {
               for (idx = 0; idx != entry->accessListSize; idx++)
                      efree (entry->accessList[idx]);
               efree (entry->accessList);
               entry->accessList = NULL;
               entry->accessListSize = 0;
        }
}

/** Checks if the given name is the name of an object type.
 * Every object must end with a dot (.)

        \param  name  name of the object
        \returns true  is an object name
                 false is not an object name
*/
static bool
isObject (const char *name)
{
        return (*(name + strlen (name) - 1) == '.');
}

/** Returns the last part of the path
 path is a list of  '.' separated names

        \param *path  char * to a pathname
        \returns      char* of the last part of the path
*/
static char *
getName (const char *path)
{
        char *tmp = NULL;
        char *name = NULL;
        name = strnDupTemp (name, path, strlen (path));
        if (isObject (name))
               *strrchr (name, '.') = '\0';
        tmp = strrchr (name, '.');
        if (tmp != NULL)
               /* skip Path Delimiter
                */
               return (tmp + 1);
        else
               return name;
}

/** Returns a copy the pathname to the parent, which is path before the last part of the path
 path is a list of '.' separated names

        \param *path  char * to a pathname
        \returns      char* up to the last part of the path or NULL
*/
static char *
getParentName (const char *path)
{
        char *tmp, *parentName = NULL;
        parentName = strnDupTemp (parentName, path, strlen (path));
        /* If path references an Object then remove the last PathDelimiter
         */
        if (isObject (parentName))
               *strrchr (parentName, '.') = '\0';
        tmp = strrchr (parentName, '.');
        if (tmp != NULL)
        {
               *tmp = '\0';
               return parentName;
        }
        else
        {
               return NULL;
        }
}
/** Count the number of children in the ParameterEntry
*/
static int
countChilds (ParameterEntry * entry)
{
        register int cnt = 0;
        register ParameterEntry *tmp = entry->child;
        while (tmp != NULL)
        {
               cnt++;
               tmp = tmp->next;
        }
        return cnt;
}

/**     Stores the content of the pointer value in ParameterEntry entry
*/
static int
setAccess2ParamValue (ParameterEntry * entry, ParameterValue *accessValue)
{
        int ret = OK;

        switch (entry->type)
        {
               case IntegerType:
               case DefIntegerType:
                      entry->value.ival = accessValue->out_int;
                      break;
               case UnsignedIntType:
               case DefUnsignedIntType:
                      entry->value.uival = accessValue->out_uint;
                      break;
               case StringType:
               case Base64Type:
               case DefStringType:
               case DefBase64Type:
                      if (accessValue->out_cval != NULL)
                      {
                             if (entry->value.cval != NULL)
                                    efree (entry->value.cval);
                             entry->value.cval = strnDup (entry->value.cval,
                                                          accessValue->out_cval,
                                                          strlen (accessValue->out_cval));

                      }
                      break;
               case BooleanType:
               case DefBooleanType:
                      entry->value.ival = accessValue->out_int;
                      break;
               case DateTimeType:
               case DefDateTimeType:
                      entry->value.tval = accessValue->out_timet;
                      break;
               default:
                      ret = ERR_INVALID_PARAMETER_TYPE;
        }

        return OK;
}
/**     Stores the content of the pointer value in ParameterEntry entry.
 It cares about the read-write ability of entry.
 If entry is write able the new value is stored in cval which is the value returned by getParameter()
 if entry is read only the new value is stored in defaultValue.
*/
static int
setValuePtr (ParameterEntry * entry, void *value)
{
        switch (entry->type)
        {
        case StringType:
        case Base64Type:
               if (entry->writeable == ReadWrite)
               {
                      if (entry->value.cval)
                             efree (entry->value.cval);
                      entry->value.cval =
                             strnDup (entry->value.cval, (char *) value,
                                     strlen ((char *) value));
                      if (entry->value.cval == NULL)
                             return ERR_RESOURCE_EXCEED;
               }
               break;
        case IntegerType:
               if (entry->writeable == ReadWrite)
               {
                      entry->value.ival = *(int *) value;
               }
//             else
//             {
//                    entry->defaultValue.ival = *(int *) value;
//             }
               break;
        default:
               if (entry->writeable == ReadWrite)
               {
                      entry->value.uival = *(unsigned int *) value;
               }
//             else
//             {
//                    entry->defaultValue.uival = *(unsigned int *) value;
//             }
        }
        return OK;
}

/**     Returns        a pointer to the value of entry,
        depending on the access type, the default value or the stored    value is returned.
*/
static void *
getValue (ParameterEntry * entry)
{
#ifdef ACS_REGMAN
        char *charValue = NULL;
#endif

        switch (entry->type)
        {
               case DefStringType:
               case StringType:
               case DefBase64Type:
               case Base64Type:
                      if (entry->writeable == ReadWrite
                          || entry->writeable == ReadOnly )
                             return entry->value.cval;
                      break;
               case DefIntegerType:
               case IntegerType:
#ifdef ACS_REGMAN
                      charValue = (char*)emallocTemp(12);
                      if (entry->writeable == ReadWrite
                             || entry->writeable == ReadOnly)
                             sprintf( charValue, "%d", entry->value.ival);
                      return charValue;
                      break;
#else
                      if (entry->writeable == ReadWrite
                          || entry->writeable == ReadOnly)
                             return &entry->value.ival;
                      break;
#endif
#ifdef ACS_REGMAN
               case DateTimeType:
                      if (entry->writeable == ReadWrite
                             || entry->writeable == ReadOnly)
                             charValue = strnDupTemp( dateTime2s( entry->value.ival), charValue, DATE_TIME_LENGHT);
//                             sprintf( charValue, "%d", entry->value.ival);
                      return charValue;
					  break;
#endif
               default:
#ifdef ACS_REGMAN
                      charValue = (char*)emallocTemp(12);
                      if (entry->writeable == ReadWrite
                          || entry->writeable == ReadOnly)
                             sprintf( charValue, "%u", entry->value.uival);
                      return charValue;
                      break;
#else
                      if (entry->writeable == ReadWrite
                             || entry->writeable == ReadOnly)
                             return &entry->value.uival;
#endif
        }

        return NULL;
}

/**     Returns the value of a parameter as a char* for storage in a file.
*/
static char *
getValueAsString (ParameterEntry * entry)
{
        char *charValue;

        switch (entry->type)
        {
               case DefStringType:
               case StringType:
               case DefBase64Type:
               case Base64Type:
                      if (entry->writeable == ReadWrite
                          || entry->writeable == ReadOnly )
                             return entry->value.cval;
                      break;
               case DefIntegerType:
               case IntegerType:
                      charValue = (char*)emallocTemp(12);
                      if (entry->writeable == ReadWrite
                             || entry->writeable == ReadOnly)
                             sprintf( charValue, "%d", entry->value.ival);
                      return charValue;
                      break;
               default:
                      charValue = (char*)emallocTemp(12);
                      if (entry->writeable == ReadWrite
                          || entry->writeable == ReadOnly)
                             sprintf( charValue, "%u", entry->value.uival);
                      return charValue;
                      break;
        }

        return NULL;
}

static void
setVoidPtr2Access (const ParameterType type, void *value, ParameterValue *accessValue)
{

        switch (type)
        {
               case StringType:
               case Base64Type:
                             accessValue->in_cval = (char*)value;
                      break;
               case BooleanType:
               case IntegerType:
                             accessValue->in_int = *(int*)value;
                      break;
               case UnsignedIntType:
                             accessValue->in_uint = *(unsigned int*)value;
                      break;
               case DateTimeType:
                             accessValue->in_timet = *(time_t*)value;
                      break;
               default:
            	   break;
        }
}
static void
setParamValue2Access (const ParameterType type, ParamValue *value, ParameterValue *accessValue)
{
        switch (type)
        {
               case StringType:
               case Base64Type:
                             accessValue->in_cval = value->cval;
                      break;
               case BooleanType:
               case IntegerType:
                             accessValue->in_int = value->ival;
                      break;
               case UnsignedIntType:
                             accessValue->in_uint = value->uival;
                      break;
               case DateTimeType:
                             accessValue->in_timet = value->tval;
                      break;
               default:
            	   break;
        }
}

static void
setAccess2VoidPtr (const ParameterType type, void *value, ParameterValue *accessValue)
{
        switch (type)
        {
               case StringType:
               case Base64Type:
                             *(char**)value = accessValue->in_cval;
                      break;
               case BooleanType:
               case IntegerType:
#ifdef ACS_REGMAN
                      *(char**)value = (char*)emallocTemp(12);
                      sprintf( *(char**)value, "%d", accessValue->out_int);
                      break;
#else
                             *(int**)value = &accessValue->out_int;
                      break;
#endif
               case UnsignedIntType:
#ifdef ACS_REGMAN
                      *(char**)value = (char*)emallocTemp(12);
                      sprintf( *(char**)value, "%u", accessValue->out_uint);
                      break;
#else
                             *(unsigned int **)value = &accessValue->out_uint;
                      break;
#endif
               case DateTimeType:
#ifdef ACS_REGMAN
                      *(char**)value = strnDupTemp( *(char**)value, dateTime2s( accessValue->out_timet), DATE_TIME_LENGHT);
					  break;
#else
                             *(time_t **)value = &accessValue->out_timet;
#endif
                      break;
               default:
            	   break;
        }
}

/** Stores all parameter data, without the value data into the database
 *  This function is used if a new parameter is created by an addObject().
 *  The data is also stored if the getIdx/writeIdx = 0 and inAddObject == true
 *  o/w there is no chance to store a default value because there is no data file
 *
*/
static int
saveParameter (ParameterEntry * entry, void *arg)
{
        int ret = OK;
        int idx, aclSize;

        char buf[MAX_PATH_NAME_SIZE];
        ret = sprintf (buf, "%s;%d;%d;%d;%d;%d;%d;%d;%d;",
                      getPathname (entry), entry->type, entry->instance,
                      entry->notification, entry->notificationMax, entry->rebootOnChange,
                      entry->initDataIdx, entry->getDataIdx,
                      entry->setDataIdx);
        aclSize = entry->accessListSize;
        if ( aclSize > 0)
        {
               for (idx = 0; idx != aclSize; idx++)
               {
                      strcat( buf, entry->accessList[idx]);
                      strcat( buf, "|" );
               }
        }
        strcat(buf, ";" );
        // if in addObject and memory data storage is on
        // store any default value in the parameter file
        if ( inAddObject == true && entry->getDataIdx == 0  ) {
			strcat(buf, getValueAsString(entry) );
        }
		strcat(buf, "\n" );

        ret = storeParameter( getPathname (entry), buf );
        return ret;
}

/** Stores only the volatile metadata of a parameter to the database
 */
static int
saveMetaParameter (ParameterEntry * entry, void *arg)
{
        int ret = OK;
        int idx, aclSize;

        char buf[MAX_PATH_NAME_SIZE];
        ret = sprintf (buf, "$;%d;%d;", entry->instance, entry->notification );
        aclSize = entry->accessListSize;
        if ( aclSize > 0)
        {
               for (idx = 0; idx != aclSize; idx++)
               {
                      strcat( buf, entry->accessList[idx]);
                      strcat( buf, "|" );
               }
        }
        strcat(buf, ";\n" );
        ret = storeParameter( getPathname (entry), buf );
        return ret;
}

static int
newParam1 (char *name, char *data)
{
        int ret = OK;
        char *bufPtr;
        char *path;
        char *accessList;
        ParameterType type;
        int instance;
        NotificationType notification;
        NotificationMax  notificationMax;
        RebootType reboot;
        int initIdx;
        int getIdx;
        int setIdx;
        ParamValue value;

        // Skip comments or empty lines
        if ( data[0] == '#' || data[0] == ' ' || data[0] == '\n' || data[0] == '\0' )
           return OK;
        if ( data[0] == '$' ){
           // read a parameter update set
           bufPtr = data;
           // skip the update set sign character in the first column
           strsep (&bufPtr, ";");
           instance = a2i (strsep (&bufPtr, ";"));
           notification = a2i (strsep (&bufPtr, ";"));
           accessList = strsep (&bufPtr, ";");
           updParameter( name, instance, notification, accessList );
        } else {
		   // read a complete parameter set
		   bufPtr = data;
		   path = strsep (&bufPtr, ";");
		   type = a2i (strsep (&bufPtr, ";"));
		   instance = a2i (strsep (&bufPtr, ";"));
		   notification = a2i (strsep (&bufPtr, ";"));
		   notificationMax = a2i (strsep (&bufPtr, ";"));
		   reboot = a2i (strsep (&bufPtr, ";"));
		   initIdx = a2i (strsep (&bufPtr, ";"));
		   getIdx = a2i (strsep (&bufPtr, ";"));
		   setIdx = a2i (strsep (&bufPtr, ";"));
		   accessList = strsep (&bufPtr, ";");
		   // Data comes from tmp.param file
//		   if ( name == NULL ) {
			  switch (type)
			  {
				 case DefIntegerType:
				 case DefUnsignedIntType:
				 case DefBooleanType:
				        value.ival = a2i (bufPtr);
				        break;
				 case IntegerType:
				 case UnsignedIntType:
				 case BooleanType:
				        value.ival = a2i (bufPtr);
				        break;
				 case DefStringType:
				 case DefBase64Type:
				        value.cval = bufPtr;
				        break;
				 case StringType:
				 case Base64Type:
				        value.cval = bufPtr;
				        break;
				 case DefDateTimeType:
				        value.tval = a2l (bufPtr);
				        break;
				 case DateTimeType:
				        value.tval = a2l (bufPtr);
				        break;
				 case ObjectType:
				 case MultiObjectType:
				 case DefaultType:
				 default:
				        value.cval = "";
				            break;
			   }
//			}
			ret = newParameter (path, type, instance, reboot, notification,notificationMax,
		                 initIdx, getIdx, setIdx, accessList, &value, paramBootstrap);
        }
        efreeTemp();
        return ret;
}

static int
newParam2 (char *name, char *data)
{
        int ret = OK;
#if 0
        char *bufPtr;
        char *path;
        char *accessList;
        ParameterType type;
        int instance;
        NotificationType notification;
        NotificationMax  notificationMax;
        RebootType reboot;
        int initIdx;
        int getIdx;
        int setIdx;
        ParamValue value;

        // Skip comments or empty lines
        if ( data[0] == '#' || data[0] == ' ' || data[0] == '\n' || data[0] == '\0' )
           return OK;
        if ( data[0] == '$' ){
           // read a parameter update set
           bufPtr = data;
           // skip the update set sign character in the first column
           strsep (&bufPtr, ";");
           instance = a2i (strsep (&bufPtr, ";"));
           notification = a2i (strsep (&bufPtr, ";"));
           accessList = strsep (&bufPtr, ";");
           updParameter( name, instance, notification, accessList );
        } else {
		   // read a complete parameter set
		   bufPtr = data;
		   path = strsep (&bufPtr, ";");
		   type = a2i (strsep (&bufPtr, ";"));
		   instance = a2i (strsep (&bufPtr, ";"));
		   notification = a2i (strsep (&bufPtr, ";"));
		   notificationMax = a2i (strsep (&bufPtr, ";"));
		   reboot = a2i (strsep (&bufPtr, ";"));
		   initIdx = a2i (strsep (&bufPtr, ";"));
		   getIdx = a2i (strsep (&bufPtr, ";"));
		   setIdx = a2i (strsep (&bufPtr, ";"));
		   accessList = strsep (&bufPtr, ";");
		   // Data comes from tmp.param file
//		   if ( name == NULL ) {
			  switch (type)
			  {
				 case DefIntegerType:
				 case DefUnsignedIntType:
				 case DefBooleanType:
				        value.ival = a2i (bufPtr);
				        break;
				 case IntegerType:
				 case UnsignedIntType:
				 case BooleanType:
				        value.ival = a2i (bufPtr);
				        break;
				 case DefStringType:
				 case DefBase64Type:
				        value.cval = bufPtr;
				        break;
				 case StringType:
				 case Base64Type:
				        value.cval = bufPtr;
				        break;
				 case DefDateTimeType:
				        value.tval = a2l (bufPtr);
				        break;
				 case DateTimeType:
				        value.tval = a2l (bufPtr);
				        break;
				 case ObjectType:
				 case MultiObjectType:
				 case DefaultType:
				 default:
				        value.cval = "";
				            break;
			   }
//			}
			ret = newParameter (path, type, instance, reboot, notification,notificationMax,
		                 initIdx, getIdx, setIdx, accessList, &value, paramBootstrap);
        }
        efreeTemp();
#endif
        return ret;
}

int
loadParameters (bool bootstrap)
{
	int ret = OK;
	ParamValue dimClientId;
	ParamValue dimClientVersion;

	char buf[MAX_PATH_NAME_SIZE + 1];

	paramBootstrap = bootstrap;

	// Load the initial Parameters with the whole Structure
	ret = loadInitialParameterFile( (newParam *)&newParam1 );
	newParam2 (NULL, NULL);
	paramBootstrap = false;
/*
	dimClientId.ival = 83620;
	strcpy( buf, DEVICE_INFO );
	strcat( buf, "X_DIMARK_COM_ClientID" );
	newParameter( buf, IntegerType, 0, NoReboot, NotificationAllways, NotificationAllways, -1, 0, -1, "", &dimClientId, true);
	dimClientVersion.cval = "Agent4.0";
	strcpy( buf, DEVICE_INFO );
	strcat( buf, "X_DIMARK_COM_Version" );
	newParameter( buf, StringType, 0, NoReboot, NotificationAllways, NotificationAllways, -1, 0, -1, "", &dimClientVersion, true );
*/
	return ret;
}

int
resetAllParameters (void)
{
	int ret = OK;

	ret = removeAllParameters();  // deleteParameters (PERSISTENT_PARAMETER_DIR);

	return ret;
}

/** Counts the pieces in the given string.
 * The pieces are separated by a '|' character.
 *
 *      \param data           String to split, delimiter is '|'
 *
 *      \returns int   Number of pieces
 */
static int
countPieces (char *data)
{
        register char *ptr;
        register int cnt = 0;
        ptr = data;
        /* count the number of access list entries
         * separated by the '|'character
         */
        while (*ptr != '\0')
        {
               if (*ptr++ == '|')
                      cnt++;
        }
        return cnt;
}

/** Calls the initAccess function of the parameter in entry.
 * The function is only called if the parameter initDataIdx value is > 0
 *
 *      \param entry  pointer to parameter
 *      \param param  pointer to parameter value
 *
 *      \return        int           Return code of the function call
 */
static int
callInitFunction( ParameterEntry *entry, void *param )
{
        int ret = OK;
        if ( entry->initDataIdx > 0)
        {
			ret = initAccess (entry->initDataIdx, getPathname (entry), entry->type, param);
        }
        return ret;
}

/** Calls the deleteAccess function of the parameter in entry.
 * The function is only called if the parameter initDataIdx value is > 0
 *
 *      \param entry  pointer to parameter
 *      \param param  pointer to parameter value
 *
 *      \return        int           Return code of the function call
 */
static int
callDeleteFunction( ParameterEntry *entry, void *param )
{
        int ret = OK;
        if ( entry->initDataIdx > 0)
        {
               ret = deleteAccess (entry->initDataIdx, getPathname (entry), entry->type, param);
        }
        return ret;
}

/** Compare the value of the parameter entry with value
 * San 8 june 2011: Returns TRUE if two parameters are equal, else returns FALSE.
 */
static bool
compareValue ( ParameterEntry *entry, void *value )
{
	if ((entry == NULL)||(value == NULL)) return false;
        switch( entry->type ) {
               case DefIntegerType:
               case IntegerType:
               case BooleanType:
               case DefBooleanType:
                      return entry->value.ival == INT_SET value;
                      break;
               case UnsignedIntType:
               case DefUnsignedIntType:
                       return entry->value.uival == UINT_SET value;
                       break;
               case StringType:
               case DefStringType:
               case Base64Type:
               case DefBase64Type:
               {
            	   if((entry->value.cval == NULL)&&((STRING_GET value) == NULL)) // If two values are NULL, that return TRUE
            		   return true;

            	   if((entry->value.cval == NULL)||((STRING_GET value) == NULL)) // If one from two values are NULL, that return FALSE
            		   return false;
            	   return strCmp( entry->value.cval,  STRING_GET value);
            	   break;
               }
               case DateTimeType:
               case DefDateTimeType:
                      return entry->value.tval == TIME_SET value;
                      break;
               default:
            	   break;
        }

        return false;
}

static int traverseTreeAndCheckPassiveNotification(ParameterEntry * entry)
{
	int ret = OK;
	ParameterEntry *tmp = entry->child;
	while (tmp != NULL)
	{
		if (tmp->isLeaf)
		{
			char *strFromGetter = NULL;
			int * intValueFromGetter = NULL;
			unsigned int * uintValueFromGetter = NULL;
			time_t * ptime = NULL;
			bool notification = false;
			char new_value[256] = {'\0'};

			/* find parameter with passive notification */
			if((tmp->notification & 3)== NotificationPassive)
			{
				/* Call getter function */
				if( (tmp->type == UnsignedIntType) ||
					(tmp->type == DefUnsignedIntType)
				  )
				{
					/* use uintValueFromGetter */
					if(getParameter(getPathname(tmp), &uintValueFromGetter) == OK)
					{
						if(compareValue (tmp, &uintValueFromGetter) == false)
						{
							/* yes, value is different */
							if(uintValueFromGetter != NULL)
							{
								memset(new_value, 0, sizeof(new_value));
								sprintf(new_value, "%u", *uintValueFromGetter);
								setParameter2HostForNotifications (getPathname(tmp), &notification, new_value);
							}
						}
					}
				}
				if( (tmp->type == DefIntegerType) ||
					(tmp->type == IntegerType ) ||
					(tmp->type == BooleanType) ||
					(tmp->type == DefBooleanType) ||
					(tmp->type == DateTimeType) ||			//San TODO: Coment for type time_t
					(tmp->type == DefDateTimeType) )		//San TODO: Coment for type time_t
				{
					/* use intValueFromGetter */
					if(getParameter(getPathname(tmp), &intValueFromGetter) == OK)
					{
						if(compareValue (tmp, &intValueFromGetter) == false)
						{
							/* yes, value is different */
							if(intValueFromGetter != NULL)
							{
								memset(new_value, 0, sizeof(new_value));
								sprintf(new_value, "%d", *intValueFromGetter);
								setParameter2HostForNotifications (getPathname(tmp), &notification, new_value);
							}
						}
					}
				}
//				San TODO for type time_t
//				if( (tmp->type == DateTimeType) ||
//					(tmp->type == DefDateTimeType) )
//				{
//					/* use ptime */
//					if(getParameter(getPathname(tmp), &ptime) == OK)
//					{
//						if(compareValue (tmp, &ptime) == false)
//						{
//							/* yes, value is different */
//							if(ptime != NULL)
//							{
//								memset(new_value, 0, sizeof(new_value));
//								//sprintf(new_value, "%d", ptime);
//								setParameter2Host (getPathname(tmp), &notification, new_value);
//							}
//						}
//					}
//				}

				if( (tmp->type == StringType) ||
					(tmp->type == DefStringType) ||
					(tmp->type == Base64Type) ||
					(tmp->type == DefBase64Type) )
				{
					/* use strFromGetter */
					if(getParameter (getPathname(tmp), &strFromGetter) == OK)
					{
						if(compareValue (tmp, &strFromGetter) == false)
						{
							/* yes, value is different */
							if(strFromGetter != NULL)
							{
								/* clear buffer */
								memset(new_value, 0, sizeof(new_value));
								strcpy(new_value, strFromGetter);
								setParameter2HostForNotifications (getPathname(tmp), &notification, new_value);
							}
						}
					}
				}
			}
		}  // end  if (tmp->isLeaf)
		else
			traverseTreeAndCheckPassiveNotification(tmp);
		tmp = tmp->next;
	} // end while (tmp != NULL)
	return ret;
}

void checkPassiveNotification()
{
	ParameterEntry *pEntry = &rootEntry;
	if(!pEntry)
		return ;
	traverseTreeAndCheckPassiveNotification(pEntry);
}

static int traverseTreeAndCheckActiveNotification(ParameterEntry * entry)
{
	int ret = OK;
	ParameterEntry *tmp = entry->child;
	while (tmp != NULL)
	{
		if (tmp->isLeaf)
		{
			char *strFromGetter = NULL;
			int * intValueFromGetter = NULL;
			unsigned int * uintValueFromGetter = NULL;
			time_t * ptime = NULL;
			bool notification = false;
			char new_value[256] = {'\0'};

			/* find parameter with passive notification */
			if((tmp->notification & 3) == NotificationActive)
			{
				/* Call getter function */
				if( (tmp->type == UnsignedIntType) ||
					(tmp->type == DefUnsignedIntType)
				  )
				{
					/* use uintValueFromGetter */
					if(getParameter(getPathname(tmp), &uintValueFromGetter) == OK)
					{
//						dbglog (SVR_DEBUG, DBG_PARAMETER, "new =%x old=%x\n", *uintValueFromGetter, tmp->value.ival);
						if(compareValue (tmp, &uintValueFromGetter) == false)
						{
							/* yes, value is different */
							if(uintValueFromGetter != NULL)
							{
								sendActiveNofi = true;
								memset(new_value, 0, sizeof(new_value));
								sprintf(new_value, "%u", *uintValueFromGetter);
								setParameter2HostForNotifications (getPathname(tmp), &notification, new_value);
							}
						}
					}
				}
				if( (tmp->type == DefIntegerType) ||
					(tmp->type == IntegerType ) ||
					(tmp->type == BooleanType) ||
					(tmp->type == DefBooleanType) ||
					(tmp->type == DateTimeType) ||			//San TODO: Coment for type time_t
					(tmp->type == DefDateTimeType) )		//San TODO: Coment for type time_t
				{
					/* use intValueFromGetter */
					if(getParameter(getPathname(tmp), &intValueFromGetter) == OK)
					{
//						dbglog (SVR_DEBUG, DBG_PARAMETER, "new =%x old=%x\n", *intValueFromGetter, tmp->value.ival);
						if(compareValue (tmp, &intValueFromGetter) == false)
						{
							/* yes, value is different */
							if(intValueFromGetter != NULL)
							{
								sendActiveNofi = true;
								memset(new_value, 0, sizeof(new_value));
								sprintf(new_value, "%d", *intValueFromGetter);
								setParameter2HostForNotifications (getPathname(tmp), &notification, new_value);
							}
						}
					}
				}
//				San TODO for type time_t
//				if( (tmp->type == DateTimeType) ||
//					(tmp->type == DefDateTimeType) )
//				{
//					/* use ptime */
//					if(getParameter(getPathname(tmp), &ptime) == OK)
//					{
//						if(compareValue (tmp, &ptime) == false)
//						{
//							/* yes, value is different */
//							if(ptime != NULL)
//							{
//								memset(new_value, 0, sizeof(new_value));
//								//sprintf(new_value, "%d", ptime);
//								setParameter2Host (getPathname(tmp), &notification, new_value);
//							}
//						}
//					}
//				}

				if( (tmp->type == StringType) ||
					(tmp->type == DefStringType) ||
					(tmp->type == Base64Type) ||
					(tmp->type == DefBase64Type) )
				{
					/* use strFromGetter */
					if(getParameter (getPathname(tmp), &strFromGetter) == OK)
					{
//						dbglog (SVR_DEBUG, DBG_PARAMETER, "new =%s old=%s\n", strFromGetter, tmp->value.cval);
						if(compareValue (tmp, &strFromGetter) == false)
						{
							/* yes, value is different */
							if(strFromGetter != NULL)
							{
								sendActiveNofi = true;
								/* clear buffer */
								memset(new_value, 0, sizeof(new_value));
								strcpy(new_value, strFromGetter);
								setParameter2HostForNotifications (getPathname(tmp), &notification, new_value);
							}
						}
					}
				}
			}
		} // end  if (tmp->isLeaf)
		else
			traverseTreeAndCheckActiveNotification(tmp);
		tmp = tmp->next;
	}  // end while (tmp != NULL)
	return ret;
}

void checkActiveNotification()
{
	ParameterEntry *pEntry = &rootEntry;
	if(!pEntry)
		return ;
	traverseTreeAndCheckActiveNotification(pEntry);

#ifdef PLATFORM_PLATYPUS
	efreeTemp();
#endif
}

/* San. 31 may 2011.
 * The function returns number of entries for static!!! branch, which corresponds to parameter with name = pathNameOfParam.
 * If nameOfBranch == null, than function takes name of branch from pathNameOfParam, else pathNameOfParam is ignored.
 * Return < 0  if error.
 *
 */
int getNumberOfEntries(char * nameOfBranch, const char * pathNameOfParam)
{
	char tmpNameOfBranch[MAX_PATH_NAME_SIZE] = {'\0'};

//	DEBUG_OUTPUT (
//			dbglog (SVR_DEBUG, DBG_PARAMETER, "getNumberOfEntries() 1.\n");
//	)

	if (!nameOfBranch)
	{
		// Take name of branch from pathNameOfParam.
		if (!pathNameOfParam)
		{
			// If nameOfBranch == pathNameOfParam == NULL function does not continue to work.
			DEBUG_OUTPUT (
					dbglog (SVR_ERROR, DBG_PARAMETER, "getNumberOfEntries() error: nameOfBranch == pathNameOfParam == NULL.\n");
			)
			return -1;
		}

//		DEBUG_OUTPUT (
//				dbglog (SVR_DEBUG, DBG_PARAMETER, "getNumberOfEntries() 2.\n");
//		)

		char * pointNumberOfEntries = strstr(pathNameOfParam, "NumberOfEntries");

		if (!pointNumberOfEntries)
		{
			// If parameter isn't Number Of Entries function does not continue to work.
			DEBUG_OUTPUT (
					dbglog (SVR_ERROR, DBG_PARAMETER, "getNumberOfEntries() error: parameter isn't Number Of Entries.\n");
			)
			return -1;
		}

//		DEBUG_OUTPUT (
//				dbglog (SVR_DEBUG, DBG_PARAMETER, "getNumberOfEntries() 3.\n");
//		)

		int len = pointNumberOfEntries - pathNameOfParam;

//		DEBUG_OUTPUT (
//				dbglog (SVR_DEBUG, DBG_PARAMETER, "getNumberOfEntries() 4. len = %i\n pointNumberOfEntries = %i\n pathNameOfParam = %i\n", len, pointNumberOfEntries, pathNameOfParam);
//		)

		strncpy(tmpNameOfBranch, pathNameOfParam, len);

//		DEBUG_OUTPUT (
//				dbglog (SVR_DEBUG, DBG_PARAMETER, "getNumberOfEntries() 5.\n");
//		)

	}
	else
	{
		strcpy(tmpNameOfBranch, nameOfBranch);
	}

	if (!tmpNameOfBranch)
	{
		// If nameOfBranch == NULL function does not continue to work.
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_PARAMETER, "getNumberOfEntries() error: nameOfBranch == NULL.\n");
		)
		return -1;
	}

//	DEBUG_OUTPUT (
//			dbglog (SVR_DEBUG, DBG_PARAMETER, "getNumberOfEntries() 6.\n");
//	)

    unsigned int count = 0;
    ParameterEntry *pEntry = NULL;
    do
    {
    	count++;
        int  nameLen = strlen(tmpNameOfBranch);
        char tmpNameOfBranch2[nameLen+5];
        sprintf(tmpNameOfBranch2,"%s.%u",tmpNameOfBranch,count);


//    	DEBUG_OUTPUT (
//				dbglog (SVR_DEBUG, DBG_PARAMETER, "getNumberOfEntries() nameOfBranch == %s\n",tmpNameOfBranch2);
//			)


    	pEntry = findParameterByPath(tmpNameOfBranch2);
    }
    while(pEntry);

//	DEBUG_OUTPUT (
//			dbglog (SVR_DEBUG, DBG_PARAMETER, "getNumberOfEntries() 7.\n");
//	)

    count--;

	DEBUG_OUTPUT (
			dbglog (SVR_DEBUG, DBG_PARAMETER, "getNumberOfEntries() exit. count = %i\n",count);
		)

	return  count;
}
