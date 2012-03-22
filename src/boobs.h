/*** boobs.h -- byte-order oberations
 *
 * Copyright (C) 2012 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of uterus and dateutils.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/

#if !defined INCLUDED_byteorder_h_
#define INCLUDED_byteorder_h_

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
/* *bsd except for openbsd */
#if defined HAVE_ENDIAN_H
# include <endian.h>
#elif defined HAVE_SYS_ENDIAN_H
# include <sys/endian.h>
#elif defined HAVE_BYTEORDER_H
# include <byteorder.h>
#elif defined HAVE_ARPA_INET_H
# include <arpa/inet.h>
#elif defined NETINET_IN_H
# include <netinet/in.h>
#else
/* FUCK!!! */
#endif	/* HAVE_ENDIAN_H */

/* and even now we may be out of luck */
#if !defined be32toh
# if defined betoh32
#  define be32toh	betoh32
# elif defined ntohl
#  define be32toh	ntohl
# elif defined WORDS_BIGENDIAN || __BYTE_ORDER == __BIG_ENDIAN
#  define be32toh(x)	(x)
# endif
#endif	/* !be32toh */

#if !defined le32toh
# if defined letoh32
#  define le32toh	letoh32
# elif !defined WORDS_BIGENDIAN || __BYTE_ORDER == __LITTLE_ENDIAN
#  define le32toh(x)	(x)
# endif	 /* letoh32 */
#endif	/* !le32toh */

#if !defined htobe32
# if defined htonl
#  define htobe32	htonl
# elif defined WORDS_BIGENDIAN || __BYTE_ORDER == __BIG_ENDIAN
#  define htobe32(x)	(x)
# endif
#endif	/* !be32toh */

#if !defined htole32
# if !defined WORDS_BIGENDIAN || __BYTE_ORDER == __LITTLE_ENDIAN
#  define htole32(x)	(x)
# endif
#endif	/* !le32toh */

/* we could technically include byteswap.h and to the swap ourselves
 * in the missing cases.  Instead we'll just leave it as is and wait
 * for bug reports. */

#endif	/* INCLUDED_byteorder_h_ */
