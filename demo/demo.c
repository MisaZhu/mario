#include "mario.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>

#define ERR_MAX 1023
char _err_info[ERR_MAX+1];

bool load_script(vm_t* vm, const char* fname) {
	int fd = open(fname, O_RDONLY);
	if(fd < 0) {
		snprintf(_err_info, ERR_MAX, "Can not open file '%s'\n", fname);
		mario_debug(_err_info);
		return false;
	}

	struct stat st;
	fstat(fd, &st);

	char* s = (char*)_malloc(st.st_size+1);
	read(fd, s, st.st_size);
	close(fd);
	s[st.st_size] = 0;

	bool ret = vm_load(vm, s);
	_free(s);
	return ret;
}

static var_t* native_print(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;

	var_t* args = get_func_args(env); 
	mstr_t* ret = mstr_new("");
	mstr_t* str = mstr_new("");
	uint32_t sz = var_array_size(args);
	uint32_t i;
	for(i=0; i<sz; ++i) {
		node_t* n = var_array_get(args, i);
		if(n != NULL) {
			var_to_str(n->var, str);
			mstr_append(ret, str->cstr);
		}
	}
	mstr_free(str);
	mstr_add(ret, '\n');
	_out_func(ret->cstr);
	mstr_free(ret);
	return NULL;
}

static void out(const char* str) {
    write(1, str, strlen(str));
}

bool compile(bytecode_t *bc, const char* input);

int main(int argc, char** argv) {
	const char* fname = "";

	_free = free;
	_malloc = malloc;
	_out_func = out;

	if(argc < 1) {
		mario_debug("Usage: demo [source_file]!\n");
		return 1;
	}

	fname = argv[1];
	vm_t* vm = vm_new(compile);
	vm_init(vm, NULL, NULL);
	vm_reg_static(vm, NULL, "print()", native_print, NULL);

	if(fname[0] != 0) {
		if(load_script(vm, fname)) {
			vm_run(vm);
		}
	}

	vm_close(vm);
	return 0;
}
