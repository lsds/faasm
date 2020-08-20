//
// Created by Joshua Heinemann on 04.06.20.
// TU-Braunschweig (heineman@ibr.cs.tu-bs.de)
//
//TESTBENCH//
#include <xra.h>
//ENDTESTBENCH//
#include <sgx.h>
#include <sgx_defs.h>
#include <sgx/sgx_wamr_enclave_types.h>
#include <util/rw_lock.h>
#include <sgx/faasm_sgx_error.h>
#include <tlibc/mbusafecrt.h>
#include <iwasm/include/wasm_export.h>
#include <string.h>
#include <sgx_thread.h>
#if(FAASM_SGX_ATTESTATION)
#include <sgx/sgx_wamr_attestation.h>
#endif
#if(FAASM_SGX_WHITELISTING)
#include <sgx/sgx_wamr_whitelisting.h>
#endif
#if(WASM_ENABLE_INTERP == 1)
#include <iwasm/interpreter/wasm_runtime.h>
#endif
#if(WASM_ENABLE_AOT == 1)
#include <iwasm/aot/aot_runtime.h>
#endif
#if(FAASM_SGX_ATTESTATION)
#define INCREMENT_MSG_ID() \
__sync_fetch_and_add(&_sgx_wamr_msg_id, 1)
#endif
extern "C"{
    //TESTBENCH//
    uint32_t counter = 0;
    //ENDTESTBENCH
    typedef void(*os_print_function_t)(const char* msg);
    extern void os_set_print_function(os_print_function_t pf);
    extern int os_printf(const char* message, ...);
    extern sgx_status_t SGX_CDECL ocall_printf(const char* msg);
    extern NativeSymbol sgx_wamr_native_symbols[28];
#if(FAASM_SGX_ATTESTATION)
    extern sgx_status_t SGX_CDECL ocall_init_crt(faasm_sgx_status_t* ret_val);
    extern sgx_status_t SGX_CDECL ocall_send_msg(faasm_sgx_status_t* ret_val, sgx_wamr_msg_t* msg, uint32_t msg_len);
    static uint8_t _sgx_wamr_msg_id = 0;
#endif
#if(FAASM_SGX_ATTESTATION || FAASM_SGX_WHITELISTING)
    __thread uint32_t tls_thread_id;
#endif
    _sgx_wamr_tcs_t* sgx_wamr_tcs = NULL;
    static uint32_t _sgx_wamr_tcs_len;
    static sgx_thread_mutex_t _mutex_sgx_wamr_tcs = SGX_THREAD_MUTEX_INITIALIZER;
    static rwlock_t _rwlock_sgx_wamr_tcs_realloc = {0};
    static char _wamr_global_heap_buffer[FAASM_SGX_WAMR_HEAP_SIZE * 1024];
#if(FAASM_SGX_ATTESTATION)
    static inline faasm_sgx_status_t __get_response_msg(const uint32_t thread_id, sgx_wamr_msg_t** response_ptr){
        read_lock(&_rwlock_sgx_wamr_tcs_realloc);
        *response_ptr = *sgx_wamr_tcs[thread_id].response_ptr;
        read_unlock(&_rwlock_sgx_wamr_tcs_realloc);
        return FAASM_SGX_SUCCESS;
    }
    static faasm_sgx_status_t recv_msg(uint32_t thread_id, void** payload_ptr, uint32_t* payload_len){//TODO: Maybe replace thread_id with __thread
        if(thread_id >= _sgx_wamr_tcs_len)
            return FAASM_SGX_INVALID_THREAD_ID;
        if(!payload_ptr || !payload_len)
            return FAASM_SGX_INVALID_PTR;
        faasm_sgx_status_t ret_val;
        sgx_wamr_msg_t* response_ptr;
        if((ret_val = __get_response_msg(thread_id, &response_ptr)) != FAASM_SGX_SUCCESS)
            return ret_val;
        //DO DECRYPTION STUFF
        //IMPLEMENT ME:P
        //
        *payload_ptr = (void*)response_ptr->payload;
        *payload_len = response_ptr->payload_len;
        return FAASM_SGX_SUCCESS;
    }
    static faasm_sgx_status_t send_msg(const void* payload_ptr, const uint32_t payload_len){
        sgx_wamr_msg_t* msg_ptr;
        sgx_status_t sgx_ret_val;
        faasm_sgx_status_t ret_val;
        if(!payload_ptr)
            return FAASM_SGX_INVALID_PTR;
        if(!payload_len)
            return FAASM_SGX_INVALID_PAYLOAD_LEN;
        if(!(msg_ptr = (sgx_wamr_msg_t*) calloc((sizeof(sgx_wamr_msg_t) + payload_len), sizeof(uint8_t)))){
            return FAASM_SGX_OUT_OF_MEMORY;
        }
        msg_ptr->msg_id = INCREMENT_MSG_ID();
        ///////////ENCRYPTION///////////
        //Implement me :P
        ///////////ENCRYPTION///////////
        ///////////REMOVE IF ENCRYPTION WORKS///////////
        msg_ptr->payload_len = payload_len;
        memcpy(((uint8_t*)msg_ptr->payload),payload_ptr, payload_len);
        ///////////REMOVE IF ENCRYPTION WORKS///////////
        if((sgx_ret_val = ocall_send_msg(&ret_val,msg_ptr,sizeof(sgx_wamr_msg_t) + msg_ptr->payload_len)) != SGX_SUCCESS){
            free(msg_ptr);
            return FAASM_SGX_OCALL_ERROR(sgx_ret_val);
        }
        free(msg_ptr);
        return ret_val;
    }
#endif
    static inline faasm_sgx_status_t __get_tcs_slot(uint32_t* thread_id){
        _sgx_wamr_tcs_t* temp_ptr;
        uint32_t temp_len, i = 0;
        sgx_thread_mutex_lock(&_mutex_sgx_wamr_tcs);
        read_lock(&_rwlock_sgx_wamr_tcs_realloc);
        for(; i < _sgx_wamr_tcs_len; i++){
            if(sgx_wamr_tcs[i].module == NULL){
                sgx_wamr_tcs[i].module = (WASMModuleCommon*) 0x1;
                sgx_thread_mutex_unlock(&_mutex_sgx_wamr_tcs);
                read_unlock(&_rwlock_sgx_wamr_tcs_realloc);
                *thread_id = i;
                return FAASM_SGX_SUCCESS;
            }
        }
        read_unlock(&_rwlock_sgx_wamr_tcs_realloc);
        temp_len = (_sgx_wamr_tcs_len << 1);
        write_lock(&_rwlock_sgx_wamr_tcs_realloc);
        if((temp_ptr = (_sgx_wamr_tcs_t*) realloc(sgx_wamr_tcs, (temp_len * sizeof(_sgx_wamr_tcs_t)))) != NULL){
            memset((void*)(temp_ptr + _sgx_wamr_tcs_len), 0x0, (temp_len - _sgx_wamr_tcs_len) * sizeof(_sgx_wamr_tcs_t)); //Have to zero out new memory because realloc can refer to already used memory
            sgx_wamr_tcs = temp_ptr;
            _sgx_wamr_tcs_len = temp_len;
            sgx_wamr_tcs[i].module = (WASMModuleCommon*) 0x1;
            write_unlock(&_rwlock_sgx_wamr_tcs_realloc);
            sgx_thread_mutex_unlock(&_mutex_sgx_wamr_tcs);
            *thread_id = i;
            return FAASM_SGX_SUCCESS;
        }
        write_unlock(&_rwlock_sgx_wamr_tcs_realloc);
        sgx_thread_mutex_unlock(&_mutex_sgx_wamr_tcs);
        return FAASM_SGX_OUT_OF_MEMORY;
    }
    faasm_sgx_status_t sgx_wamr_enclave_call_function(const uint32_t thread_id, const uint32_t func_id){
        wasm_function_inst_t wasm_function;
        read_lock(&_rwlock_sgx_wamr_tcs_realloc);
        WASMModuleInstance* wasm_module_inst_ptr = (WASMModuleInstance*) sgx_wamr_tcs[thread_id].module_inst;
        read_unlock(&_rwlock_sgx_wamr_tcs_realloc);
        char func_id_str[33];
        if(_itoa_s(func_id,func_id_str,sizeof(func_id_str),10))
            return FAASM_SGX_INVALID_FUNC_ID;
        if(thread_id >= _sgx_wamr_tcs_len)
            return FAASM_SGX_INVALID_THREAD_ID;
        if(!(wasm_function = wasm_runtime_lookup_function((WASMModuleInstanceCommon*) wasm_module_inst_ptr, func_id_str, NULL)))
            return FAASM_SGX_WAMR_FUNCTION_NOT_FOUND;
#if(FAASM_SGX_ATTESTATION)
        tls_thread_id = thread_id;
#endif
        if(!(wasm_create_exec_env_and_call_function(wasm_module_inst_ptr, (WASMFunctionInstance*)wasm_function, 0, 0x0))){
#if(WASM_ENABLE_INTERP == 1 && WASM_ENABLE_AOT == 0)
            if(!memcmp(wasm_module_inst_ptr->cur_exception,_WRAPPER_ERROR_PREFIX,sizeof(_WRAPPER_ERROR_PREFIX))){
                faasm_sgx_status_t ret_val = *(faasm_sgx_status_t*)&(wasm_module_inst_ptr->cur_exception[sizeof(_WRAPPER_ERROR_PREFIX)]);
                return ret_val;
            }
            ocall_printf(wasm_module_inst_ptr->cur_exception);
#elif(WASM_ENABLE_INTERP == 0 && WASM_ENABLE_AOT == 1)
            ocall_printf(((AOTModuleInstance*) wasm_module_inst_ptr)->cur_exception);
#else
            ocall_printf(((WASMModuleInstanceCommon*)wasm_module_inst_ptr)->module_type == Wasm_Module_Bytecode? ((WASMModuleInstance*)wasm_module_inst_ptr)->cur_exception : ((AOTModuleInstance*)wasm_module_inst_ptr)->cur_exception);
#endif
            return FAASM_SGX_WAMR_FUNCTION_UNABLE_TO_CALL;
        }
        return FAASM_SGX_SUCCESS;
    }
    faasm_sgx_status_t sgx_wamr_enclave_unload_module(const uint32_t thread_id){
        if(thread_id >= _sgx_wamr_tcs_len)
            return FAASM_SGX_INVALID_THREAD_ID;
        read_lock(&_rwlock_sgx_wamr_tcs_realloc);
        if(sgx_wamr_tcs[thread_id].module == 0x0){
            read_unlock(&_rwlock_sgx_wamr_tcs_realloc);
            return FAASM_SGX_MODULE_NOT_LOADED;
        }
        wasm_runtime_unload(sgx_wamr_tcs[thread_id].module);
        wasm_runtime_deinstantiate(sgx_wamr_tcs[thread_id].module_inst);
        free(sgx_wamr_tcs[thread_id].wasm_opcode);
        sgx_wamr_tcs[thread_id].module = 0x0;
        read_unlock(&_rwlock_sgx_wamr_tcs_realloc);
        return FAASM_SGX_SUCCESS;
    }
#if(FAASM_SGX_ATTESTATION)
    faasm_sgx_status_t sgx_wamr_enclave_load_module(const void* wasm_opcode_ptr, const uint32_t wasm_opcode_size, uint32_t* thread_id, sgx_wamr_msg_t** response_ptr){
#else
    faasm_sgx_status_t sgx_wamr_enclave_load_module(const void* wasm_opcode_ptr, const uint32_t wasm_opcode_size, uint32_t* thread_id){
#endif
        char module_error_buffer[FAASM_SGX_WAMR_MODULE_ERROR_BUFFER_SIZE];
        faasm_sgx_status_t ret_val;
        memset(module_error_buffer, 0x0, sizeof(module_error_buffer));
        if(!wasm_opcode_size)
            return FAASM_SGX_INVALID_OPCODE_SIZE;
#if(FAASM_SGX_ATTESTATION)
        if(!wasm_opcode_ptr || !response_ptr)
#else
        if(!wasm_opcode_ptr)
#endif
            return FAASM_SGX_INVALID_PTR;
        if((ret_val = __get_tcs_slot(thread_id)) != FAASM_SGX_SUCCESS)
            return ret_val;
#if(FAASM_SGX_WHITELISTING)
        tls_thread_id = *thread_id;
#endif
        read_lock(&_rwlock_sgx_wamr_tcs_realloc);
#if(FAASM_SGX_ATTESTATION)
        sgx_wamr_tcs[*thread_id].response_ptr = response_ptr;
#endif
        if(!(sgx_wamr_tcs[*thread_id].wasm_opcode = (uint8_t*) calloc(wasm_opcode_size, sizeof(uint8_t)))){
            sgx_wamr_tcs[*thread_id].module = 0x0;
            read_unlock(&_rwlock_sgx_wamr_tcs_realloc);
            return FAASM_SGX_OUT_OF_MEMORY;
        }
        memcpy(sgx_wamr_tcs[*thread_id].wasm_opcode, wasm_opcode_ptr, wasm_opcode_size);
        //TESTBENCH//
        char demo_payload[] = {"Hello World"}, *demo_recv;
        uint32_t demo_recv_len_ptr;
        if(send_msg((void*) demo_payload,sizeof(demo_payload)) != FAASM_SGX_SUCCESS){
            asm("ud2");
        }
        if(recv_msg(*thread_id,(void**)&demo_recv,&demo_recv_len_ptr) != FAASM_SGX_SUCCESS){
            asm("ud2");
        }
        if(memcmp((const void*) demo_payload,(const void*) demo_recv,demo_recv_len_ptr)){
            asm("ud2");
        }
        os_printf("demo_payload: %s \t demo_recv: %s\n",demo_payload,demo_recv);
        xra_report_t xra_report;
        xra_status_t xra_ret_val;
        sgx_status_t sgx_ret_val;
        char* whitelist;
        if(!counter){
            whitelist = (char*) malloc(sizeof("puts"));
            memcpy(whitelist, "puts",sizeof("puts"));
            counter++;
        }else if(counter == 1){
            whitelist = (char*) malloc(sizeof("puts|printf"));
            memcpy(whitelist, "puts|printf",sizeof("puts|printf"));
            counter++;
        }else if(counter == 2 || counter == 3){
            whitelist = (char*) malloc(sizeof("*"));
            memcpy(whitelist, "*",sizeof("*"));
            counter++;
        }else{
        whitelist = (char*) malloc(sizeof(""));
            memcpy(whitelist,"",sizeof(""));
        }
        _register_whitelist(whitelist,'|');
        if((xra_ret_val = xra_create_report(sgx_wamr_tcs[*thread_id].wasm_opcode,wasm_opcode_size,(uint8_t*)whitelist,&xra_report)) != XRA_SUCCESS)
            __asm("ud2");
        sgx_sha256_hash_t wasm_opcode_hash, whitelist_hash;
        if((sgx_ret_val = sgx_sha256_msg(sgx_wamr_tcs[*thread_id].wasm_opcode,wasm_opcode_size,&wasm_opcode_hash)) != SGX_SUCCESS)
            __asm("ud2");
        if((sgx_ret_val = sgx_sha256_msg((uint8_t*)whitelist,strlen(whitelist) + 1,&whitelist_hash)) != SGX_SUCCESS)
            __asm("ud2");
        if(memcmp(xra_report.wasm_opcode_hash,&wasm_opcode_hash,SGX_SHA256_HASH_SIZE))
            __asm("ud2");
        if(memcmp(xra_report.whitelist_hash,&whitelist_hash,SGX_SHA256_HASH_SIZE))
            __asm("ud2");
        //ENDTESTBENCH//
        if(!(sgx_wamr_tcs[*thread_id].module = wasm_runtime_load((uint8_t*)sgx_wamr_tcs[*thread_id].wasm_opcode, wasm_opcode_size, module_error_buffer, sizeof(module_error_buffer)))){
            free(sgx_wamr_tcs[*thread_id].wasm_opcode);
            sgx_wamr_tcs[*thread_id].module = 0x0;
            read_unlock(&_rwlock_sgx_wamr_tcs_realloc);
            ocall_printf(module_error_buffer);
            return FAASM_SGX_WAMR_MODULE_LOAD_FAILED;
        }
        //TESTBENCH//
        _free_whitelist();
        os_printf("Test was successful\n");
        //ENDTESTBENCH//
        if(!(sgx_wamr_tcs[*thread_id].module_inst = wasm_runtime_instantiate(sgx_wamr_tcs[*thread_id].module, (uint32_t)FAASM_SGX_WAMR_INSTANCE_DEFAULT_STACK_SIZE, (uint32_t)FAASM_SGX_WAMR_INSTANCE_DEFAULT_HEAP_SIZE, module_error_buffer, sizeof(module_error_buffer)))){
            free(sgx_wamr_tcs[*thread_id].wasm_opcode);
            wasm_runtime_unload(sgx_wamr_tcs[*thread_id].module);
            sgx_wamr_tcs[*thread_id].module = 0x0;
            read_unlock(&_rwlock_sgx_wamr_tcs_realloc);
            ocall_printf(module_error_buffer);
            return FAASM_SGX_WAMR_MODULE_INSTANTIATION_FAILED;
        }
        read_unlock(&_rwlock_sgx_wamr_tcs_realloc);
        return FAASM_SGX_SUCCESS;
    }
    faasm_sgx_status_t sgx_wamr_enclave_init_wamr(const uint32_t thread_number){
        sgx_status_t sgx_ret_val;
        faasm_sgx_status_t ret_val;
        os_set_print_function((os_print_function_t)ocall_printf);
        RuntimeInitArgs wamr_runtime_init_args;
        if((sgx_wamr_tcs = (_sgx_wamr_tcs_t*) calloc(thread_number, sizeof(_sgx_wamr_tcs_t))) == NULL){
            return FAASM_SGX_OUT_OF_MEMORY;
        }
        _sgx_wamr_tcs_len = thread_number;
        memset(&wamr_runtime_init_args, 0x0, sizeof(wamr_runtime_init_args));
        wamr_runtime_init_args.mem_alloc_type = Alloc_With_Pool;
        wamr_runtime_init_args.mem_alloc_option.pool.heap_buf = _wamr_global_heap_buffer;
        wamr_runtime_init_args.mem_alloc_option.pool.heap_size = sizeof(_wamr_global_heap_buffer);
        wamr_runtime_init_args.native_module_name = "env";
        wamr_runtime_init_args.native_symbols = sgx_wamr_native_symbols;
        wamr_runtime_init_args.n_native_symbols = (sizeof(sgx_wamr_native_symbols) / sizeof(NativeSymbol));
        if(!wasm_runtime_full_init(&wamr_runtime_init_args))
            return FAASM_SGX_WAMR_RTE_INIT_FAILED;
#if(FAASM_SGX_ATTESTATION)
        if((sgx_ret_val = ocall_init_crt(&ret_val)) != SGX_SUCCESS)
            return FAASM_SGX_OCALL_ERROR(sgx_ret_val);
        if(ret_val != FAASM_SGX_SUCCESS)
            return ret_val;
#endif
        return FAASM_SGX_SUCCESS;
    }
};