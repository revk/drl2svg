all: git drl2svg

AXL/axl.o: AXL/axl.c
	make -C AXL

drl2svg: drl2svg.c AXL/axl.o
	cc -O -o $@ $< -lpopt -IAXL AXL/axl.o -lcurl

git:
	git submodule update --init

update:
	git submodule update --remote --merge
