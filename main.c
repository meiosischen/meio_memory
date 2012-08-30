/************************************************
# @Author:       meiosis.chen@gmail.com
# @revision:     0.1
# @date:         2012-08-30
************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdlib.h>
#include "./interface.h"

#define TRY_COUNT	1024
#define ARRAY_COUNT	256

#define BARRIER ("\n\n********************\n\n")

#define PRINT_STRUCT 0

unsigned long get_tick_count(void)
{
	unsigned long t;
	struct timeval curr_time;
	gettimeofday(&curr_time, NULL);
	t = curr_time.tv_sec * 1000 + curr_time.tv_usec/1000;
	return t;
}

struct my_struct;
typedef struct my_struct my_struct;

struct my_struct
{
	const char *name;
	int age;
	char ghost[0];
};

static void my_func(void *data, muint32 size)
{
#if PRINT_STRUCT    
    my_struct *st = (my_struct*)data;
    int count = size / sizeof(my_struct);
    int i;

    for (i=0; i<count; i++) {
        printf("my_struct name: %s, age: %d\n", 
            st[i].name,
            st[i].age);
    }
#endif     
}

void test_meio_memory(void)
{
	mi_pool_op *pool_op;
	mi_pool_t *pool;
	mi_mem_op *mem_op;
	my_struct *array;
	mi_pool_t *sub_pool;
	const int count = ARRAY_COUNT;
	char name[count][256];
	unsigned long tick;
	int i;
	int j;
	
	printf("we will start meio_memory!\n");
	tick = get_tick_count();
	pool_op = create_mi_pool_op_default();
	pool = pool_op->create(sizeof(my_struct), count, NULL);
	mem_op = create_mi_mem_op_default();
	
	for (j=0; j<TRY_COUNT; j++) {
		sub_pool = pool_op->alloc(pool, sizeof(my_struct), count);
		array = 
          mem_op->alloc_ex(sub_pool, (sizeof(my_struct))*count, my_func);

		for (i=0; i<count; i++) {
			memset(name[i], 0, sizeof(name[0]));
			sprintf(name[i], "helllo my name %d.%d", j, i);
			array[i].name = name[i];
			array[i].age = i+30;
		}

#if PRINT_STRUCT
		for (i=0; i<count; i++) {
            printf("name: %s  \t age: %d\n",
                array[i].name, 
                array[i].age); 
        }
        printf(BARRIER);
#endif                
        view_simple_pool(sub_pool);
		pool_op->free(sub_pool);
	}


	pool_op->free(pool);

	printf("meio_memory finished %lu msec elapsed\n", 
           get_tick_count() - tick);
}

void test_clib(void)
{
	my_struct *array;
	const int count = TRY_COUNT;
	char name[count][256];
	unsigned long tick;
	int i;
	int j;
	
	printf("we will start clib!\n");
	tick = get_tick_count();
	
	for (j=0; j<ARRAY_COUNT; j++) {
		array = malloc((sizeof(my_struct)+j)*count);
		for (i=0; i<count; i++) {
			memset(name[i], 0, sizeof(name[0]));
			sprintf(name[i], "hello my name %d.%d", j, i);
			array[i].name = name[i];
			array[i].age = i+30;
		}

#if PRINT_STRUCT
		for (i=0; i<count; i++) {
			printf("name: %s  \t age: %d\n", array[i].name, array[i].age); 
		}
#endif
		free(array);
	}
	printf("clib finished %lu msec elapsed\n", get_tick_count()-tick);
}

int main(int argc, char **argv)
{
	test_clib();
	test_meio_memory();
	return 0;
}
