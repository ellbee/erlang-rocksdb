// Copyright (c) 2011-2013 Basho Technologies, Inc. All Rights Reserved.
// Copyright (c) 2016-2017 Benoit Chesneau
//
// This file is provided to you under the Apache License,
// Version 2.0 (the "License"); you may not use this file
// except in compliance with the License.  You may obtain
// a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once
#ifndef INCL_REFOBJECTS_H
#define INCL_REFOBJECTS_H

#include <memory>
#include <stdint.h>
#include <list>

#include "erl_nif.h"
#include "mutex.h"

namespace rocksdb {
    class DB;
    class ColumnFamilyHandle;
    class Snapshot;
    class Iterator;
    class TransactionLogIterator;
    class BackupEngine;
    class Slice;
}

namespace erocksdb {

/**
 * Simple wrapper around an Erlang Environment that can
 * be stored in a shared pointer
 */
class ErlEnvCtr {
    public:

        ErlNifEnv *env;

        ErlEnvCtr() {
            env = enif_alloc_env();
        };

        ~ErlEnvCtr() {
            enif_free_env(env);
        }
};

/**
 * Base class for any object that offers RefInc / RefDec interface
 */

class RefObject
{
public:

protected:
    volatile uint32_t m_RefCount;     //!< simple count of reference, auto delete at zero

public:
    RefObject();

    virtual ~RefObject();

    virtual uint32_t RefInc();

    virtual uint32_t RefDec();

private:
    RefObject(const RefObject&);              // nocopy
    RefObject& operator=(const RefObject&);   // nocopyassign
};  // class RefObject


/**
 * Base class for any object that is managed as an Erlang reference
 */

class ErlRefObject : public RefObject
{
public:
    // these member objects are public to simplify
    //  access by statics and external APIs
    //  (yes, wrapper functions would be welcome)
    volatile uint32_t m_CloseRequested;  // 1 once api close called, 2 once thread starts destructor, 3 destructor done

    // DO NOT USE CONTAINER OBJECTS
    //  ... these must be live after destructor called
    pthread_mutex_t m_CloseMutex;        //!< for erlang forced close
    pthread_cond_t  m_CloseCond;         //!< for erlang forced close

protected:


public:
    ErlRefObject();

    virtual ~ErlRefObject();

    virtual uint32_t RefDec();

    // allows for secondary close actions IF InitiateCloseRequest returns true
    virtual void Shutdown()=0;

    // the following will sometimes be called AFTER the
    //  destructor ... in which case the vtable is not valid
    static bool InitiateCloseRequest(ErlRefObject * Object);

    static void AwaitCloseAndDestructor(ErlRefObject * Object);


private:
    ErlRefObject(const ErlRefObject&);              // nocopy
    ErlRefObject& operator=(const ErlRefObject&);   // nocopyassign
};  // class RefObject


/**
 * Class to manage access and counting of references
 * to a reference object.
 */

template <class TargetT>
class ReferencePtr
{
    TargetT * t;

public:
    ReferencePtr()
        : t(NULL)
    {};

    ReferencePtr(TargetT *_t)
        : t(_t)
    {
        if (NULL!=t)
            t->RefInc();
    }

    ReferencePtr(const ReferencePtr &rhs)
    {t=rhs.t; if (NULL!=t) t->RefInc();};

    ~ReferencePtr()
    {
        if (NULL!=t)
            t->RefDec();
    }

    void assign(TargetT * _t)
    {
        if (_t!=t)
        {
            if (NULL!=t)
                t->RefDec();
            t=_t;
            if (NULL!=t)
                t->RefInc();
        }   // if
    };

    TargetT * get() {return(t);};

    TargetT * operator->() {return(t);};

private:
 ReferencePtr & operator=(const ReferencePtr & rhs); // no assignment

};  // ReferencePtr


/**
 * Per database object.  Created as erlang reference.
 *
 * Extra reference count created upon initialization, released on close.
 */
class DbObject : public ErlRefObject
{
public:
    rocksdb::DB* m_Db;                                   // NULL or rocksdb database object

    Mutex m_ItrMutex;                         //!< mutex protecting m_ItrList
    Mutex m_SnapshotMutex;                    //!< mutext protecting m_SnapshotList
    Mutex m_ColumnFamilyMutex;                //!< mutex ptotecting m_ColumnFamily
    Mutex m_TLogItrMutex;              //!< mutex ptotecting m_TransactionLogList

    std::list<class ItrObject *> m_ItrList;   //!< ItrObjects holding ref count to this
    std::list<class SnapshotObject *> m_SnapshotList;
    std::list<class ColumnFamilyObject *> m_ColumnFamilyList;
    std::list<class TLogItrObject *> m_TLogItrList;

protected:
    static ErlNifResourceType* m_Db_RESOURCE;

public:
    DbObject(rocksdb::DB * DbPtr); // Open with default CF

    virtual ~DbObject();

    virtual void Shutdown();

    // manual back link to Snapshot ColumnFamilyObject holding reference to this
    void AddColumnFamilyReference(class ColumnFamilyObject *);

    void RemoveColumnFamilyReference(class ColumnFamilyObject *);


    // manual back link to ItrObjects holding reference to this
    bool AddReference(class ItrObject *);

    void RemoveReference(class ItrObject *);

     // manual back link to Snapshot DbObjects holding reference to this
    void AddSnapshotReference(class SnapshotObject *);

    void RemoveSnapshotReference(class SnapshotObject *);

    // manual back link to ItrObjects holding reference to this
    void AddTLogReference(class TLogItrObject *);

    void RemoveTLogReference(class TLogItrObject *);

    static void CreateDbObjectType(ErlNifEnv * Env);

    static DbObject * CreateDbObject(rocksdb::DB * Db);

    static DbObject * RetrieveDbObject(ErlNifEnv * Env, const ERL_NIF_TERM & DbTerm);

    static void DbObjectResourceCleanup(ErlNifEnv *Env, void * Arg);

private:
    DbObject();
    DbObject(const DbObject&);              // nocopy
    DbObject& operator=(const DbObject&);   // nocopyassign
};  // class DbObject

/**
 * Per ColumnFamilyObject object.  Created as erlang reference.
 */
class ColumnFamilyObject : public ErlRefObject
{
public:
    rocksdb::ColumnFamilyHandle* m_ColumnFamily;
    ReferencePtr<DbObject> m_DbPtr;

protected:
    static ErlNifResourceType* m_ColumnFamily_RESOURCE;

public:
    ColumnFamilyObject(DbObject * Db, rocksdb::ColumnFamilyHandle* Handle);

    virtual ~ColumnFamilyObject(); // needs to perform free_itr

    virtual void Shutdown();

    static void CreateColumnFamilyObjectType(ErlNifEnv * Env);

    static ColumnFamilyObject * CreateColumnFamilyObject(DbObject * Db, rocksdb::ColumnFamilyHandle* m_ColumnFamily);

    static ColumnFamilyObject * RetrieveColumnFamilyObject(ErlNifEnv * Env, const ERL_NIF_TERM & DbTerm);

    static void ColumnFamilyObjectResourceCleanup(ErlNifEnv *Env, void * Arg);

private:
    ColumnFamilyObject();
    ColumnFamilyObject(const ColumnFamilyObject &);            // no copy
    ColumnFamilyObject & operator=(const ColumnFamilyObject &); // no assignment
};  // class ColumnFamilyObject

/**
 * Per Snapshot object.  Created as erlang reference.
 */
class SnapshotObject : public ErlRefObject
{
public:
    const rocksdb::Snapshot* m_Snapshot;

    ReferencePtr<DbObject> m_DbPtr;

    Mutex m_ItrMutex;                         //!< mutex protecting m_ItrList
    std::list<class ItrObject *> m_ItrList;   //!< ItrObjects holding ref count to this

protected:
    static ErlNifResourceType* m_DbSnapshot_RESOURCE;

public:
    SnapshotObject(DbObject * Db, const rocksdb::Snapshot * Snapshot);

    virtual ~SnapshotObject(); // needs to perform free_itr

    virtual void Shutdown();

    static void CreateSnapshotObjectType(ErlNifEnv * Env);

    static SnapshotObject * CreateSnapshotObject(DbObject * Db, const rocksdb::Snapshot* Snapshot);

    static SnapshotObject * RetrieveSnapshotObject(ErlNifEnv * Env, const ERL_NIF_TERM & DbTerm);

    static void SnapshotObjectResourceCleanup(ErlNifEnv *Env, void * Arg);

private:
    SnapshotObject();
    SnapshotObject(const SnapshotObject &);            // no copy
    SnapshotObject & operator=(const SnapshotObject &); // no assignment
};  // class SnapshotObject



/**
 * Per Iterator object.  Created as erlang reference.
 */
class ItrObject : public ErlRefObject
{
public:
    rocksdb::Iterator * m_Iterator;
    std::shared_ptr<erocksdb::ErlEnvCtr> env;
    ReferencePtr<DbObject> m_DbPtr;

    rocksdb::Slice *upper_bound_slice;
    rocksdb::Slice *lower_bound_slice;

protected:
    static ErlNifResourceType* m_Itr_RESOURCE;

public:
    ItrObject(DbObject *, std::shared_ptr<erocksdb::ErlEnvCtr> Env, rocksdb::Iterator * Iterator);

    virtual ~ItrObject(); // needs to perform free_itr

    virtual void Shutdown();

    static void CreateItrObjectType(ErlNifEnv * Env);

    static ItrObject * CreateItrObject(DbObject * Db, std::shared_ptr<erocksdb::ErlEnvCtr> Env, rocksdb::Iterator * Iterator);

    static ItrObject * RetrieveItrObject(ErlNifEnv * Env, const ERL_NIF_TERM & DbTerm,
                                         bool ItrClosing=false);

    static void ItrObjectResourceCleanup(ErlNifEnv *Env, void * Arg);

    void SetUpperBoundSlice(rocksdb::Slice*);

    void SetLowerBoundSlice(rocksdb::Slice*);



private:
    ItrObject();
    ItrObject(const ItrObject &);            // no copy
    ItrObject & operator=(const ItrObject &); // no assignment
};  // class ItrObject

/**
 * Per Iterator object.  Created as erlang reference.
 */
class TLogItrObject : public ErlRefObject
{
public:
    rocksdb::TransactionLogIterator * m_Iter;
    ReferencePtr<DbObject> m_DbPtr;

protected:
    static ErlNifResourceType* m_TLogItr_RESOURCE;

public:
    TLogItrObject(DbObject *, rocksdb::TransactionLogIterator * Itr);

    virtual ~TLogItrObject(); // needs to perform free_itr

    virtual void Shutdown();

    static void CreateTLogItrObjectType(ErlNifEnv * Env);

    static TLogItrObject * CreateTLogItrObject(DbObject * Db, rocksdb::TransactionLogIterator * Itr);

    static TLogItrObject * RetrieveTLogItrObject(ErlNifEnv * Env, const ERL_NIF_TERM & DbTerme);

    static void TLogItrObjectResourceCleanup(ErlNifEnv *Env, void * Arg);

private:
    TLogItrObject();
    TLogItrObject(const TLogItrObject &);            // no copy
    TLogItrObject & operator=(const TLogItrObject &); // no assignment
};  // class TLogItrObject



/**
 * BackupEngine object.  Created as erlang reference.
 */
class BackupEngineObject : public ErlRefObject
{
public:
    rocksdb::BackupEngine* m_BackupEngine;                                   // NULL or rocksdb BackupEngine object


protected:
    static ErlNifResourceType* m_BackupEngine_RESOURCE;

public:
    BackupEngineObject(rocksdb::BackupEngine * BackupEnginePtr);

    virtual ~BackupEngineObject();

    virtual void Shutdown();

    static void CreateBackupEngineObjectType(ErlNifEnv * Env);

    static BackupEngineObject * CreateBackupEngineObject(rocksdb::BackupEngine * BackupEngine);

    static BackupEngineObject * RetrieveBackupEngineObject(ErlNifEnv * Env, const ERL_NIF_TERM & DbTerm);

    static void BackupEngineObjectResourceCleanup(ErlNifEnv *Env, void * Arg);

private:
    BackupEngineObject();
    BackupEngineObject(const BackupEngineObject&);              // nocopy
    BackupEngineObject& operator=(const BackupEngineObject&);   // nocopyassign
};  // class BackupEngineObject

} // namespace erocksdb


#endif  // INCL_REFOBJECTS_H
