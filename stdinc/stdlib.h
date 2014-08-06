#include <string.h>
#include <sys/malloc.h>

void *mir_malloc(unsigned long size, int flags);
void *mir_realloc(void *addr, unsigned long size, int flags);
void *mir_contigmalloc(unsigned long size, int flags, vm_paddr_t low,
  vm_paddr_t high, unsigned long alignment, unsigned long boundary);
void mir_free(void *addr);
void mir_contigfree(void *addr, unsigned long size);

#define malloc(X)                 mir_malloc(X, M_NOWAIT)
#define realloc(P,X)              mir_realloc(P, X, M_NOWAIT)
#define calloc(X,S)               mir_malloc(X * S, M_NOWAIT | M_ZERO)
#define free(X)                   mir_free(X)
#define contigmalloc(S,F,L,H,A,B) mir_contigmalloc(S, F, L, H, A, B)
#define contigfree(P,S)           mir_contigfree(P, S)

int atoi(const char *);
double strtod(const char *, char **);
void exit(int) __attribute__ ((noreturn)); // not strictly true...
#define fflush(x)
