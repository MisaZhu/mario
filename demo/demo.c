#include "mario.h"
#include "bcdump/bcdump.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>

#define ERR_MAX 1023
char _err_info[ERR_MAX+1];

char* load_script(const char* fname) {
	int fd = open(fname, O_RDONLY);
	if(fd < 0) {
		snprintf(_err_info, ERR_MAX, "Can not open file '%s'\n", fname);
		mario_debug(_err_info);
		return NULL;
	}

	struct stat st;
	fstat(fd, &st);

	char* s = (char*)_malloc(st.st_size+1);
	if(s != NULL) {
		read(fd, s, st.st_size);
		s[st.st_size] = 0;
	}
	close(fd);
	return s;
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

bool _dump = false;
static int doargs(int argc, char* argv[]) {
	int c = 0;
	while (c != -1) {
		c = getopt (argc, argv, "ad?");
		if(c == -1)
			break;

		switch (c) {
		case 'a':
			_dump = true;
			break;
		case 'd':
			_m_debug = true;
			break;
		case '?':
			return -1;
		default:
			c = -1;
			break;
		}
	}
	return optind;
}

bool compile(bytecode_t *bc, const char* input);

int main(int argc, char** argv) {
	const char* fname = "";

	_dump = false;
	_free = free;
	_malloc = malloc;
	_out_func = out;

	int argind = doargs(argc, argv);
	if(argind < 0 || argind >= argc) {
		mario_error("Usage: demo (-d/-a) [source_file]!\n");
		return 1;
	}

	fname = argv[argind];
	vm_t* vm = vm_new(compile);
	vm_init(vm, NULL, NULL);
	vm_reg_static(vm, NULL, "print()", native_print, NULL);

	char* s = load_script(fname);
	if(s == NULL) {
		mario_error("Load script failed!\n");
		return 1;
	}

	bool res = vm_load(vm, s);
	_free(s);

	if(res) {
		if(_dump) {
			mstr_t* dump = bc_dump(&vm->bc);
			if(dump != NULL) {
				_out_func(dump->cstr);
				mstr_free(dump);
			}
		}
		else
			vm_run(vm);
	}
	vm_close(vm);
	return 0;
}
