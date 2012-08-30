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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "./interface.h"

static mi_pool_t*		mi_pool_create		(muint32 node_size, muint32 node_count, void *ctx);
static mi_pool_t*		mi_pool_alloc		(mi_pool_t *parent, muint32 node_size, muint32 node_count);
static void				mi_pool_free		(mi_pool_t *pool);

static void*			mi_mem_alloc		(mi_pool_t *pool, muint32 size);
static void				mi_mem_free			(void *data);
static void*            mi_mem_alloc_ex     (mi_pool_t *pool, muint32 size, void ( *func )(void*, muint32));

static mi_lock_t*		mi_lock_create		(void);
static void				mi_lock_delete		(mi_lock_t *lock);
static void				mi_lock_lock		(mi_lock_t *lock, ENUM_RWLOCK mode);
static void				mi_lock_unlock		(mi_lock_t *lock);

mi_pool_op*				create_mi_pool_op_default(void);
mi_mem_op*				create_mi_mem_op_default(void);
mi_lock_op*				create_mi_lock_op_default(void);

struct _mi_mem_node_head_t;
typedef struct _mi_mem_node_head_t _mi_mem_node_head_t; 

struct _mi_mem_node_head_t
{
	mi_mem_node_t *mnode;
	mbyte data[0];
};

#undef MALLOC
#define MALLOC(x) malloc((x))

#undef MALLOC_T
#define MALLOC_T(t,c) (t*)MALLOC(sizeof(t)*(c))

#undef MFREE
#define MFREE(x) free((x))

#undef MALLOC_CHECK
#define MALLOC_CHECK(o,ret) \
	do{ \
		if (!o) return ret; \
	} while(0)

#undef ZEROM
#define ZEROM(o,size) \
	do { \
		memset((o), 0, (size));\
	} while(0)

#undef FOR_EACH_LIST_DATA
#define FOR_EACH_LIST_DATA(l,d,func)\
	do { \
		if(l->d) func(l->d); \
		l = l->next; \
	} while(0)

#ifndef MI_LOG
#if USE_LOG
#define MI_LOG printf
#else
#define MI_LOG(x,...)
#endif
#endif

#undef MI_ERR
#define MI_ERR(x,info) \
	do { \
		printf(info); \
		printf(": "); \
		printf(x); \
		printf("\n"); \
	} while(0)

#ifndef TRY_LOCK
#define TRY_LOCK(l,op,mode) \
	do {\
		if ((l) && (op)) op->lock((l), (mode)); \
	} while(0)
#endif

#ifndef TRY_UNLOCK
#define TRY_UNLOCK(lock,op) \
	do {\
		if ((lock) && (op)) op->unlock((lock)); \
	} while(0)
#endif

#ifndef TRACE_FUNC
#define TRACE_FUNC \
	do { \
		if (UNIT_TEST) \
		MI_LOG("\t\tcall into %s, %d, %s\n", __FUNCTION__, __LINE__, __FILE__); \
	} while(0)
#endif

#undef ADJUST_DEFAULT_NODE_SIZE
#define ADJUST_DEFAULT_NODE_SIZE(x) \
	do { \
		if ((x) < DEFAULT_MI_MEM_NODE_SIZE) \
			(x) = DEFAULT_MI_MEM_NODE_SIZE; \
		else if ((x)> DEFAULT_MI_MEM_NODE_SIZE) \
			(x) += DEFAULT_MI_MEM_NODE_SIZE - ((x)% DEFAULT_MI_MEM_NODE_SIZE); \
	} while(0)

inline static void TEST_PRINT (const char* x, ...)
{
#if UNIT_TEST
	va_list arg;
	va_start(arg, x);
	vprintf(x, arg); 
	va_end(arg);
	printf("\n");
#endif
}

inline static void _free_mem_node_list(mi_mem_node_list_t *list)
{
	TRACE_FUNC;
	RET_IF_NULL(list);
	do {
		MFREE(list->data->data);
		MFREE(list->data);
		list = list->next;
	} while(list);
}

/* cut a new memory node from a exist memory node */
inline static mi_mem_node_t* _mem_node_cut (mi_mem_node_t *node)
{
	mi_mem_node_t *new_node = NULL;

	TRACE_FUNC;
	RETV_IF_NULL(node, NULL);
	if(node->free_index == 0 || node->free_index == node->curr_size) 
		  return NULL;

	new_node = MALLOC_T(mi_mem_node_t, 1);
	MALLOC_CHECK(new_node, NULL);
	ZEROM(new_node, sizeof(mi_mem_node_t));

	if (node->child) {
		node->child->parent = new_node;
		new_node->child = node->child;
	}

	node->child = new_node;
	new_node->parent = node;	
	new_node->pool = node->pool;
	new_node->data = node->data + node->free_index;
	new_node->curr_size = new_node->orig_size = node->curr_size - node->free_index;
	node->curr_size = node->free_index;

	return new_node;
}

inline static mi_mem_node_t* _fetch_free_mem_node(mi_pool_t *pool, muint32 size)
{
	mi_mem_node_list_t *list = NULL;	
	mi_mem_node_t *node = NULL;
	mi_mem_node_t *new_node = NULL;

	TRACE_FUNC;
	TEST_PRINT("%s pool: 0x%08X, size: %lu", __FUNCTION__, pool, size);
	RETV_IF_FALSE(pool && size > 0, NULL);
	
	list = pool->data;
	while (list) {
		node = list->data;		
		if ( (node->curr_size - node->free_index) >= size ) {
			if (node->free_index == 0) {
				node->free_index = size;
				return node;
			}	
			else {
				new_node = _mem_node_cut(node);	
				new_node->free_index = size;
				return new_node;
			}
		}
		list = list->next;
	}

    /* no more useable memory node */
	return NULL;
}

inline static mi_mem_node_list_t* _create_mem_node_list(mi_pool_t *pool, muint32 size)
{
	mi_mem_node_t *mnode = NULL;
	mi_mem_node_list_t *mnlist = NULL;
	muint32 alloc_size = size;

	TRACE_FUNC;
	RETV_IF_FALSE(pool && alloc_size > 0, NULL);

	mnode = MALLOC_T(mi_mem_node_t, 1);
	MALLOC_CHECK(mnode, NULL);	
	ZEROM(mnode, sizeof(mi_mem_node_t));
	mnode->parent = NULL;

	ADJUST_DEFAULT_NODE_SIZE(alloc_size);

	mnode->data = (mbyte*)MALLOC(alloc_size);
	MALLOC_CHECK(mnode->data, NULL);	
	TEST_PRINT("__alloc__ %d  bytes", alloc_size);
	ZEROM(mnode->data, alloc_size);
	mnode->orig_size = mnode->curr_size = alloc_size;
	mnode->pool = pool;

	mnlist = MALLOC_T(mi_mem_node_list_t, 1);
	MALLOC_CHECK(mnlist, NULL);	
	mnlist->next = NULL;
	mnlist->data = mnode;
	
	return mnlist;
}

static mi_pool_t* mi_pool_create (muint32 node_size, muint32 node_count, void *ctx)
{
	mi_pool_t *pool = NULL;
	mi_mem_node_list_t *mnlist = NULL;
	muint32 alloc_size = node_size * node_count;

	TRACE_FUNC;
	RETV_IF_FALSE(alloc_size > 0, NULL);	

	pool = MALLOC_T(mi_pool_t, 1);
	MALLOC_CHECK(pool, NULL);	
	ZEROM(pool, sizeof(mi_pool_t));
	mnlist = _create_mem_node_list(pool, alloc_size);
	pool->data = mnlist;
	mnlist->data->pool = pool;

	return pool;
}


static mi_pool_t* mi_pool_alloc	(mi_pool_t *parent, muint32 node_size, muint32 node_count)
{
	muint32 alloc_size = node_size * node_count;
	mi_mem_node_list_t *list = NULL;
	mi_mem_node_list_t *prev = NULL;
	mi_mem_node_t *new_node = NULL;
	mi_pool_list_t *new_pool_list = NULL;
	mi_pool_t *new_pool = NULL;
	mi_lock_op *lock_op = NULL;

	TRACE_FUNC;
	ADJUST_DEFAULT_NODE_SIZE(alloc_size);
	TEST_PRINT("%s alloc_size: %lu", __FUNCTION__, alloc_size);
	RETV_IF_FALSE(parent && alloc_size > 0, NULL);	
	RETV_IF_FALSE(parent->data || parent->data->data, NULL);
	list = parent->data;


	/* to find an avaliable node on parent */
	TRY_LOCK(parent->lock, parent->lock_op, RWLOCK_WRITE);
	while (list) {
		if (list->data && 
			list->data->free_index == 0 &&
			list->data->curr_size == list->data->orig_size &&
			list->data->curr_size >= alloc_size &&
			!list->data->parent &&
			!list->data->child ) {
			
			new_node = list->data;
			break;
		}
		prev = list;
		list = list->next;
	}

	new_pool_list = MALLOC_T(mi_pool_list_t, 1);
	if (!new_node) {
		/* need alloc a new node */	
		MI_LOG("mi_pool_alloc: need alloc a new node\n");
		new_pool = mi_pool_create(node_size, node_count, NULL); 
		goto done;
	}

	/* found an avaliable node */			
	TEST_PRINT("mi_pool_alloc: found an avaliable node at 0x%08X, list at 0x%08X", new_node, list);
	if (!prev) {
		/* insert to head */		
		TEST_PRINT("mi_pool_alloc: insert to head 0x%08X", list);
		parent->data = list->next;
	}
	else {
		TEST_PRINT("mi_pool_alloc: adjust list prev 0x%08X, list 0x%08X", prev, list);
		prev->next = list->next;	
	}
	list->next = NULL;
		
	new_pool = MALLOC_T(mi_pool_t, 1);
	MALLOC_CHECK(new_pool, NULL);
	ZEROM(new_pool, sizeof(mi_pool_t));
	new_pool->data = list;

done:
	new_pool_list->data = new_pool;
	new_pool->parent = parent;
	new_pool_list->next = parent->child;
	parent->child = new_pool_list;
	lock_op = create_mi_lock_op_default();
	new_pool->lock = lock_op->create();
	new_pool->lock_op = lock_op;
	TRY_UNLOCK(parent->lock, parent->lock_op);
	return new_pool;
}

static void mi_pool_free (mi_pool_t *pool)
{
	mi_lock_op *lop = NULL;
	mi_pool_list_t *pool_list = NULL;
	mi_pool_list_t *prev_pool_list = NULL;
	mi_mem_node_list_t *mnlist = NULL;

	TRACE_FUNC;
	RET_IF_NULL(pool);	

	if (!pool->parent){
		/* root pool */	
		TEST_PRINT("mi_pool_free: root pool at 0x%08X", pool);
		if (pool->data && pool->data->data)
		  _free_mem_node_list(pool->data);

		MFREE(pool);
		return;
	}
	else {
		/* child pool, merge into the parent */
		if (pool->data && pool->data->data){
			TEST_PRINT("mi_pool_free: child pool at 0x%08X of parent: 0x%08X", pool, pool->parent);
			
			/* reset every mem_node from mem_node_list */
			mnlist = pool->data;
			while(mnlist) {
				if (mnlist->data) {
					if (mnlist->data->parent)
						TEST_PRINT("error at %s %d %s", __FUNCTION__, __LINE__, __FILE__);

					mnlist->data->child = NULL;
					mnlist->data->free_index = 0;
                    mnlist->data->func = NULL;
					mnlist->data->curr_size = mnlist->data->orig_size;
					mnlist->data->pool = pool->parent;
				}
				mnlist = mnlist->next;	
			}

			TRY_LOCK(pool->parent->lock, pool->parent->lock_op, RWLOCK_WRITE);
			pool->data->next = pool->parent->data;
			pool->parent->data = pool->data;

			/* remove the mi_pool_list_t node */
			pool_list = pool->parent->child;
			while (pool_list) {
				if (pool_list->data == pool) {
					if (!prev_pool_list) {
						pool->parent->child = pool_list->next;	
						MFREE(pool_list);
						break;
					}
					else {
						prev_pool_list->next = pool_list->next;	
						MFREE(pool_list);
						break;	
					}

				}
				prev_pool_list = pool_list;
				pool_list = pool_list->next;
			}

			/* remove the lock */
			lop = create_mi_lock_op_default();
			lop->delete(pool->lock);
			TRY_UNLOCK(pool->parent->lock, pool->parent->lock_op);
			MFREE(pool);
			return;
		}
		else {
			/* we got some error */	
			MI_ERR("pool free error", "err");
			MFREE(pool);
		}
	}
}

static void* mi_mem_alloc (mi_pool_t *pool, muint32 size)
{
	return mi_mem_alloc_ex(pool, size, NULL);
}

static void* mi_mem_alloc_ex (mi_pool_t *pool, 
                              muint32 size, 
                              void ( *func )(void*, muint32))
{
	mi_mem_node_t *new_node = NULL;
	mi_mem_node_list_t *new_node_list = NULL;	
	_mi_mem_node_head_t *head = NULL;

	TRACE_FUNC;
	RETV_IF_FALSE(pool && size > 0, NULL);	

	TEST_PRINT("user __alloc %d bytes", size);
	TRY_LOCK(pool->lock, pool->lock_op, RWLOCK_WRITE);
	new_node = _fetch_free_mem_node(pool, size + sizeof(_mi_mem_node_head_t));
	if (new_node) 
		goto done;				
		
	/* we need alloc a new mem node list */
	new_node_list = _create_mem_node_list(pool, size + sizeof(_mi_mem_node_head_t));
	new_node = new_node_list->data;
	new_node->free_index = size;
	new_node_list->next = pool->data;
	pool->data = new_node_list;

done:
	TRY_UNLOCK(pool->lock, pool->lock_op);
	MI_LOG("new mem_node at 0x%08X\n", (muint32)new_node);
	
    if (func) new_node->func = func;

	head = (_mi_mem_node_head_t*)(new_node->data);
	head->mnode = new_node;
	return head->data;
}

static void mi_mem_free (void *data)
{
	mi_mem_node_t *node = NULL;
	mi_pool_t *pool = NULL;
	_mi_mem_node_head_t *head = NULL;

	TRACE_FUNC;
	RET_IF_NULL(data);
	/* get the mem node */
	/* TODO */
	head = (_mi_mem_node_head_t*)((mbyte*)data-sizeof(_mi_mem_node_head_t));
	node = head->mnode;
	MI_LOG("get mem_node at 0x%08X\n", (muint32)node);
	if (!node || !node->pool) {
		/* we should not be here! */
		MI_LOG("mi_mem_free: error at 0x%08X\n", (unsigned int)data);
		MI_ERR("mi_mem_free ", "unexpected error");
		return;;
	}

	pool = node->pool; 
	TRY_LOCK(pool->lock, pool->lock_op, RWLOCK_WRITE); 
	if (!node->parent) {
		node->free_index = 0;
        node->func = NULL;
		goto done;
	}
	else {
		/* merge into the parent node */		
		node->parent->curr_size += node->curr_size;
		node->parent->child = node->child;
        MFREE(head);
		MFREE(node);
        node = NULL;
	}

done:
	TRY_UNLOCK(pool->lock, pool->lock_op);
}

static mi_lock_t* mi_lock_create (void)
{
	TRACE_FUNC;
	TEST_PRINT("mi_lock_create: not implemented");
	return NULL;	
}

static void	 mi_lock_delete (mi_lock_t *lock) 
{
	TRACE_FUNC;
	TEST_PRINT("mi_lock_delete: not implemented");
}

static void mi_lock_lock (mi_lock_t *lock, ENUM_RWLOCK mode)
{
	TRACE_FUNC;
	TEST_PRINT("mi_lock_lock: not implemented");
}

static void mi_lock_unlock (mi_lock_t *lock)
{
	TRACE_FUNC;
	TEST_PRINT("mi_lock_unlock: not implemented");
}

mi_pool_op* create_mi_pool_op_default(void)
{
	static mi_pool_op *plop = NULL;
	static mi_pool_op splop;

	TRACE_FUNC;
	if (plop) return plop;

	splop.create = mi_pool_create;
	splop.alloc = mi_pool_alloc;
	splop.free = mi_pool_free;
	plop = &splop;

	return plop;
}

mi_mem_op* create_mi_mem_op_default(void)
{
	static mi_mem_op *mop = NULL;
	static mi_mem_op smop;

	TRACE_FUNC;
	if (mop) return mop;

	smop.alloc = mi_mem_alloc;
	smop.free = mi_mem_free;
    smop.alloc_ex = mi_mem_alloc_ex;
	mop = &smop;

	return mop;
}


mi_lock_op* create_mi_lock_op_default(void)
{
	static mi_lock_op *lop = NULL;
	static mi_lock_op slop;
	
	TRACE_FUNC;
	if (lop) return lop;

	slop.create = mi_lock_create;
	slop.delete = mi_lock_delete;
	slop.lock = mi_lock_lock;
	slop.unlock = mi_lock_unlock;
	lop = &slop;

	return lop;
}

static void _show_mem_node (mi_mem_node_t *node)
{
	mi_mem_node_t *anode = node;
    _mi_mem_node_head_t *head;

	RET_IF_NULL(node);
	
	while (anode) {
        if (anode->free_index > 0) {
		    MI_LOG("mem_node: fi: %d, cs: %d, os: %d, dt: 0x%08X, child :0x%08X\n",
			    	anode->free_index,
					anode->curr_size,
					anode->orig_size,
					(muint32)anode->data,
					(muint32)anode->child);

	        head = (_mi_mem_node_head_t*)anode->data;
            if (head && anode->func) anode->func(head->data, anode->free_index+1);
        }
		anode = anode->child;
	}
}

void _show_mem_node_list (mi_mem_node_list_t *list)
{
	mi_mem_node_list_t *alist = list;

	RET_IF_NULL(list);	

	while (alist) {
		MI_LOG("mem_node_list: data: 0x%08X, next: 0x%08X\n", (muint32)alist->data, (muint32)alist->next);
		_show_mem_node(alist->data);
		alist = alist->next;	
	}
}

void view_simple_pool (mi_pool_t *root)
{
	mi_pool_list_t *child_list;
	mi_mem_node_list_t *node_list;

	TRACE_FUNC;
	RET_IF_NULL(root);

	MI_LOG("pool: pt: 0x%08X, cd: 0x%08X, dt: 0x%08X, lk: 0x%08X, lo: 0x%08X\n",
				(muint32)root->parent, (muint32)root->child, (muint32)root->data, (muint32)root->lock, (muint32)root->lock_op);

	node_list = root->data;
	_show_mem_node_list(node_list);

	child_list = root->child;
	while (child_list) {
		view_simple_pool(child_list->data);	
		child_list = child_list->next;
	}
}
