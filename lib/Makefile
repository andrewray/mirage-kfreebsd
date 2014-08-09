
all:
	OCAMLPARAM="nodynlink=1,nopic=1,fixedpt=16,_" ocamlbuild -use-ocamlfind oS.cmxa

install:
	mkdir install
	cp _build/oS.cmxa install
	cp _build/oS.cmi install
	cp _build/oS.a install
