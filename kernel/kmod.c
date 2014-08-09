/*-
 * Copyright (c) 2012, 2013 Gabor Pali
 * Copyright (c) 2014, andy.ray@ujamjar.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#define _KMOD_

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <sys/errno.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/sched.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#ifdef MEM_LEAK
#include <sys/queue.h>
#endif

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/uma.h>
#include <vm/uma_int.h>

#include "caml/mlvalues.h"
#include "caml/callback.h"
#include "caml/memory.h"
#include "caml/gc_ctrl.h"
#include "caml/major_gc.h"
#include "caml/minor_gc.h"
#include "caml/io.h"
#include "caml/roots.h"
#include "caml/globroots.h"
#include "caml/callback.h"
#include "caml/startup.h"
#include "caml/custom.h"
#include "caml/finalise.h"

CAMLprim value caml_block_kernel(value v_timeout);

int atoi(const char *str) {
  return (int) strtol(str, (char**) NULL, 10);
}

int errno = 0;

void exit(int x) {
  // .... what to do
}

static char* argv[] = { "mirage", NULL };
static int inited;
static const char* module_name;

static MALLOC_DEFINE(M_MIRAGE, "mirage", "Mirage/kFreeBSD");

#ifdef MEM_LEAK
enum allocation_type {
	ALLOC_MALLOC,
	ALLOC_CONTIG
};

struct allocation_info {
	TAILQ_ENTRY(allocation_info) ai_next;
	char *ai_file;
	int ai_line;
	unsigned long ai_size;
	void *ai_ptr;
	enum allocation_type ai_type;
	char *ai_comment;
};

static TAILQ_HEAD(allocations_head, allocation_info) aihead =
    TAILQ_HEAD_INITIALIZER(aihead);

static struct mtx aihead_lock;
#endif

enum thread_state {
	THR_NONE,
	THR_RUNNING,
	THR_STOPPED
};

static enum thread_state mirage_kthread_state = THR_NONE;
static struct thread *mirage_kthread = NULL;
static const long mirage_minmem = 32 << 20; /* Minimum limit: 32 MB */
static long mirage_memlimit;

int event_handler(struct module *module, int event, void *arg);

static int allocated(void);
static void mem_cleanup(void);

#if 0
void netif_init(void);
void netif_deinit(void);
#endif

int get_memlimit(void);
char *get_rtparams(void);


static void
mirage_kthread_body(void *arg __unused)
{
	value *v_f;
	int caml_completed = 0;

	mirage_kthread_state = THR_RUNNING;
	caml_startup(argv);
	v_f = caml_named_value("OS.Main.run");

	if (v_f == NULL) {
		printf("[%s] Function 'OS.Main.run' could not be found.\n",
		    module_name);
		goto done;
	}

	for (; (caml_completed == 0) && (mirage_kthread_state == THR_RUNNING);) {
		caml_completed = Bool_val(caml_callback(*v_f, Val_unit));
	}

done:
	v_f = caml_named_value("OS.Main.finalize");

	if (v_f != NULL) {
		caml_callback(*v_f, Val_unit);
	}

	if (mirage_kthread_state == THR_STOPPED) {
		wakeup(&mirage_kthread_state);
  }
	mirage_kthread_state = THR_NONE;
	kthread_exit();
}

static int
mirage_kthread_init(void)
{
	int error;

	error = kthread_add(mirage_kthread_body, NULL, NULL, &mirage_kthread,
	    RFSTOPPED, 40, "mirage");

	mirage_kthread_state = THR_STOPPED;

	if (error != 0)
		printf("[%s] Could not create herding kernel thread.\n",
		    module_name);

	return error;
}

static int
mirage_kthread_deinit(void)
{
	if (mirage_kthread_state == THR_RUNNING) {
		mirage_kthread_state = THR_STOPPED;
		tsleep((void *) &mirage_kthread_state, 0,
		    "mirage_kthread_deinit", 0);
		pause("mirage_kthread_deinit", 1);
	}
	return 0;
}

static void
mirage_kthread_launch(void)
{
	thread_lock(mirage_kthread);
	sched_add(mirage_kthread, SRQ_BORING);
	sched_class(mirage_kthread, PRI_MIN_KERN);
	sched_prio(mirage_kthread, PRI_MIN_KERN);
	thread_unlock(mirage_kthread);
}

#ifdef MEM_LEAK
static void
leakfinder_init(void)
{
	TAILQ_INIT(&aihead);
	mtx_init(&aihead_lock, "aihead", NULL, MTX_DEF);
}
#endif

int
get_memlimit(void)
{
	int limit;
	char buf[256];
	char *env;

	limit = 0;
	env = getenv("mirage.maxmem");

	if (env != NULL)
		limit = atoi(env);

	if (module_name != NULL) {
		snprintf(buf, 255, "mirage.%s.maxmem", module_name);
		buf[255] = '\0';
		env = getenv(buf);

		if (env != NULL)
			limit = atoi(env);
	}

	return max(limit, mirage_minmem);
}

char *
get_rtparams(void)
{
	char buf[256];
	char *env;

	env = NULL;

	if (module_name != NULL) {
		snprintf(buf, 255, "mirage.%s.rtparams", module_name);
		buf[255] = '\0';
		env = getenv(buf);
	}

	if (env == NULL)
		env = getenv("mirage.rtparams");

	return env;
}

int
event_handler(struct module *module, int event, void *arg) {
	int retval;

	retval = 0;

	switch (event) {
	case MOD_LOAD:
		module_name = module_getname(module);
#ifdef MEM_LEAK
		leakfinder_init();
#endif
		printf("[%s] Kernel module is about to load.\n", module_name);
		mirage_memlimit = get_memlimit();
		printf("[%s] Memory limit: %d MB\n", module_name,
		    (int) (mirage_memlimit >> 20));
		//netif_init();
		mirage_kthread_init();
		mirage_kthread_launch();
		inited = 1;
		break;
	case MOD_UNLOAD:
		printf("[%s] Kernel module is about to unload.\n",
		    module_name);
		if (!inited) {
			retval = EALREADY;
			break;
		}
		retval = mirage_kthread_deinit();
		//netif_deinit();
		mem_cleanup();
		break;
	default:
		retval = EOPNOTSUPP;
		break;
	}

	return retval;
}

static int block_timo;

CAMLprim value
caml_block_kernel(value v_timeout)
{
	CAMLparam1(v_timeout);

	block_timo = fixpt_to_int(fixpt_mul(Double_val(v_timeout),
	    fixpt_from_int(hz)));
	pause("caml_block_kernel", block_timo);
	CAMLreturn(Val_unit);
}

static int
allocated(void)
{
	struct malloc_type_internal *mtip;
	long alloced;
	int i;

	mtip = M_MIRAGE->ks_handle;
	alloced = 0;
	for (i = 0; i < MAXCPU; i++)
		alloced += mtip->mti_stats[i].mts_memalloced;

	return alloced;
}

#ifdef MEM_LEAK
static void
register_allocation(void *addr, unsigned long size, char *file, int line,
    enum allocation_type atype, char *comment)
{
	struct allocation_info *a;

	a = (struct allocation_info *) malloc(sizeof(struct allocation_info),
	    M_MIRAGE, M_NOWAIT);
	if (a != NULL) {
		a->ai_file = file;
		a->ai_line = line;
		a->ai_ptr = addr;
		a->ai_size = size;
		a->ai_type = atype;
		a->ai_comment = comment;
		mtx_lock(&aihead_lock);
		TAILQ_INSERT_HEAD(&aihead, a, ai_next);
		mtx_unlock(&aihead_lock);
	}
	else
	printf("Warning: could not track allocation: p=%p, size=%ld, "
	    "loc=%s:%d\n", addr, size, file, line);
}

static void
unregister_allocation(void *addr, unsigned long size, char *file, int line)
{
	struct allocation_info *a, *a_tmp;

	mtx_lock(&aihead_lock);
	TAILQ_FOREACH_SAFE(a, &aihead, ai_next, a_tmp) {
		if (a->ai_ptr == addr) {
			if ((size > 0 && a->ai_size == size) || size == 0) {
				TAILQ_REMOVE(&aihead, a, ai_next);
				if (a->ai_comment)
					free(a->ai_comment, M_MIRAGE);
				free(a, M_MIRAGE);
			}
			else
			if (size > 0) {
				a->ai_size -= size;
			}
			else
			printf("Warning: could not track free: p=%p, size=%ld, "
			    "loc=%s:%d\n", addr, size, file, line);
			break;
		}
	}
	mtx_unlock(&aihead_lock);
}
#endif

#ifdef MEM_DEBUG
void *
mir_malloc(unsigned long size, int flags, char* file, int line, char *comment)
#else
void *
mir_malloc(unsigned long size, int flags)
#endif
{
#ifdef MEM_LEAK
	void *p;
#endif

	if (allocated() + size > mirage_memlimit) return NULL;

#ifdef MEM_LEAK
	p = malloc(size, M_MIRAGE, flags);

	if (p != NULL)
		register_allocation(p, size, file, line, ALLOC_MALLOC,
		    comment);

	return p;
#else
	return malloc(size, M_MIRAGE, flags);
#endif
}

#ifdef MEM_DEBUG
void *
mir_realloc(void *addr, unsigned long size, int flags, char* file, int line,
    char *comment)
#else
void *
mir_realloc(void *addr, unsigned long size, int flags)
#endif
{
	uma_slab_t slab;
	u_long old_size;
#ifdef MEM_LEAK
	void *p;
#endif

	slab = vtoslab((vm_offset_t)addr & (~UMA_SLAB_MASK));

	if (slab == NULL)
		return NULL;

	old_size = (!(slab->us_flags & UMA_SLAB_MALLOC)) ?
	    slab->us_keg->uk_size : slab->us_size;

	if (allocated() + (size - old_size) > mirage_memlimit) return NULL;

#ifdef MEM_LEAK
	p = realloc(addr, size, M_MIRAGE, flags);

	if (p != NULL) {
		unregister_allocation(addr, 0, file, line);
		register_allocation(p, size, file, line, ALLOC_MALLOC,
		    comment);
	}

	return p;
#else
	return realloc(addr, size, M_MIRAGE, flags);
#endif
}

#ifdef MEM_DEBUG
void *
mir_contigmalloc(unsigned long size, int flags, vm_paddr_t low,
    vm_paddr_t high, unsigned long alignment, unsigned long boundary,
    char *file, int line, char *comment)
#else
void *
mir_contigmalloc(unsigned long size, int flags, vm_paddr_t low,
    vm_paddr_t high, unsigned long alignment, unsigned long boundary)
#endif
{
#ifdef MEM_LEAK
	void *p;
#endif

	if (allocated() + size > mirage_memlimit) return NULL;

#ifdef MEM_LEAK
	p = contigmalloc(size, M_MIRAGE, flags, low, high, alignment,
	    boundary);

	if (p != NULL)
		register_allocation(p, size, file, line, ALLOC_CONTIG,
		    comment);

	return p;
#else
	return contigmalloc(size, M_MIRAGE, flags, low, high, alignment,
	    boundary);
#endif
}

#ifdef MEM_DEBUG
void
mir_free(void *addr, char *file, int line)
#else
void
mir_free(void *addr)
#endif
{
	free(addr, M_MIRAGE);
#ifdef MEM_LEAK
	unregister_allocation(addr, 0, file, line);
#endif
}

#ifdef MEM_DEBUG
void
mir_contigfree(void *addr, unsigned long size, char *file, int line)
#else
void
mir_contigfree(void *addr, unsigned long size)
#endif
{
	contigfree(addr, size, M_MIRAGE);
#ifdef MEM_LEAK
	unregister_allocation(addr, size, file, line);
#endif
}

#ifdef MEM_LEAK
static void
check_for_leaks(void)
{
	struct allocation_info *a, *b;
	int i, total;

	mtx_lock(&aihead_lock);
	a = TAILQ_FIRST(&aihead);
	i = 0;
	total = 0;

	if (a != NULL)
		printf("Memory leaks found:\n");

	while (a != NULL) {
		printf("[%d] p=%p, size=%ld, loc=%s:%d%s", i++, a->ai_ptr,
		    a->ai_size, a->ai_file, a->ai_line,
		    a->ai_comment ? " " : "\n");

		if (a->ai_comment) {
			printf("(%s)\n", a->ai_comment);
			free(a->ai_comment, M_MIRAGE);
		}

		total += a->ai_size;

		switch (a->ai_type) {
		case ALLOC_MALLOC:
			free(a->ai_ptr, M_MIRAGE);
			break;
		case ALLOC_CONTIG:
			contigfree(a->ai_ptr, a->ai_size, M_MIRAGE);
			break;
		}

		b = TAILQ_NEXT(a, ai_next);
		free(a, M_MIRAGE);
		a = b;
	}

	if (i > 0)
		printf("\n%d allocations, %d bytes leaked.\n\n", i, total);

	TAILQ_INIT(&aihead);
	mtx_unlock(&aihead_lock);
	mtx_destroy(&aihead_lock);
}
#endif /* MEM_DEBUG */

static void
mem_cleanup(void)
{
  extern void caml_free_minor_heap();
  extern void caml_deinit_major_heap();
  extern void caml_final_deinit();
  extern void caml_deinit_custom_operations();
  extern void caml_deinit_atoms();
  extern void caml_free_global_roots();
  extern void caml_free_named_values();
  extern void caml_deinit_frame_descriptors();
  extern void caml_page_table_deinitialize();
  extern void caml_deinit_backtrace_buffer();
	caml_free_minor_heap();
	caml_deinit_major_heap();
	caml_final_deinit();
	caml_deinit_custom_operations();
	caml_deinit_atoms();
	caml_free_global_roots();
	caml_free_named_values();
	caml_close_all_channels();
	caml_deinit_frame_descriptors();
	caml_page_table_deinitialize();
  caml_deinit_backtrace_buffer();
#ifdef MEM_LEAK
	check_for_leaks();
#endif
}
