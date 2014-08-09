ENV=OCAMLPARAM="nodynlink=1,nopic=1,fixedpt=16,_"
OPAM=`opam config var lib`

all:
	$(ENV) ocamlfind opt -syntax camlp4o -I ../lib/install \
		-package cstruct,lwt,lwt.syntax,mirage-types,mirage-console-kfreebsd,ipaddr \
		-output-obj -o main.native.o -linkpkg -dontlink unix \
		oS.cmxa unikernel.ml main.ml
	cc -c module.c
	ld -nostdlib -r -d -o main.ko main.native.o module.o ../libmir.a
	objcopy --strip-debug main.ko

clean:
	rm -f *.o *.cmx *.cmi *.ko

