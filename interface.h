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
#**************************************************************/

#ifndef _MEIO_INTERFACE_H_
#define _MEIO_INTERFACE_H_
#include "./define.h"

struct mi_mem_node_t;
typedef struct mi_mem_node_t	mi_mem_node_t;

struct mi_mem_node_list_t;
typedef struct mi_mem_node_list_t mi_mem_node_list_t;

struct mi_pool_t;
typedef struct mi_pool_t		mi_pool_t;

struct mi_pool_list_t;
typedef struct mi_pool_list_t	mi_pool_list_t;

struct mi_lock_op;
typedef struct mi_lock_op		mi_lock_op;

struct mi_mem_op;
typedef struct mi_mem_op		mi_mem_op;

struct mi_pool_op;
typedef struct mi_pool_op		mi_pool_op;

struct mi_mem_node_op;
typedef struct mi_mem_node_op	mi_mem_node_op;

struct mi_mem_node_t
{
	mi_pool_t			*pool;
	mi_mem_node_t		*parent;
	mi_mem_node_t		*child;
	muint32				free_index;
	mbyte				*data;
	muint32				curr_size;
	muint32				orig_size;
    void                (*func)(void*,muint32);
};

struct mi_mem_node_list_t
{
	mi_mem_node_list_t	*next;
	mi_mem_node_t		*data;
};

struct mi_pool_t
{
	mi_pool_t			*parent;
	mi_pool_list_t		*child;
	mi_mem_node_list_t	*data;
	mi_lock_t			*lock;
	mi_lock_op			*lock_op;
};

struct mi_pool_list_t
{
	mi_pool_list_t		*next;
	mi_pool_t			*data;
};

struct mi_lock_op
{
	mi_lock_t*			( *create )(void);
	void				( *delete )(mi_lock_t *lock);
	void				( *lock )(mi_lock_t *lock, ENUM_RWLOCK mode);
	void				( *unlock )(mi_lock_t *lock);
};

struct mi_pool_op
{
	mi_pool_t*			( *create )(muint32 node_size, muint32 node_count, void *ctx);
	mi_pool_t*			( *alloc )(mi_pool_t *parent, muint32 node_size, muint32 node_count);
	void				( *free )(mi_pool_t *pool);
};

struct mi_mem_op
{
	void*				( *alloc )(mi_pool_t *pool, muint32 size);
	void				( *free )(void *data);
    void*               ( *alloc_ex )(mi_pool_t *pool, muint32 size, void ( *func )(void*, muint32));
};


mi_pool_op*		 create_mi_pool_op_default(void);
mi_mem_op*		 create_mi_mem_op_default(void);
mi_lock_op*		 create_mi_lock_op_default(void);

void view_simple_pool (mi_pool_t *root);

#endif
