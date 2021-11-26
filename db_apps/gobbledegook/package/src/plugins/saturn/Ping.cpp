/*
 *
 * Copyright Notice:
 * Copyright (C) 2020 Casa Systems.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Casa Systems.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CASA SYSTEMS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CASA
 * SYSTEMS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <iostream>

#define	DEFDATALEN	(64-ICMP_MINLEN)
#define	MAXIPLEN	60
#define	MAXICMPLEN	76
#define	MAXPACKET	(65536 - 60 - ICMP_MINLEN)

const unsigned one = 1U;

// *(reinterpret_cast<const char*>(&one)) --> value on the lowest address byte.
inline bool is_little_endian()
{
    return *(reinterpret_cast<const char*>(&one)) == 1;;
}

// Compute ICMP checksum.
//
// addr: address of ICMP header
// len: length of ICMP header
uint16_t in_cksum(uint16_t *addr, unsigned len)
{
    /*
     * Algorithm is simple, using a 32 bit accumulator (sum), add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */

    uint32_t sum = 0;
    while (len > 1)  {
        sum += *addr++;
        len -= 2;
    }

    if (len == 1) {
        if (is_little_endian()) {
            sum += *(uint8_t*)addr;
        } else {
            sum += *(uint8_t*)addr << 8;
        }
    }

    /* Add back carry outs from top 16 bits to low 16 bits */
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (uint16_t)~sum;
}

// Send ICMP packet to the target
//
// target: hostname or IP address of target server.
//
// Return
// val > 0: Success. Elapsed time in microseconds.
// val = 0: Failure. No data within one seconds.
// val < 0: Failure. Error case
int ping(std::string target)
{
    int s = -1;
    int cc;
    int packlen;
    int datalen = DEFDATALEN;
    int fromlen;
    int retval;
    int end_t = -1;
    u_char *packet = NULL;
    u_char outpack[MAXPACKET];
    u_int16_t l_icmp_id = getpid();
    u_int16_t l_icmp_seq = 12345;
    bool cont = true;
    struct hostent *hp;
    struct sockaddr_in to;
    struct sockaddr_in from;
    struct icmp *icp;
    fd_set rfds;
    struct timeval tv;
    struct timeval start;
    struct timeval end;

    to.sin_family = AF_INET;
    to.sin_addr.s_addr = inet_addr(target.c_str());

    if (to.sin_addr.s_addr == INADDR_NONE) { // hostname instead of ip address.
        hp = gethostbyname(target.c_str());
        if (!hp)
        {
            std::cerr << "unknown host "<< target << std::endl;
            goto exit;
        }
        to.sin_family = hp->h_addrtype;
        bcopy(hp->h_addr, (caddr_t)&to.sin_addr, hp->h_length);
    }

    packlen = datalen + MAXIPLEN + MAXICMPLEN;
    if ( (packet = (u_char *)malloc((u_int)packlen)) == NULL)
    {
        std::cerr << "malloc error" << std::endl;
        goto exit;
    }

    if ( (s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)
    {
        goto exit;
    }

    icp = (struct icmp *)outpack;
    icp->icmp_type = ICMP_ECHO;
    icp->icmp_code = 0;
    icp->icmp_cksum = 0;
    icp->icmp_seq = l_icmp_seq;
    icp->icmp_id = l_icmp_id;

    cc = datalen + ICMP_MINLEN;
    icp->icmp_cksum = in_cksum((uint16_t *)icp, cc);

    gettimeofday(&start, NULL);

    if (sendto(s, (char *)outpack, cc, 0, (struct sockaddr*)&to, (socklen_t)sizeof(struct sockaddr_in)) < 0) {
            perror("sendto error");
            goto exit;
    }

    FD_ZERO(&rfds);
    FD_SET(s, &rfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    while(cont)
    {
        retval = select(s+1, &rfds, NULL, NULL, &tv);
        if (retval == -1)
        {
            perror("select()");
            goto exit;
        }
        else if (retval)
        {
            fromlen = sizeof(sockaddr_in);
            if (recvfrom(s, (char *)packet, packlen, 0,(struct sockaddr *)&from, (socklen_t*)&fromlen) < 0)
            {
                perror("recvfrom error");
                goto exit;
            }

            struct iphdr *iphdr = (struct iphdr *) packet;

            icp = (struct icmp *) (packet + (iphdr->ihl << 2));   /* skip ip hdr */
            if (icp->icmp_type == ICMP_ECHOREPLY)
            {
                if (icp->icmp_seq != l_icmp_seq)
                {
                    continue;
                }
                if (icp->icmp_id != l_icmp_id)
                {
                    continue;
                }
                cont = false;
            }
            else
            {
                continue;
            }

            gettimeofday(&end, NULL);
            end_t = 1000000*(end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);

            if(end_t < 1) {
                end_t = 1;
            }
            goto exit;
        } else {
            end_t = 0;
            goto exit;
        }
    }
    end_t = 0;

exit:
    if (s >= 0) {
        close(s);
    }
    if (packet != NULL) {
        free(packet);
    }
    return end_t;
}

