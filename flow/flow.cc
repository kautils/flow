#include "./flow.h"
#include "stdint.h"

#include <vector>
#include <string>

#include "libgen.h"
#include "sys/stat.h"
#include "kautil/sharedlib/sharedlib.h"



struct filter_database_handler;
struct filter_internal{
    filter_database_handler * db=0;
    flow * hdl=0;
    int option=0;
    uint32_t pos = 0;
};
static filter * filter_factory(){ return new filter{.m=new filter_internal}; }
int flow_push_input(flow * fhdl,void * data,uint64_t block_size,uint64_t nitems);
filter_database_handler* filter_database_handler_lookup(filter * ,const char *);
//filter_database_handler* filter_database_handler_lookup_with_filter(filter * f);
bool filter_input_is_uniformed(filter * f);
void* filter_input(filter * f);
uint64_t filter_input_bytes(filter * f);
uint64_t filter_input_size(filter *f);
int filter_database_save(filter * f);
void filter_free(filter * f);

struct filter_lookup_elem{
    const char * key = 0; 
    void * value = nullptr;
}__attribute__((aligned(sizeof(uintptr_t))));



#include <map> 
struct filter_database_handler{
    filter_database_handler* (*alloc)(filter * ,const char *)=filter_database_handler_lookup;
    void* (*initialize)(void * hdl)=0;
    void (*free)(void * hdl)=0;
    int (*uri_hasher)(const char * prfx,const char * filter_id,const char * state_id)=0;
    int (*set_uri)(void * hdl,const char * prfx,const char * filter_id,const char * state_id)=0;
    int (*setup)(void * hdl)=0;
    int (*set_index)(void * hdl,uint64_t * begin)=0; // only begin are needed because it's size is the same as output. 
    int (*set_output)(void * hdl,const void * begin,uint64_t,uint64_t)=0;
    int (*set_input)(void * hdl,const void * begin,uint64_t block_size,uint64_t nitems)=0;
    int (*save)(void * hdl)=0;
    int (*sw_overwrite)(void * hdl,bool)=0;
    int (*sw_without_rowid)(void * hdl,bool)=0;
    int (*sw_uniformed)(void * hdl,bool)=0;
    int (*sw_key_is_uniformed)(void * hdl,bool)=0;
    void * instance=0;
    void * dl=0;
    int last_option=-1;
    
    std::map<uint64_t,void*> instance_map;
    
};



using index_t = uint64_t* (*)(filter *);
using output_t = void*(*)(filter *);
using output_size_t = uint64_t (*)(filter *);
using output_bytes_t = uint64_t(*)(filter *);
using filter_id_t = const char* (*)(filter *);
using state_reset_t = void (*)(filter *);
using state_next_t = bool (*)(filter *);
using state_id_t = const char* (*)(filter *);
using output_is_uniformed_t = bool (*)(filter *);
using database_close_always_t = bool (*)(filter *);
using size_of_pointer_t =  uint64_t(*)();
using lookup_table_initialize_t = filter_lookup_table*(*)();
using lookup_table_free_t = void (*)(filter_lookup_table * f);


struct flow_lookup_table{
   filter_lookup_table * (*initialize_lookup_table)()=0;
   void (*free_lookup_table)(filter_lookup_table * f)=0;
   filter_lookup_table * lookup_table=0;
   filter * filter =0;
   void * dl=0; 
};



struct filter_first_member;
struct flow{ 
    uint32_t start_offset=0;
    std::vector<filter*> filters;
    std::vector<flow_lookup_table*> lookup_releaser;
    filter_first_member * first_member=0;
    int db_options;
    std::string path_to_database;
    std::string local_uri;
    std::map<std::string,void*> dls;
};

void * flow_dlopen(flow * m,const char * path_to_so,int op){
    auto [itr,insert] = m->dls.insert({path_to_so, nullptr});
    if(insert){
        itr->second = kautil_dlopen(path_to_so,op);
    }
    return itr->second;
}


bool filter_database_close_always(filter * f){ return false; }
bool filter_is_uniformed(filter * f){ return true; }
flow* flow_initialize(){ return new flow{}; }
bool flow_check_filter_validity(filter * f){
    auto arr = reinterpret_cast<uint64_t*>(f);
    auto arr_len = sizeof(filter)/sizeof(uint64_t);
    auto res=uint64_t(0);
    for(auto i = 0; i < arr_len;++i)res+=!arr[i];
    return !res;
}


///@note default is null == no limitation == length of input and output is the same.
uint64_t * filter_index(){ return nullptr; }

filter * flow_lookup_filter_functions(flow * fhdl,filter_lookup_table * flookup){
    auto f =filter_factory();{
        f->m->hdl=fhdl;
        f->main= (decltype(f->main))filter_lookup_table_get_value(flookup,"fmain");
        f->output= (output_t)filter_lookup_table_get_value(flookup,"output");
        f->output_bytes= (output_bytes_t)filter_lookup_table_get_value(flookup,"output_bytes");
        f->output_size= (output_size_t)filter_lookup_table_get_value(flookup,"output_size");
        f->index= (index_t)filter_lookup_table_get_value(flookup,"index");
        f->index = (index_t) 
                (!uintptr_t(f->index)*uintptr_t(filter_index) 
                 +uintptr_t(f->index));
        f->input = filter_input;
        f->input_bytes = filter_input_bytes;
        f->input_size = filter_input_size;
        f->input_is_uniformed = filter_input_is_uniformed; 
        
        f->id= (filter_id_t)filter_lookup_table_get_value(flookup,"id");
        f->id_hr= (filter_id_t)filter_lookup_table_get_value(flookup,"id_hr");
        f->state_next= (state_next_t)filter_lookup_table_get_value(flookup,"state_next");
        f->state_reset= (state_reset_t)filter_lookup_table_get_value(flookup,"state_reset");
        f->state_id= (state_id_t)filter_lookup_table_get_value(flookup,"state_id");
        f->save=filter_database_save;
        {
            f->output_is_uniformed= (output_is_uniformed_t)filter_lookup_table_get_value(flookup, "output_is_uniformed");
            f->output_is_uniformed = (output_is_uniformed_t) 
                      (uintptr_t(!f->output_is_uniformed) * uintptr_t(filter_is_uniformed)
                    + (uintptr_t(f->output_is_uniformed))); 
        }

        {
            f->database_close_always=(database_close_always_t)filter_lookup_table_get_value(flookup, "database_close_always");
            f->database_close_always=(database_close_always_t) 
                    (!uint64_t(f->database_close_always)*uint64_t(filter_database_close_always)
                     +uint64_t(f->database_close_always));
        }
        
        f->fm= filter_lookup_table_get_value(flookup, "member");
        if(!flow_check_filter_validity(f)){ 
            filter_free(f);
            f = nullptr; 
        }
    }
    return f;
}


uint64_t filter_database_uri_hasher(const char * prfx,const char * filter_id,const char * state_id){
    return std::hash<std::string>{}(std::string(prfx)+filter_id+state_id);
}


filter_database_handler* filter_database_handler_lookup(filter* f,const char * sharedlib){
    struct stat st;
    auto res = (filter_database_handler*) 0;
    if(0==stat(sharedlib,&st)){
        if(auto dl = flow_dlopen(f->m->hdl,sharedlib,rtld_lazy|rtld_nodelete)){
            auto pointer_size= (size_of_pointer_t) kautil_dlsym(dl,"size_of_pointer");
            auto initialize= (lookup_table_initialize_t) kautil_dlsym(dl,"lookup_table_initialize");
            auto free_lookup_t= (lookup_table_free_t) kautil_dlsym(dl,"lookup_table_free");
            if(0== !pointer_size + !initialize + !free_lookup_t){
                res = new filter_database_handler{};{
                    auto lookup_tb = initialize();
                    auto ptr_size= pointer_size();
                    res->dl = dl;
                    res->initialize = (typeof(res->initialize))filter_lookup_table_get_value(lookup_tb, "initialize");
                    res->free =  (typeof(res->free)) filter_lookup_table_get_value(lookup_tb,"free");
                    
                    {
                        res->uri_hasher = (typeof(res->uri_hasher)) filter_lookup_table_get_value(lookup_tb,"uri_hasher");
                        res->uri_hasher = (typeof(res->uri_hasher)) 
                                (!uintptr_t(res->uri_hasher)*uintptr_t(filter_database_uri_hasher) 
                                 +uintptr_t(res->uri_hasher)); 
                    }
                    
                    res->set_index =  (typeof(res->set_index)) filter_lookup_table_get_value(lookup_tb,"set_index");
                    res->set_uri =  (typeof(res->set_uri)) filter_lookup_table_get_value(lookup_tb,"set_uri");
                    res->setup =  (typeof(res->setup)) filter_lookup_table_get_value(lookup_tb,"setup");
                    res->set_output =  (typeof(res->set_output)) filter_lookup_table_get_value(lookup_tb,"set_output");
                    res->set_input =  (typeof(res->set_input)) filter_lookup_table_get_value(lookup_tb,"set_input");
                    res->save =  (typeof(res->save)) filter_lookup_table_get_value(lookup_tb,"save");
                    res->sw_overwrite =  (typeof(res->sw_overwrite)) filter_lookup_table_get_value(lookup_tb,"sw_overwrite");
                    res->sw_without_rowid =  (typeof(res->sw_without_rowid)) filter_lookup_table_get_value(lookup_tb,"sw_without_rowid");
                    res->sw_uniformed =  (typeof(res->sw_uniformed)) filter_lookup_table_get_value(lookup_tb,"sw_uniformed");
                    res->sw_key_is_uniformed =  (typeof(res->sw_key_is_uniformed)) filter_lookup_table_get_value(lookup_tb,"sw_key_is_uniformed");
                    
                    free_lookup_t(lookup_tb);
                }
                return res;
            }
            kautil_dlclose(dl);
        }
    }
    return nullptr;
}


flow_lookup_table* _flow_push_with_lookup_table(flow * fhdl
                                          ,filter_lookup_table * (*filter_lookup_table_initialize)()
                                          ,void (*filter_lookup_table_free)(filter_lookup_table * f)){
    if(auto ltb = filter_lookup_table_initialize()){
        if(auto f  =flow_lookup_filter_functions(fhdl,ltb)){
            fhdl->lookup_releaser.emplace_back(new flow_lookup_table{
                .initialize_lookup_table = filter_lookup_table_initialize
                ,.free_lookup_table = filter_lookup_table_free
                ,.lookup_table = ltb
                ,.filter=f
            });
            if(f){
                f->state_reset(f);
                flow_push(fhdl,f);
                return fhdl->lookup_releaser.back();
            }
        }
    }
    return nullptr;
}

int flow_push_with_lookup_table(flow * fhdl
                                          ,filter_lookup_table * (*filter_lookup_table_initialize)()
                                          ,void (*filter_lookup_table_free)(filter_lookup_table * f)){
    return _flow_push_with_lookup_table(fhdl,filter_lookup_table_initialize,filter_lookup_table_free) ? 0:1;
}


int flow_execute(flow * fhdl){
    for(auto i = fhdl->start_offset; i < fhdl->filters.size(); ++i){
        auto f = fhdl->filters[i];
        f->main(f);
        f->save(f);
        
    }
    return 0;
}


void flow_set_local_uri(flow * hdl,const char * uri){ hdl->local_uri = uri; }
void flow_set_database(flow * hdl,const char * path_to_so,int op){ hdl->path_to_database=path_to_so; hdl->db_options=op; }
int flow_push(flow * fhdl,filter* f){
    f->m->pos=fhdl->filters.size();
    fhdl->filters.push_back(f);
    return 0;
}

struct filter_first_member{ void * o=0;uint64_t o_bytes=0; uint64_t o_block_bytes=0; uint64_t o_size=0; }__attribute__((aligned(8)));
void* filter_first_member_output(filter * f){ return reinterpret_cast<filter_first_member*>(f->fm)->o; }
uint64_t filter_first_member_size(filter * f){ return reinterpret_cast<filter_first_member*>(f->fm)->o_size; }
uint64_t filter_first_member_output_bytes(filter * f){ return reinterpret_cast<filter_first_member*>(f->fm)->o_bytes; }
bool filter_first_member_output_is_uniformed(filter * f){ return true; }
int _flow_push_input(flow * fhdl,void * data,uint32_t block_bytes,uint64_t nitems){
    
    fhdl->first_member = new filter_first_member{
        .o=data
        ,.o_bytes=block_bytes*nitems
        ,.o_block_bytes=block_bytes
        ,.o_size=nitems
    };
    
    auto f = filter_factory();{
        f->output = filter_first_member_output;
        f->output_bytes = filter_first_member_output_bytes;
        f->output_size = filter_first_member_size;
        f->output_is_uniformed = filter_first_member_output_is_uniformed;
        f->fm=fhdl->first_member;
    }
    fhdl->lookup_releaser.emplace_back(new flow_lookup_table{.filter=f});
    flow_push(fhdl,fhdl->lookup_releaser.back()->filter);
    return 0;
}



int _flow_set_input(flow * fhdl,void * data,uint32_t block_bytes,uint64_t nitems){
    if(!fhdl->first_member){
        flow_push_input(fhdl, nullptr,0,0);
    }
    if(fhdl->first_member){
        fhdl->first_member->o=data;
        fhdl->first_member->o_bytes=block_bytes*nitems;
        fhdl->first_member->o_block_bytes=block_bytes;
        fhdl->first_member->o_size=nitems;
        return 0;
    }
    return 1;
}


void filter_free(filter * f){
    if(f->m){
        auto db = f->m->db;
        if(db){
            for(auto & e : db->instance_map){
                if(e.second) db->free(e.second); 
            } 
        }
        delete f->m;
    }
    delete f;
}


void flow_free(flow * fhdl){ 
    
    // free filter and lookup_table. free for lookup_table depends dl  
    for(auto & e : fhdl->lookup_releaser){
        filter_free(e->filter);
        if(e->free_lookup_table) e->free_lookup_table(e->lookup_table);
    }
    for(auto & dl_elem : fhdl->dls) if(dl_elem.second)kautil_dlclose(dl_elem.second);
    for(auto & e : fhdl->lookup_releaser) delete e; 
    
    
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
    auto db =f->m->db; 
    if(0== !db + !f->save){
        auto instance = db->instance;
        
        auto in = (const char *) f->input(f);
        db->set_input(instance, in, f->input_bytes(f)/f->input_size(f),f->input_size(f));
        
        auto out = (const char *) f->output(f);
        db->set_output(instance, out, f->output_bytes(f)/f->output_size(f),f->output_size(f));
        
        auto index = f->index(f);
        db->set_index(instance, index);
    
        
        return db->save(instance);
    } 
    return 0;
}


bool filter_input_is_uniformed(filter * f){
    return f->m->hdl->filters[f->m->pos-1]->output_is_uniformed(f->m->hdl->filters[f->m->pos - 1]);
}
void* filter_input(filter * f) { return f->m->hdl->filters[f->m->pos-1]->output(f->m->hdl->filters[f->m->pos-1]); }
uint64_t filter_input_size(filter *f){ return f->m->hdl->filters[f->m->pos-1]->output_size(f->m->hdl->filters[f->m->pos-1]); }
uint64_t filter_input_bytes(filter * f) { return f->m->hdl->filters[f->m->pos-1]->output_bytes(f->m->hdl->filters[f->m->pos-1]); }



void * filter_lookup_table_get_value(filter_lookup_table * flookup_table,const char * key){
    // sorting is bad for this process. i neither want  not to change the order of the flookup_table nor to copy it 
    auto arr = reinterpret_cast<char **>(flookup_table);
    for(;;arr+=sizeof(filter_lookup_elem)/sizeof(uintptr_t)){
        if(nullptr==arr[0]) break;
        if(!strcmp(key,arr[0]))return reinterpret_cast<void*>(arr[1]);
    }
    return nullptr;
}



int filter_database_setup(filter * f) { 
    auto m = f->m;
    auto library = f->m->hdl->path_to_database.data();
    if(!m->db) m->db = filter_database_handler_lookup(f, library);
    auto res  =0;
    {
        auto state_hash = m->db->uri_hasher(m->hdl->local_uri.data(), f->id_hr(f),f->state_id(f));
        auto [itr,insert] = m->db->instance_map.insert({state_hash, nullptr});
        m->db->instance=itr->second;
        if(insert || f->database_close_always(f)){
            if(itr->second) m->db->free(itr->second);
            itr->second=m->db->initialize(m->db);
            m->db->instance=itr->second;
            
            m->db->sw_uniformed(m->db->instance,f->output_is_uniformed(f));
            m->db->sw_key_is_uniformed(m->db->instance,f->input_is_uniformed(f));
            
            filter_database_handler_set_option(m->db,m->option ? m->option : f->m->hdl->db_options);
            m->db->set_uri(m->db->instance, m->hdl->local_uri.data(), f->id_hr(f),f->state_id(f));
            m->db->last_option = m->option;
            res=m->db->setup(m->db->instance);
        }
    }
    return res;
}


int flow_push_input(flow * fhdl,void * data,uint64_t block_size,uint64_t nitems){
    // "_flow_push_input(fhdl,nullptr,0)" is because to omit a branch when accessing input value via output function 
    fhdl->start_offset=2;
    return 0 == _flow_push_input(fhdl,nullptr,0,0) + _flow_push_input(fhdl,data,block_size,nitems);
}


int flow_set_input(flow * fhdl,void * data,uint64_t block_size,uint64_t nitems){
    return _flow_set_input(fhdl,data,block_size,nitems);
}



int flow_push_with_library(flow * fhdl,const char * so_filter){
    if(auto dl = flow_dlopen(fhdl,so_filter,rtld_lazy|rtld_nodelete)){
        auto tb_init = (lookup_table_initialize_t) kautil_dlsym(dl,"lookup_table_initialize");
        auto sizeof_ptr = (size_of_pointer_t) kautil_dlsym(dl,"size_of_pointer");
        auto tb_free = (lookup_table_free_t) kautil_dlsym(dl,"lookup_table_free");
        
        if(fhdl->filters.empty())flow_push_input(fhdl,nullptr,0,0);
        if(auto insert = _flow_push_with_lookup_table(fhdl,tb_init,tb_free)){
            insert->dl = dl;
            return 0;
        } else kautil_dlclose(dl);
    } 
    return 1;
}



int flow_execute_all_state(flow * fhdl){
    while(true){
        uint32_t state=0;
        for(auto i = fhdl->start_offset; i < fhdl->filters.size(); ++i){
            auto f = fhdl->filters[i];
            //printf("%s ",f->state_id(f));
            filter_database_setup(f);
        }
        //printf("\n"); fflush(stdout);
        flow_execute(fhdl);
        
        for(auto i = fhdl->start_offset; i < fhdl->filters.size(); ++i){
            auto f = fhdl->filters[i];
            if(!f->state_next(f)){
                state+=1;
                f->state_reset(f);
                continue;
            }
            break;
        }
        if(state == fhdl->filters.size()-fhdl->start_offset) break;
    }
    return 0;
}


#include <numeric>


struct ig{
    void * data=0;
    uint64_t size=0;
};


int tmain_kautil_flow_static_tmp(const char * database_so,const char * so_filter){
    
    // miss : in database, io is treated as the same length  
    
    {
        remove("R:\\flow\\build\\android\\filter.arithmetic.subtract\\KautilFilterArithmeticSubtract.0.0.1\\4724af5.sqlite");

        auto fhdl = flow_initialize();
        flow_set_local_uri(fhdl,"./");
        flow_set_database(fhdl,database_so,FILTER_DATABASE_OPTION_OVERWRITE | FILTER_DATABASE_OPTION_WITHOUT_ROWID);
        {
            // it is loss to open database multiple times
            
            auto input_len = 10; 
            std::vector<double> arr(10);
            
            flow_push_with_library(fhdl,so_filter);
            flow_push_with_library(fhdl,so_filter);
            
            std::iota(arr.begin(),arr.end(),0);
            flow_set_input(fhdl,arr.data(), sizeof(decltype(arr)::value_type),arr.size());
            flow_execute_all_state(fhdl);
            
//            std::iota(arr.begin(),arr.end(),123);
//            flow_set_input(fhdl,arr.data(), sizeof(decltype(arr)::value_type),arr.size());
//            flow_execute_all_state(fhdl);
            
        }
        flow_free(fhdl);
    }
    
    
    return 0;
}




