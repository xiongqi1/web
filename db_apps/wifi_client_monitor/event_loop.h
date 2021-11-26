/*
 * event_loop.h
 * Header file for event loop which handles timers and reader file descriptors
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 */

#ifndef EVENT_LOOP_H_
#define EVENT_LOOP_H_

/*
 * handler function invoked when the file descriptor becomes "ready" for reading
 * @fd: file descriptor being monitored
 * @data: data passed to the handler
 */
typedef void (*event_loop_fd_handler)(int fd, void *data);

/*
 * handler function invoked when the timer expires
 * @data: data passed to the handler
 */
typedef void (*event_loop_timer_handler)(void *data);

/*
 * Initialise event loop
 *
 * Returns 0 if success, negative error code otherwise
 */
int event_loop_init(void);
/*
 * free event loop.
 * Precondition: event loop must not be running.
 */
void event_loop_free(void);
/*
 * add a file descriptor to be monitored by event loop, waiting until the file descriptor
 * becomes "ready" for reading
 * @fd: file descriptor to be monitored
 * @handler: function is invoked when the file descriptor is ready for reading
 * @data: data passed to the handler
 *
 * returns 0 if success, negative error code otherwise
 */
int event_loop_add_fd(int fd, event_loop_fd_handler handler, void *data);
/*
 * delete a file descriptor from being monitored by event loop for reading
 * @fd: file descriptor to be deleted
 *
 * returns 0 if success, negative error code otherwise
 */
int event_loop_del_fd(int fd);
/*
 * add a timer to event loop
 * when the timer expires, it is removed from the list
 * and its handler is invoked
 *
 * @secs: seconds in future when the timer expires
 * @usecs: micro seconds in future when the timer expires
 * @handler: function to invoke when the timer expires
 * @data: data to be passed to the handler
 *
 * returns 0 if success, negative error code otherwise
 */
int event_loop_add_timer(unsigned int secs, unsigned int usecs, event_loop_timer_handler handler, void *data);
/*
 * delete a timer from event loop
 * @handler: function would be invoked when the timer expired
 * @data: data would be passed to the handler
 *
 * returns 0 if success, negative error code otherwise
 */
int event_loop_del_timer(event_loop_timer_handler handler, void *data);
/*
 * Wait for any monitored file descriptors to be ready to process or any timers expire.
 * This will block until any of those events available.
 * It can be terminated by SIGINT, SIGQUIT, or SIGTERM.
 *
 * Returns 0 if success, negative error code otherwise
 */
int event_loop_run(void);
#endif /* EVENT_LOOP_H_ */
