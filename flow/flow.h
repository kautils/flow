#ifndef FLOW_FILTER_ARITHMETIC_SUBTRACT_SUBTRACT_FLOW_FLOW_CC
#define FLOW_FILTER_ARITHMETIC_SUBTRACT_SUBTRACT_FLOW_FLOW_CC

#include <stdint.h>

#define FILTER_DATABASE_OPTION_OVERWRITE 1 // ignore / overwrite
#define FILTER_DATABASE_OPTION_WITHOUT_ROWID 2 // not use rowid, if database dose not support rowid, then shoudl be ignored (e.g : in case of using filesystem directlly).
#define FILTER_DATABASE_TYPE_SQLITE3 1


struct flow;
struct filter_database_handler;
struct filter{
    int (*main)(filter * m)=0;
    void * (*output)(filter *)=0;
    uint64_t(*output_bytes)(filter *)=0;
    void * (*input)(filter *)=0; 
    uint64_t(*input_bytes)(filter *)=0; 
    const char *(*id)(filter *)=0;
    const char *(*id_hr)(filter *)=0;
    int (*save)(filter *)=0;
    void* m=0;
    filter_database_handler * db=0;
    flow * hdl=0;
    int option=0;
    uint32_t pos = 0;
} __attribute__((aligned(8)));


struct filter_lookup_table{};
void * filter_lookup_table_get_value(filter_lookup_table * flookup_table,const char * key);
flow* flow_initialize();
void flow_free(flow * fhdl);
int flow_input(flow * fhdl,void * data,int32_t block_size);
int flow_push(flow * fhdl,filter* f);
filter * flow_lookup(flow * fhdl,filter_lookup_table * );
int flow_push_with_lookup_table(flow * fhdl,filter_lookup_table * (*filter_lookup_table_initialize)(),void (*filter_lookup_table_free)(filter_lookup_table * f));
int flow_execute(flow * fhdl);
void flow_set_io_length(flow * hdl,uint64_t);
void flow_set_local_uri(flow * hdl,const char * uri);




#endif