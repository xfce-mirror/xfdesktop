/*
 * xfmenu Copyright (C) 2002 Biju Chacko (botsie@users.sf.net)
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  This program is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.  You
 * should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 
 */



#ifndef __DEBUG_H__
#define __DEBUG_H__

# if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
#  define __DBG_FUNCTION	__func__
# else
#  define __DBG_FUNCTION	((__const char *) 0)
# endif

#if defined(DEBUG) && DEBUG
#include <stdio.h>


# define DBG(fmt, args...) 																\
		({																				\
			fprintf(stderr, "\n%s: line %d: %s(): ", __FILE__ , __LINE__, __DBG_FUNCTION); 	\
			fprintf(stderr , fmt , ## args );											\
		})
#else

# define DBG(fmt, args...)

#endif

#if defined(DEBUG_TRACE) && DEBUG_TRACE

#define TRACE() 																\
		({																				\
			fprintf(stderr, "\n%s: line %d: %s(): ", __FILE__ , __LINE__, __DBG_FUNCTION); 	\
		})
#else
#define TRACE()
#endif

#endif /* __DEBUG_H__ */
