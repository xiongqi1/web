#ifndef OSS_H_10061704122015
#define OSS_H_10061704122015
/**
 * @file oss.h
 * @brief Provides public functions and data structures to use operating system
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*******************************************************************************
 * Define public macros
 ******************************************************************************/

#define OSS_TIMEOUT 1

/*******************************************************************************
 * Declare public structures
 ******************************************************************************/

typedef struct oss_sync oss_sync_t;
typedef struct oss_lock oss_lock_t;

/*******************************************************************************
 * Declare public functions
 ******************************************************************************/

/**
 * @brief Creates an instance of the lock
 *
 * @param frame The context of the frame layer
 * @return The instance of the lock on success, or NULL on error
 */
oss_lock_t *oss_lock_create(void);

/**
 * @brief Destorys the instance of the lock
 *
 * @param lock The instance of the lock
 * @return 0 on success, or a negative value on error
 */
void oss_lock_destroy(oss_lock_t *lock);

/**
 * @brief Locks the thread
 *
 * @param lock The instance of the lock
 * @param timeout Indicates how long it should wait until acquiring the lock.
 *        0 indicates it should wait forever until acquiring the lock
 * @return 0 on success, or a negative value on error
 */
int oss_lock(oss_lock_t *lock, unsigned int timeout);

/**
 * @brief Unlocks the thread
 *
 * @param lock The instance of the lock
 * @return 0 on success, or a negative value on error
 */
int oss_unlock(oss_lock_t *lock);

/**
 * @brief Creates an instance of the synchronisation
 *
 * @return The instance of the synchronisation on success, or NULL on error
 */
oss_sync_t *oss_sync_create(void);

/**
 * @brief Destorys the instance of the synchronisation
 *
 * @param sync The instance of the synchronisation
 * @return 0 on success, or a negative value on error
 */
void oss_sync_destroy(oss_sync_t *sync);

/**
 * @brief Synchronises the thread
 *
 * @param sync The instance of the synchronisation
 * @param timeout Indicates how long it should wait until synchronising.
 *        0 indicates it should wait forever until synchronising
 * @return 0 on success, or a negative value on error
 */
int oss_sync_wait(oss_sync_t *sync, unsigned int timeout);

/**
 * @brief Signals the thread waiting for synchronisation.
 *
 * @param sync The instance of the synchronisation
 * @return 0 on success, or a negative value on error
 */
int oss_sync_signal(oss_sync_t *sync);

#endif
