#ifndef FLOW_FILTER_ARITHMETIC_SUBTRACT_SUBTRACT_FLOW_FLOW_CC
#define FLOW_FILTER_ARITHMETIC_SUBTRACT_SUBTRACT_FLOW_FLOW_CC

#include <stdint.h>

#define FILTER_DATABASE_OPTION_OVERWRITE 1 // ignore / overwrite
#define FILTER_DATABASE_OPTION_WITHOUT_ROWID 2 // not use rowid, if database dose not support rowid, then shoudl be ignored (e.g : in case of using filesystem directlly).
#define FILTER_DATABASE_TYPE_SQLITE3 1

struct filter_lookup_table{};
struct filter_lookup_elem{
    const char * key = 0; 
    void * value = nullptr;
}__attribute__((aligned(8)));
void * filter_lookup_table_get_value(filter_lookup_table * flookup_table,const char * key);


struct filter_handler;
struct filter_database_handler;
struct filter{
    int (*main)(filter * m)=0;
    void * (*output)(filter *)=0;
    uint64_t(*output_bytes)(filter *)=0;
    void * (*input)(filter *)=0; 
    uint64_t(*input_bytes)(filter *)=0; 
    const char *(*id)(filter *)=0;
    const char *(*id_hr)(filter *)=0;
    int (*database_type)(filter *)=0;
    int (*setup_database)(filter *)=0;
    int (*save)(filter *)=0;
    void* m=0;
    filter_database_handler * db=0;
    filter_handler * hdl=0;
    int option=0;
} __attribute__((aligned(8)));


filter_handler* filter_handler_initialize();
void filter_handler_free(filter_handler * fhdl);
int filter_handler_input(filter_handler * fhdl,void * data,int32_t block_size);
int filter_handler_push(filter_handler * fhdl,filter* f);
filter * filter_handler_lookup(filter_handler * fhdl,filter_lookup_table * );
int filter_handler_push_with_lookup_table(filter_handler * fhdl
                                          ,filter_lookup_table * (*filter_lookup_table_initialize)()
                                          ,void (*filter_lookup_table_free)(filter_lookup_table * f));
int filter_handler_execute(filter_handler * fhdl);
void filter_handler_set_io_length(filter_handler * hdl,uint64_t);
void filter_handler_set_local_uri(filter_handler * hdl,const char * uri);




#endif