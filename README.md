# mario
A tiny single file bytecode vm engine, None 3rd libs relied, so can be used on most of embedded systems.

You have to implement "bool compile(bytecode_t *bc, const char* input)" function for your own language(check demo/demo_basic/compiler.c). "const char *_mario_lang"(declared in mario_vm.h) must be assigned as well.


