/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000-2001 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 */

#include "beos/locks.h"
#include "apr_strings.h"
#include "apr_portable.h"


/* At present we only have one implementation, so here it is :) */
static apr_status_t _lock_cleanup(void * data)
{
    apr_lock_t *lock = (apr_lock_t*)data;
    if (lock->ben != 0) {
        /* we're still locked... */
    	while (atomic_add(&lock->ben , -1) > 1){
    	    /* OK we had more than one person waiting on the lock so 
    	     * the sem is also locked. Release it until we have no more
    	     * locks left.
    	     */
            release_sem (lock->sem);
    	}
    }
    delete_sem(lock->sem);
    return APR_SUCCESS;
}    

static apr_status_t _create_lock(apr_lock_t *new)
{
    int32 stat;
    
    if ((stat = create_sem(0, "apr_lock")) < B_NO_ERROR) {
        _lock_cleanup(new);
        return stat;
    }
    new->ben = 0;
    new->sem = stat;
    apr_pool_cleanup_register(new->pool, (void *)new, _lock_cleanup,
                              apr_pool_cleanup_null);
    return APR_SUCCESS;
}

static apr_status_t _lock(apr_lock_t *lock)
{
    int32 stat;
    
	if (atomic_add(&lock->ben, 1) > 0) {
		if ((stat = acquire_sem(lock->sem)) < B_NO_ERROR) {
		    atomic_add(&lock->ben, -1);
		    return stat;
		}
	}
    return APR_SUCCESS;
}

static apr_status_t _unlock(apr_lock_t *lock)
{
    int32 stat;
    
	if (atomic_add(&lock->ben, -1) > 1) {
        if ((stat = release_sem(lock->sem)) < B_NO_ERROR) {
            atomic_add(&lock->ben, 1);
            return stat;
        }
    }
    return APR_SUCCESS;
}

static apr_status_t _destroy_lock(apr_lock_t *lock)
{
    apr_status_t stat;
    if ((stat = _lock_cleanup(lock)) == APR_SUCCESS) {
        apr_pool_cleanup_kill(lock->pool, lock, _lock_cleanup);
        return APR_SUCCESS;
    }
    return stat;
}

apr_status_t apr_lock_create(apr_lock_t **lock, apr_locktype_e type, 
                           apr_lockscope_e scope, const char *fname, 
                           apr_pool_t *pool)
{
    apr_lock_t *new;
    apr_status_t stat;
  
    /* FIXME: Remove when read write locks implemented. */ 
    if (type == APR_READWRITE)
        return APR_ENOTIMPL; 

    new = (apr_lock_t *)apr_pcalloc(pool, sizeof(apr_lock_t));
    if (new == NULL){
        return APR_ENOMEM;
    }
    
    new->pool  = pool;
    new->type  = type;
    new->scope = scope;

    if ((stat = _create_lock(new)) != APR_SUCCESS)
        return stat;

    (*lock) = new;
    return APR_SUCCESS;
}

apr_status_t apr_lock_acquire(apr_lock_t *lock)
{
    apr_status_t stat;

    if (lock->owner == apr_os_thread_current()) {
        lock->owner_ref++;
        return APR_SUCCESS;
    }

    switch (lock->type)
    {
    case APR_MUTEX:
        if ((stat = _lock(lock)) != APR_SUCCESS)
            return stat;
        break;

    case APR_READWRITE:
        return APR_ENOTIMPL;
    }

    lock->owner = apr_os_thread_current();
    lock->owner_ref = 1;

    return APR_SUCCESS;
}

apr_status_t apr_lock_acquire_rw(apr_lock_t *lock, apr_readerwriter_e e)
{
    switch (lock->type)
    {
    case APR_MUTEX:
        return APR_ENOTIMPL;
    case APR_READWRITE:
        switch (e)
        {
        case APR_READER:
            break;
        case APR_WRITER:
            break;
        }
        return APR_ENOTIMPL;
    }

    return APR_SUCCESS;
}

apr_status_t apr_lock_release(apr_lock_t *lock)
{
    apr_status_t stat;

    if (lock->owner_ref > 0 && lock->owner == apr_os_thread_current()) {
        lock->owner_ref--;
        if (lock->owner_ref > 0)
            return APR_SUCCESS;
    }

    switch (lock->type)
    {
    case APR_MUTEX:
        if ((stat = _unlock(lock)) != APR_SUCCESS)
            return stat;
        break;
    case APR_READWRITE:
        return APR_ENOTIMPL;
    }

    lock->owner = -1;
    lock->owner_ref = 0;

    return APR_SUCCESS;
}

apr_status_t apr_lock_destroy(apr_lock_t *lock)
{
    apr_status_t stat; 

    switch (lock->type)
    {
    case APR_MUTEX:
        if ((stat = _destroy_lock(lock)) != APR_SUCCESS)
            return stat;
        break;
    case APR_READWRITE:
        return APR_ENOTIMPL;
    }

    return APR_SUCCESS;
}

apr_status_t apr_lock_child_init(apr_lock_t **lock, const char *fname, 
			                     apr_pool_t *pool)
{
    return APR_SUCCESS;
}

apr_status_t apr_lock_data_get(apr_lock_t *lock, const char *key, void *data)
{
    return apr_pool_userdata_get(data, key, lock->pool);
}

apr_status_t apr_lock_data_set(apr_lock_t *lock, void *data, const char *key,
                            apr_status_t (*cleanup) (void *))
{
    return apr_pool_userdata_set(data, key, cleanup, lock->pool);
}

apr_status_t apr_os_lock_get(apr_os_lock_t *oslock, apr_lock_t *lock)
{
    oslock->sem = lock->sem;
    oslock->ben = lock->ben;
    return APR_SUCCESS;
}

apr_status_t apr_os_lock_put(apr_lock_t **lock, apr_os_lock_t *thelock, 
                             apr_pool_t *pool)
{
    if (pool == NULL) {
        return APR_ENOPOOL;
    }
    if ((*lock) == NULL) {
        (*lock) = (apr_lock_t *)apr_pcalloc(pool, sizeof(apr_lock_t));
        (*lock)->pool = pool;
    }
    (*lock)->sem = thelock->sem;
    (*lock)->ben = thelock->ben;

    return APR_SUCCESS;
}
    
