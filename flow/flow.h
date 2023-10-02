#ifndef FLOW_FILTER_ARITHMETIC_SUBTRACT_SUBTRACT_FLOW_FLOW_CC
#define FLOW_FILTER_ARITHMETIC_SUBTRACT_SUBTRACT_FLOW_FLOW_CC

#include <stdint.h>

#define FILTER_DATABASE_OPTION_OVERWRITE 1 // ignore / overwrite
#define FILTER_DATABASE_OPTION_WITHOUT_ROWID 2 // not use rowid, if database dose not support rowid, then shoudl be ignored (e.g : in case of using filesystem directlly).
#define FILTER_DATABASE_TYPE_SQLITE3 1


struct flow;
struct filter_internal;
struct filter{
    int (*main)(filter * m)=0;
    uint64_t * (*index)(filter *)=0;
    void * (*output)(filter *)=0;
    uint64_t(*output_bytes)(filter *)=0;
    uint64_t(*output_size)(filter *)=0;
    bool (*output_is_uniformed)(filter * f)=0;
    void * (*input)(filter *)=0; 
    uint64_t(*input_size)(filter *)=0;
    uint64_t(*input_bytes)(filter *)=0; 
    bool(*input_is_uniformed)(filter *)=0; 
    const char *(*id)(filter *)=0;
    const char *(*id_hr)(filter *)=0;
    int (*save)(filter *)=0;
    void (*state_reset)(filter * f)=0;
    bool (*state_next)(filter * f)=0;
    const char * (*state_id)(filter * f)=0;
    bool (*database_close_always)(filter * f)=0; ///@note specify database lifecycle at filter level. use when want to close dbcon each time of flow_execute.
    void* fm=0;
    filter_internal * m=0;
} __attribute__((aligned(sizeof(uintptr_t))));


struct filter_lookup_table{};
void * filter_lookup_table_get_value(filter_lookup_table * flookup_table,const char * key);
flow* flow_initialize();
void flow_free(flow * fhdl);
int flow_input(flow * fhdl,void * data,int32_t block_size);
int flow_push(flow * fhdl,filter* f);
filter * flow_lookup_filter_functions(flow * fhdl,filter_lookup_table * );
int flow_push_with_lookup_table(flow * fhdl,filter_lookup_table * (*filter_lookup_table_initialize)(),void (*filter_lookup_table_free)(filter_lookup_table * f));
int flow_execute(flow * fhdl);
void flow_set_local_uri(flow * hdl,const char * uri);




#endif