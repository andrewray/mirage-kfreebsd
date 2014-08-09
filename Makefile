
KERNELSRC=/usr/src/sys
PLATFORM=amd64

INCLUDES = \
	-I. \
	-Istdinc \
	-Icaml \
	-Icaml/$(PLATFORM) \
	-Ifixpt \
	-I$(KERNELSRC) \
	-I$(KERNELSRC)/contrib/altq

DEFINES = \
	-DMEM_DEBUG \
	-D_KERNEL \
	-DKLD_MODULE \
	-DCAML_NAME_SPACE \
	-DNATIVE_CODE \
	-DSYS_bsd_elf \
	-DTARGET_$(PLATFORM)

# These warnings seem to be turned off by general kernel compilation as well
#
#-Wno-error-tautological-compare 
#-Wno-error-empty-body  
#-Wno-error-parentheses-equality
#
# this is a very annoying option.
#-Wmissing-prototypes 
WARNINGS = \
	-Werror \
	-Wall \
	-Wredundant-decls \
	-Wnested-externs \
	-Wstrict-prototypes \
	-Wpointer-arith \
	-Winline \
	-Wcast-qual \
	-Wundef \
	-Wno-pointer-sign \
	-Wmissing-include-dirs

# these are not used by the kfreebsd version
#-mno-omit-leaf-frame-pointer 
#-fno-strict-aliasing 
CODEGEN = \
	-mno-omit-leaf-frame-pointer \
	-mno-aes \
	-mno-avx \
	-mcmodel=kernel \
	-mno-red-zone \
	-mno-mmx \
	-mno-sse \
	-msoft-float \
	-fno-strict-aliasing \
	-fno-common  \
	-fno-omit-frame-pointer \
	-fno-asynchronous-unwind-tables \
	-ffreestanding \
	-fstack-protector \
	-fstack-protector \
	-fformat-extensions \
	-fdiagnostics-show-option  

# -pipe and -Qunused-arguments are not used by kfreebsd
CLAGS = \
	-O2 \
	-pipe \
	-nostdinc \
	-std=iso9899:1999 \
	-Qunused-arguments \
	$(INCLUDES) $(DEFINES) $(WARNINGS) $(CODEGEN)

.SUFFIXES: .c .o

#caml/natdynlink.c 
#caml/unix.c 
SRC = \
	caml/alloc.c \
	caml/array.c \
	caml/backtrace.c \
	caml/bigarray_stubs.c \
	caml/callback.c \
	caml/compact.c \
	caml/compare.c \
	caml/custom.c \
	caml/debugger.c \
	caml/dynlink.c \
	caml/extern.c \
	caml/fail.c \
	caml/finalise.c \
	caml/floats.c \
	caml/freelist.c \
	caml/gc_ctrl.c \
	caml/globroots.c \
	caml/hash.c \
	caml/intern.c \
	caml/ints.c \
	caml/io.c \
	caml/lexing.c \
	caml/main.c \
	caml/major_gc.c \
	caml/md5.c \
	caml/memory.c \
	caml/meta.c \
	caml/minor_gc.c \
	caml/misc.c \
	caml/obj.c \
	caml/parsing.c \
	caml/printexc.c \
	caml/roots.c \
	caml/signals.c \
	caml/signals_asm.c \
	caml/stacks.c \
	caml/startup.c \
	caml/str.c \
	caml/strstubs.c \
	caml/sys.c \
	caml/weak.c \
	caml/amd64/amd64.S \
	kernel/clock_stubs.c \
	kernel/page_stubs.c \
	/usr/home/andyman/dev/ocaml-cstruct-1.3.1/lib/cstruct_stubs.c \
	kernel/kmod.c

SRC += fixpt/fixmath.c

SRC_ = ${SRC:.c=.o}
OBJ  = ${SRC_:.S=.o}

.PHONY: ocaml_lib clean test

all: links libmir.a ocaml_lib test

machine:
	ln -s $(KERNELSRC)/$(PLATFORM)/include ${.TARGET}

x86:
	ln -s $(KERNELSRC)/x86/include ${.TARGET}

links: machine x86

.c.o: 
	cc $(CLAGS) -o ${.TARGET} -c ${.IMPSRC}

.S.o: 
	cc $(CLAGS) -D__ASSEMBLY__ -o ${.TARGET} -c ${.IMPSRC}

libmir.a: $(OBJ)
	ar rc libmir.a $(OBJ)

ocaml_lib:
	(cd lib; make)

clean:
	(cd lib; make clean)
	(cd test; make clean)
	rm -f x86 machine libmir.a
	rm -f $(OBJ)
	find . -name "*~" | xargs rm -f

test:
	(cd test; make)

load:
	sudo kldload test/main.ko

unload:
	sudo kldunload test/main.ko

