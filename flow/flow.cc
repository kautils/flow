#include "./flow.h"
#include "stdint.h"
#include <vector>
#include <string>

#include "libgen.h"
#include "sys/stat.h"
#include "kautil/sharedlib/sharedlib.h"


filter_database_handler* filter_database_handler_initialize(const char *);
struct filter_database_handler{
    filter_database_handler* (*alloc)(const char *)=filter_database_handler_initialize;
    void (*free)(void * hdl)=0;
    int (*set_uri)(void * hdl,const char * prfx,const char * id)=0;
    int (*setup)(void * hdl)=0;
    int (*set_output)(void * hdl,const void * begin,const void * end)=0;
    int (*set_input)(void * hdl,const void * begin,const void * end)=0;
    int (*set_io_length)(void * hdl,uint64_t)=0;
    int (*save)(void * hdl)=0;
    int (*sw_overwrite)(void * hdl,bool)=0;
    int (*sw_without_rowid)(void * hdl,bool)=0;
    
    int type=0;
    void * instance=0;
    void * dl=0;
    int last_option=-1;
};



using output_t = void*(*)(filter *);
using output_bytes_t = uint64_t(*)(filter *);
using filter_id_t = const char* (*)(filter *);
using database_type_t = int (*)(filter *);



struct filter_handler_lookup_table{
   filter_lookup_table * (*initialize_lookup_table)()=0;
   void (*free_lookup_table)(filter_lookup_table * f)=0;
   filter_lookup_table * lookup_table=0;
   filter * filter =0;
   void * dl=0; 
};

struct filter_first_member;
struct filter_handler{ 
    uint32_t start_offset=0;
    std::vector<filter*> filters;
    std::vector<filter_handler_lookup_table*> lookup_releaser;
    std::string local_uri;
    uint64_t io_len=0;
    filter_first_member * first_member=0;
};

struct filter_database_handler;
filter_database_handler* filter_database_handler_initialize(filter * f);
filter_handler* filter_handler_initialize(){ return new filter_handler{}; }


void* filter_input(filter * f);
uint64_t filter_input_bytes(filter * f);
int filter_database_setup(filter * f);
int filter_database_save(filter * f);
filter * filter_handler_lookup(filter_handler * fhdl,filter_lookup_table * flookup){
    auto f = new filter{};{
        f->hdl=fhdl;
        f->main= (decltype(f->main))filter_lookup_table_get_value(flookup,"fmain");
        f->output= (output_t)filter_lookup_table_get_value(flookup,"output");
        f->output_bytes= (output_bytes_t)filter_lookup_table_get_value(flookup,"output_bytes");
        f->input = filter_input;
        f->input_bytes = filter_input_bytes;
        f->id= (filter_id_t)filter_lookup_table_get_value(flookup,"id");
        f->id_hr= (filter_id_t)filter_lookup_table_get_value(flookup,"id_hr");
        f->database_type =(database_type_t) filter_lookup_table_get_value(flookup,"database_type");
        f->save=filter_database_save;
        f->m= filter_lookup_table_get_value(flookup,"member");
    }
    return f;
}

filter_handler_lookup_table* _filter_handler_push_with_lookup_table(filter_handler * fhdl
                                          ,filter_lookup_table * (*filter_lookup_table_initialize)()
                                          ,void (*filter_lookup_table_free)(filter_lookup_table * f)){
    if(auto ltb = filter_lookup_table_initialize()){
        auto f  =filter_handler_lookup(fhdl,ltb);
        fhdl->lookup_releaser.emplace_back(new filter_handler_lookup_table{
            .initialize_lookup_table = filter_lookup_table_initialize
            ,.free_lookup_table = filter_lookup_table_free
            ,.lookup_table = ltb
            ,.filter=f
        });
        if(f){
            filter_handler_push(fhdl,f);
            return fhdl->lookup_releaser.back();
        }
    }
    return nullptr;
}

int filter_handler_push_with_lookup_table(filter_handler * fhdl
                                          ,filter_lookup_table * (*filter_lookup_table_initialize)()
                                          ,void (*filter_lookup_table_free)(filter_lookup_table * f)){
    return _filter_handler_push_with_lookup_table(fhdl,filter_lookup_table_initialize,filter_lookup_table_free) ? 0:1;
}


int filter_handler_execute(filter_handler * fhdl){
    for(auto i = fhdl->start_offset; i < fhdl->filters.size(); ++i){
        auto f = fhdl->filters[i];
        f->main(f);
        f->save(f);
    }
    return 0;
}


void filter_handler_set_io_length(filter_handler * hdl,uint64_t len){ hdl->io_len=len; }
void filter_handler_set_local_uri(filter_handler * hdl,const char * uri){ hdl->local_uri = uri; }
int filter_handler_push(filter_handler * fhdl,filter* f){
    f->pos=fhdl->filters.size();
    fhdl->filters.push_back(f);
    return 0;
}

struct filter_first_member{ void * o=0;uint64_t o_bytes=0; }__attribute__((aligned(8)));
void* filter_first_member_output(filter * f){ return reinterpret_cast<filter_first_member*>(f->m)->o; }
uint64_t filter_first_member_output_bytes(filter * f){ 
    return reinterpret_cast<filter_first_member*>(f->m)->o_bytes; 
}
int _filter_handler_input(filter_handler * fhdl,void * data,int32_t block_size){
    fhdl->lookup_releaser.emplace_back(new filter_handler_lookup_table{
        .filter=new filter{}
    });
    
    auto input = fhdl->lookup_releaser.back()->filter;
    {
        fhdl->first_member = new filter_first_member{};{
            fhdl->first_member->o = data;
            fhdl->first_member->o_bytes = block_size*fhdl->io_len; 
        }
        input->m = fhdl->first_member;
        input->output=filter_first_member_output;
        input->output_bytes=filter_first_member_output_bytes;
    }
    filter_handler_push(fhdl,input);
    return 0;
}


void filter_handler_free(filter_handler * fhdl){ 
    for(auto & e : fhdl->lookup_releaser){
        {
            auto f = e->filter;
            if(f->db) f->db->free(f->db->instance);
            delete e->filter;
        }
        
        if(e->free_lookup_table){
            e->free_lookup_table(e->lookup_table);
        }
        
        if(e->dl){
            kautil_dlclose(e->dl);
            e->dl=nullptr;
        }
        
        delete e;
    }
    delete fhdl->first_member;
    delete fhdl; 
}



int filter_database_handler_set_option(filter_database_handler * hdl,int op){
    auto op_ow = op & FILTER_DATABASE_OPTION_OVERWRITE;
    auto op_norowid = op & FILTER_DATABASE_OPTION_WITHOUT_ROWID;
    if(hdl->sw_without_rowid)hdl->sw_without_rowid(hdl->instance, op_ow);
    if(hdl->sw_overwrite)hdl->sw_overwrite(hdl->instance, op_norowid);
    return 0;    
}

int filter_database_save(filter * f){
    if(0== !f->db + !f->save){
        
        auto instance = f->db->instance;
        auto out = (const char *) f->output(f);
        f->db->set_output(instance, out, out + f->output_bytes(f));
    
        auto in = (const char *) f->input(f);
        f->db->set_input(instance, in, in + f->input_bytes(f));
        
        return f->db->save(instance);
    } 
    return 0;
}



void* filter_input(filter * f) { return f->hdl->filters[f->pos-1]->output(f->hdl->filters[f->pos-1]); }
uint64_t filter_input_bytes(filter * f) { return f->hdl->filters[f->pos-1]->output_bytes(f->hdl->filters[f->pos-1]); }



void * filter_lookup_table_get_value(filter_lookup_table * flookup_table,const char * key){
    // sorting is bad for this process. i neither want  not to change the order of the flookup_table nor to copy it 
    auto arr = reinterpret_cast<char **>(flookup_table);
    for(;;arr+=sizeof(filter_lookup_elem)/sizeof(uintptr_t)){
        if(nullptr==arr[0]) break;
        if(!strcmp(key,arr[0]))return reinterpret_cast<void*>(arr[1]);
    }
    return nullptr;
}


using size_of_pointer_t =  uint64_t(*)();
using lookup_table_initialize_t = filter_lookup_table*(*)();
using lookup_table_free_t = void (*)(filter_lookup_table * f);



filter_database_handler* filter_database_handler_initialize(const char * sharedlib){
    struct stat st;
    auto res = (filter_database_handler*) 0;
    if(0==stat(sharedlib,&st)){
        if(auto dl = kautil_dlopen(sharedlib,rtld_lazy|rtld_nodelete)){
            auto pointer_size= (size_of_pointer_t) kautil_dlsym(dl,"size_of_pointer");
            auto initialize= (lookup_table_initialize_t) kautil_dlsym(dl,"lookup_table_initialize");
            auto free_lookup_t= (lookup_table_free_t) kautil_dlsym(dl,"lookup_table_free");
            if(0== !pointer_size + !initialize + !free_lookup_t){
                res = new filter_database_handler{};{
                    auto lookup_tb = initialize();
                    auto ptr_size= pointer_size();
                    res->dl = dl;
                    res->instance = ((lookup_table_initialize_t) filter_lookup_table_get_value(lookup_tb, "initialize"))();
                    res->free =  (typeof(res->free)) filter_lookup_table_get_value(lookup_tb,"free");
                    res->set_uri =  (typeof(res->set_uri)) filter_lookup_table_get_value(lookup_tb,"set_uri");
                    res->setup =  (typeof(res->setup)) filter_lookup_table_get_value(lookup_tb,"setup");
                    res->set_output =  (typeof(res->set_output)) filter_lookup_table_get_value(lookup_tb,"set_output");
                    res->set_input =  (typeof(res->set_input)) filter_lookup_table_get_value(lookup_tb,"set_input");
                    res->set_io_length =  (typeof(res->set_io_length)) filter_lookup_table_get_value(lookup_tb,"set_io_length");
                    res->save =  (typeof(res->save)) filter_lookup_table_get_value(lookup_tb,"save");
                    res->sw_overwrite =  (typeof(res->sw_overwrite)) filter_lookup_table_get_value(lookup_tb,"sw_overwrite");
                    res->sw_without_rowid =  (typeof(res->sw_without_rowid)) filter_lookup_table_get_value(lookup_tb,"sw_without_rowid");
                    free_lookup_t(lookup_tb);
                }
                return res;
            }
            kautil_dlclose(dl);
        }
    }
    return nullptr;
}


int filter_database_setup(filter * f,const char * so) { 
    auto instance = (void * ) 0;
    if(!f->db){
        f->db = filter_database_handler_initialize(so); 
    }
    else{
        f->db->free(f->db->instance);
        f->db=f->db->alloc(so);
    }
    instance = f->db->instance;
    
    filter_database_handler_set_option(f->db,f->option);
    f->db->set_io_length(instance, f->hdl->io_len);
    f->db->set_uri(instance, f->hdl->local_uri.data(), f->id_hr(f));

    f->db->last_option = f->option;
    return f->db->setup(instance); 
}




int filter_handler_push_with_library(filter_handler * fhdl,const char * so_filter){
    if(auto dl = kautil_dlopen(so_filter,rtld_lazy|rtld_nodelete)){
        auto tb_init = (lookup_table_initialize_t) kautil_dlsym(dl,"lookup_table_initialize");
        auto sizeof_ptr = (size_of_pointer_t) kautil_dlsym(dl,"size_of_pointer");
        auto tb_free = (lookup_table_free_t) kautil_dlsym(dl,"lookup_table_free");
        if(auto insert = _filter_handler_push_with_lookup_table(fhdl,tb_init,tb_free))insert->dl = dl;
    } 
    return 1;
}


int filter_handler_input(filter_handler * fhdl,void * data,int32_t block_size){
    // "_filter_handler_input(fhdl,nullptr,0)" is because to omit a branch when accessing input value via output function 
    fhdl->start_offset=2;
    return 0 == _filter_handler_input(fhdl,nullptr,0) + _filter_handler_input(fhdl,data,block_size);
}

int tmain_kautil_flow_static_tmp(const char * database_so,const char * so_filter){

    {
        remove("R:\\flow\\build\\android\\filter.arithmetic.subtract\\KautilFilterArithmeticSubtract.0.0.1\\4724af5.sqlite");
        auto input_len = 100; // all the input/output inside a chain is the same,if the result structure are counted as one data 
        auto arr = new double[input_len];
        for(auto i = 0; i < input_len; ++i)arr[i] = i;

        auto fhdl = filter_handler_initialize();
        filter_handler_set_io_length(fhdl,input_len);
        filter_handler_set_local_uri(fhdl,"./");
        {
            filter_handler_input(fhdl,arr,sizeof(double));
            filter_handler_push_with_library(fhdl,so_filter);
            for(auto i = fhdl->start_offset; i < fhdl->filters.size();++i){
                auto f=fhdl->filters[i];
                f->option=FILTER_DATABASE_OPTION_OVERWRITE | FILTER_DATABASE_OPTION_WITHOUT_ROWID;
                filter_database_setup(f,database_so);
            }
            filter_handler_execute(fhdl);
        }
        filter_handler_free(fhdl);
    }
    
    
    return 0;
}




