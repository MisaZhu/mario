#ifndef MARIO_BC_H
#define MARIO_BC_H

#include "mario.h"

#ifdef __cplusplus /* __cplusplus */
extern "C" {
#endif

void vm_gen_mbc(vm_t* vm, const char* fname_out);
bool vm_load_mbc(vm_t* vm, const char* fname);
void bc_dump_out(bytecode_t* bc);

#ifdef __cplusplus /* __cplusplus */
}
#endif

#endif