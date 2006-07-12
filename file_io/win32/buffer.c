/* Copyright 2000-2005 The Apache Software Foundation or its licensors, as
 * applicable.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "apr_arch_file_io.h"
#include "apr_thread_mutex.h"

APR_DECLARE(apr_status_t) apr_file_buffer_set(apr_file_t *file, 
                                              char * buffer,
                                              apr_size_t bufsize)
{
    apr_status_t rv;

    apr_thread_mutex_lock(file->mutex);
 
    if(file->buffered) {
        /* Flush the existing buffer */
        rv = apr_file_flush(file);
        if (rv != APR_SUCCESS) {
            apr_thread_mutex_unlock(file->mutex);
            return rv;
        }
    }
        
    file->buffer = buffer;
    file->bufsize = bufsize;
    file->buffered = 1;
    file->bufpos = 0;
    file->direction = 0;
    file->dataRead = 0;
 
    if (file->bufsize == 0) {
            /* Setting the buffer size to zero is equivalent to turning 
             * buffering off. 
             */
            file->buffered = 0;
    }
    
    apr_thread_mutex_unlock(file->mutex);

    return APR_SUCCESS;
}

APR_DECLARE(apr_size_t) apr_file_buffer_size_get(apr_file_t *file)
{
    return file->bufsize;
}
