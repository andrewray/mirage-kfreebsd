#define _KERNEL 1
#define KLD_MODULE 1

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>

extern int event_handler(module_t m, int w, void *p);

static moduledata_t conf = {
	"mir-app"
,	event_handler
,	NULL
};

DECLARE_MODULE(mir_app, conf, SI_SUB_KLD, SI_ORDER_ANY);

