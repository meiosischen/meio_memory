/**************************************************************
#  
#  Licensed to the Apache Software Foundation (ASF) under one
#  or more contributor license agreements.  See the NOTICE file
#  distributed with this work for additional information
#  regarding copyright ownership.  The ASF licenses this file
#  to you under the Apache License, Version 2.0 (the
#  "License"); you may not use this file except in compliance
#  with the License.  You may obtain a copy of the License at
#  
#    http://www.apache.org/licenses/LICENSE-2.0
#  
#  Unless required by applicable law or agreed to in writing,
#  software distributed under the License is distributed on an
#  "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#  KIND, either express or implied.  See the License for the
#  specific language governing permissions and limitations
#  under the License.
#
# @Author:       meiosis.chen@gmail.com
# @revision:     0.1
# @date:         2012-08-30
#**************************************************************/

/* @brief define constants */

#ifndef _MEIO_DEFINE_H_
#define _MEIO_DEFINE_H_

#ifndef NULL
#define NULL 0
#endif

#ifndef mint32
#define mint32 int
#endif

#ifndef muint32
#define muint32 unsigned int
#endif

#ifndef mbyte
#define mbyte unsigned char
#endif

#ifndef mchar
#define mchar char
#endif

#define mi_lock_t int

enum ENUM_RWLOCK;
typedef enum ENUM_RWLOCK ENUM_RWLOCK;

enum ENUM_RWLOCK
{
	RWLOCK_READ = 0,
	RWLOCK_WRITE				
};

/* @note the default size of a memory node to alloc
   this is the minimal size to alloc from os
   (not from meio_memory lib)
*/
#define DEFAULT_MI_MEM_NODE_SIZE		(512 * 8)

#ifndef RET_IF_FALSE
#define RET_IF_FALSE(e) \
	do { \
		if (!(e)) return ;	\
	} while(0)
#endif


#ifndef RETV_IF_FALSE
#define RETV_IF_FALSE(e,ret) \
	do { \
		if (!(e)) return (ret);	\
	} while(0)
#endif

#ifndef RET_IF_NULL
#define RET_IF_NULL RET_IF_FALSE
#endif

#ifndef RETV_IF_NULL
#define RETV_IF_NULL RETV_IF_FALSE
#endif

/* set to 1 if you want to test in detail */
#define UNIT_TEST 0

/* set to 1 if you want to print the pool's layout*/
#define USE_LOG 1

#endif

