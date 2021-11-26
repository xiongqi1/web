#ifndef APP_API_H_16130810122015
#define APP_API_H_16130810122015
/**
 * @file app_api.h
 * @brief Declares common types used through all applications
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

/**
 * @brief Gets the list of source ports to be listened.
 *
 * @param sources The list of source ports
 * @param max_source_num The maximum number of source port to be stored in the sources
 * @return Returns the number of source ports
 */
int get_source_ports(unsigned short *sources, int max_source_num);

/**
 * @brief Gets the target port that is associated with the given source port.
 *
 * @param source_port The number of the source port
 * @return Returns the target port
 */
unsigned short get_target_port(unsigned short source_port);

/**
 * @brief Gets the optimal baud rate.
 *
 * @return Returns the optimal baud rate
 */
unsigned int get_optimal_baud_rate(void);

#endif
