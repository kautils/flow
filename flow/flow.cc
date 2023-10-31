#include "./flow.h"
#include "stdint.h"

#include <vector>
#include <string>

#include "libgen.h"
#include "sys/stat.h"
#include "kautil/sharedlib/sharedlib.h"



//#include "kautil/cache/cache.hpp"
#include "unistd.h"
#include "fcntl.h"



int mkdir_recurse(char * p){
    
    auto c = p;
    struct stat st;

    if(0==stat(p,&st)){
        return !S_ISDIR(st.st_mode);
    } 
    auto b = true;
    for(;b;++c){
        if(*c == '\\' || *c=='/'){
            ++c;
            auto evacu = *c;
            *c = 0;
            if(stat(p,&st)){
                if(mkdir(p)) b = false;
            }
            *c = evacu;
        }
        if(0==!b+!*c) continue;
        if(stat(p,&st)){
            if(mkdir(p)) b = false;
        }
        break;
    }
    return !b;
}





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
void * filter_lookup_table_get_value(filter_lookup_table * flookup_table,const char * key);
bool filter_input_is_uniformed(filter * f);
void * filter_input_high(filter * f);
void * filter_input_low(filter * f);
void* filter_input(filter * f);
uint64_t filter_input_bytes(filter * f);
uint64_t filter_input_size(filter *f);

int flow_database_save(filter * f);
void filter_free(filter * f);

struct filter_lookup_elem{
    const char * key = 0; 
    void * value = nullptr;
}__attribute__((aligned(sizeof(uintptr_t))));



#include <map> 
struct filter_database_handler{
    filter_database_handler* (*alloc)(filter * ,const char *)=filter_database_handler_lookup;
    void* (*initialize)()=0;
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


bool filter_database_close_always(void * f){ return false; }
bool filter_is_uniformed(void * f){ return true; }
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
        f->output=(decltype(f->output))filter_lookup_table_get_value(flookup,"output");
        f->output_bytes= (decltype(f->output_bytes))filter_lookup_table_get_value(flookup,"output_bytes");
        f->output_size= (decltype(f->output_size))filter_lookup_table_get_value(flookup,"output_size");
        f->output_high= (decltype(f->output_high))filter_lookup_table_get_value(flookup,"output_high");
        f->output_low= (decltype(f->output_low))filter_lookup_table_get_value(flookup,"output_low");
        
        f->set_input=(decltype(f->set_input))filter_lookup_table_get_value(flookup,"set_input");
        f->index= (decltype(f->index))filter_lookup_table_get_value(flookup,"index");
        f->index = (decltype(f->index)) 
                (!uintptr_t(f->index)*uintptr_t(filter_index) 
                 +uintptr_t(f->index));
//        f->input = filter_input;
//        f->input_bytes = filter_input_bytes;
//        f->input_size = filter_input_size;
//        f->input_is_uniformed = filter_input_is_uniformed; 
        
        f->id= (decltype(f->id))filter_lookup_table_get_value(flookup,"id");
        f->id_hr= (decltype(f->id_hr))filter_lookup_table_get_value(flookup,"id_hr");
        f->state_next= (decltype(f->state_next))filter_lookup_table_get_value(flookup,"state_next");
        f->state_reset= (decltype(f->state_reset))filter_lookup_table_get_value(flookup,"state_reset");
        f->state_id= (decltype(f->state_id))filter_lookup_table_get_value(flookup,"state_id");
//        f->save=flow_database_save;
        {
            f->output_is_uniformed= (decltype(f->output_is_uniformed))filter_lookup_table_get_value(flookup, "output_is_uniformed");
            f->output_is_uniformed = (decltype(f->output_is_uniformed)) 
                      (uintptr_t(!f->output_is_uniformed) * uintptr_t(filter_is_uniformed)
                    + (uintptr_t(f->output_is_uniformed))); 
        }

        {
            f->database_close_always=(decltype(f->database_close_always))filter_lookup_table_get_value(flookup, "database_close_always");
            f->database_close_always=(decltype(f->database_close_always)) 
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
                f->state_reset(f->fm);
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

template<typename element_type>
struct file_syscall_double_pref{
    using value_type = element_type;
    using offset_type = long;
    
    int fd=-1;
    char * buffer = 0;
    offset_type buffer_size = 0;
    struct stat st;
    
    
    ~file_syscall_double_pref(){ free(buffer); }
    offset_type block_size(){ return sizeof(value_type)*2; }
    offset_type size(){ fstat(fd,&st);return static_cast<offset_type>(st.st_size); }
    
    void read_value(offset_type const& offset, value_type ** value){
        lseek(fd,offset,SEEK_SET);
        ::read(fd,*value,sizeof(value_type));
    }
    
    bool write(offset_type const& offset, void ** data, offset_type size){
        lseek(fd,offset,SEEK_SET);
        return size==::write(fd,*data,size);
    }
    
    // api may make a confusion but correct. this is because to avoid copy of value(object such as bigint) for future.
    bool read(offset_type const& from, void ** data, offset_type size){
        lseek(fd,from,SEEK_SET);
        return size==::read(fd,*data,size);
    }
    
    int extend(offset_type extend_size){ 
        fstat(fd,&st); 
        return ftruncate(fd,st.st_size+extend_size);
    }
    int shift(offset_type dst,offset_type src,offset_type size){
        if(buffer_size < size){
            if(buffer)free(buffer);
            buffer = (char*) malloc(buffer_size = size);
        }
        lseek(fd,src,SEEK_SET);
        auto read_size = ::read(fd,buffer,size);
        lseek(fd,dst,SEEK_SET);
        ::write(fd,buffer,read_size);
        return 0;
    }
    
    int flush_buffer(){ return 0; }
};



template<typename value_type,typename offset_type>
void debug_out_file(FILE* outto,int fd,offset_type from,offset_type to){
    struct stat st;
    fstat(fd,&st);
    auto cnt = 0;
    lseek(fd,0,SEEK_SET);
    auto start = from;
    auto size = st.st_size;
    value_type block[2];
    for(auto i = 0; i< size; i+=(sizeof(value_type)*2)){
        if(from <= i && i<= to ){
            lseek(fd,i,SEEK_SET);
            ::read(fd,&block, sizeof(value_type) * 2);
            printf("[%ld] %lf %lf\n",i,block[0],block[1]);fflush(outto);
        }
    }
}


void flow_cache_dump_file(int fd){
    debug_out_file<double,long>(stdout,fd,0,20);
    exit(0);
}

enum cache_primitive_type_id{
    kDouble=0
};

//#define m(ptr) reinterpret_cast<cache_primitive_type_handler*>(ptr) 
struct cache_primitive_type_handler{
    std::string path;
    cache_primitive_type_id type_id;
    void * obj=0;
    void * pref=0;
    int fd = -1;
};


cache_primitive_type_handler* get_instance(void * ptr){ return reinterpret_cast<cache_primitive_type_handler*>(ptr); }
int cache_primitive_type_set_uri(void * hdl,const char * prfx,const char * filter_id,const char * state_id){
    auto m = get_instance(hdl);
    if(prfx) (m->path = prfx);
    else m->path = "./filter_cache_primitive_type/";
    
    if(0== bool(filter_id) + bool(state_id)){
        m->path = "/not_specified";
    }else{
        if(filter_id)m->path.append("/").append(filter_id);
        if(state_id)m->path.append("/").append(state_id);
    }
    m->path.append(".cache");
    return 0;
}

//int cache_primitive_type_setup(void * hdl,cache_primitive_type_id const& type){
//    auto m = get_instance(hdl);
//    {
//        struct stat st;
//        auto cache_path = m->path.data();
//        if(stat(cache_path,&st)){
//            m->fd = ::open(cache_path,O_CREAT|O_BINARY|O_EXCL|O_RDWR,0755);
//        }else{
//            m->fd = ::open(cache_path,O_RDWR|O_BINARY);
//        }
//    }
//    m->type_id = type;
//    auto pref =new file_syscall_double_pref<double>{.fd=m->fd}; 
//    m->pref = pref;
//    m->obj = new kautil::cache{pref};
//    
////    using file_16_struct_type = file_syscall_double_pref<double>; 
////    auto pref = file_16_struct_type{.fd=m->fd};
////    auto a = kautil::cache{&pref};
//    //auto res= a.merge(reinterpret_cast<double*>(filter_input_high(f)),reinterpret_cast<double*>(filter_input_low(f)));
//    return 0;
//}

//template<typename primitive_type>
//kautil::cache<file_syscall_double_pref<primitive_type>>* to_cache_object(void * cache_obj){
//    return reinterpret_cast<kautil::cache<file_syscall_double_pref<primitive_type>>*>(cache_obj);
//}
//
//template<typename primitive_type>
//typename kautil::cache<file_syscall_double_pref<primitive_type>>::gap_context* to_cache_gap_context_object(void * cache_obj){
//    return reinterpret_cast<typename kautil::cache<file_syscall_double_pref<primitive_type>>::gap_context*>(cache_obj);
//}
//
//bool cache_primitive_type_merge(void * hdl,void * begin,void * end){ 
//    auto m = get_instance(hdl);
//    switch (m->type_id) {
//        case kDouble:{
//            return to_cache_object<double>(m->obj)->merge(reinterpret_cast<double*>(begin),reinterpret_cast<double*>(end));
//        }
//    };
//    return false;
//}
//
//bool cache_primitive_type_exists(void * hdl,void * begin,void * end){
//    auto m = get_instance(hdl);
//    switch (m->type_id) {
//        case kDouble: return to_cache_object<double>(m->obj)->exists(reinterpret_cast<double*>(begin),reinterpret_cast<double*>(end));
//    };
//    return false;
//}

//
//struct flow_cache_gap_context{};
//flow_cache_gap_context * cache_primitive_type_gap_context(void * hdl,void * begin,void * end){
//    auto m = get_instance(hdl);
//    switch (m->type_id) {
//        case kDouble:return (flow_cache_gap_context*) to_cache_object<double>(m->obj)->gap(reinterpret_cast<double*>(begin),reinterpret_cast<double*>(end));
//    };
//    return nullptr;
//}
//
//void cache_primitive_type_gap_context_free(void * hdl,flow_cache_gap_context* ctx){
//    auto m = get_instance(hdl);
//    switch (m->type_id) {
//        case kDouble:to_cache_object<double>(m->obj)->gap_context_free(to_cache_gap_context_object<double>(ctx));
//    };
//    
//}
//void *cache_primitive_type_initialize(){ return new cache_primitive_type_handler; }
//void cache_primitive_type_free(void * hdl){ delete get_instance(hdl); }


//int flow_cache_save(filter * f){
//    
//    auto m = f->m;
//    auto cache = cache_primitive_type_initialize();
//    cache_primitive_type_set_uri(cache,m->hdl->local_uri.data(), f->id_hr(f->fm),f->state_id(f->fm));
//    cache_primitive_type_setup(cache,cache_primitive_type_id::kDouble);
//    
//    double from = 0;
//    double to = 9;
//    cache_primitive_type_merge(cache,&from,&to);
//    if(cache_primitive_type_exists(cache,&from,&to)){
//        printf("exists\n"); fflush(stdout);
//    }else{
//        printf("wrong\n"); fflush(stdout);
//    }
//    auto ctx = cache_primitive_type_gap_context(cache,&from,&to);
//    
//    
//    
//    
//    cache_primitive_type_gap_context_free(cache,ctx);
//    cache_primitive_type_free(cache);
//    flow_cache_dump_file(get_instance(cache)->fd);
//    return 0;
//}



int flow_execute(flow * fhdl){
    for(auto i = fhdl->start_offset; i < fhdl->filters.size(); ++i){
        auto f = fhdl->filters[i];
        
        f->set_input(f->fm,filter_input(f),filter_input_bytes(f)/filter_input_size(f),filter_input_size(f));
        f->main(f->fm);
        
        if(0==flow_database_save(f)){
//            flow_cache_save(f);
            // todo : specify diff
            exit(0);

            
        }
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



struct filter_first_member{ 
    void * o=0;
    uint64_t o_bytes=0;
    uint64_t o_block_bytes=0;
    uint64_t o_size=0;
    void * high = 0;
    void * low = 0;
}__attribute__((aligned(8)));


void* filter_first_member_high(void * m){ return reinterpret_cast<filter_first_member*>(m)->high; }
void* filter_first_member_low(void * m){ return reinterpret_cast<filter_first_member*>(m)->low; }
void* filter_first_member_output(void * m){ return reinterpret_cast<filter_first_member*>(m)->o; }
uint64_t filter_first_member_size(void * f){ return reinterpret_cast<filter_first_member*>(f)->o_size; }
uint64_t filter_first_member_output_bytes(void * f){ return reinterpret_cast<filter_first_member*>(f)->o_bytes; }
bool filter_first_member_output_is_uniformed(void * f){ return true; }


int _flow_push_input(flow * fhdl,void * data,uint32_t block_bytes,uint64_t nitems){
    
    if(fhdl->first_member)delete fhdl->first_member;
    fhdl->first_member = new filter_first_member{
        .o=data
        ,.o_bytes=block_bytes*nitems
        ,.o_block_bytes=block_bytes
        ,.o_size=nitems
    };
    
    auto f = filter_factory();{
        f->output = filter_first_member_output;
        f->output_bytes = filter_first_member_output_bytes;
        f->output_high = filter_first_member_high;
        f->output_low = filter_first_member_low;
        
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
                if(e.second){
                    db->free(e.second); 
                }
            } 
            delete db;
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

int flow_database_save(filter * f){
    auto db =f->m->db; 
    if(db ){
        auto instance = db->instance;
        
//        auto in = (const char *) f->input(f);
        auto in = (const char *) filter_input(f);
//        db->set_input(instance, in, f->input_bytes(f)/f->input_size(f),f->input_size(f));
        db->set_input(instance, in, filter_input_bytes(f)/filter_input_size(f),filter_input_size(f));
        
        auto out = (const char *) f->output(f->fm);
        db->set_output(instance, out, f->output_bytes(f->fm)/f->output_size(f->fm),f->output_size(f->fm));
        
        auto index = f->index(f->fm);
        db->set_index(instance, index);
    
        
        return db->save(instance);
    } 
    return 0;
}



void * filter_input_high(filter * f){ return f->m->hdl->filters[f->m->pos-1]->output_high(f->m->hdl->filters[f->m->pos - 1]->fm); }
void * filter_input_low(filter * f){ return f->m->hdl->filters[f->m->pos-1]->output_low(f->m->hdl->filters[f->m->pos - 1]->fm); }
bool filter_input_is_uniformed(filter * f){ return f->m->hdl->filters[f->m->pos-1]->output_is_uniformed(f->m->hdl->filters[f->m->pos - 1]->fm); }
void* filter_input(filter * f) { return f->m->hdl->filters[f->m->pos-1]->output(f->m->hdl->filters[f->m->pos-1]->fm); }
uint64_t filter_input_size(filter *f){ return f->m->hdl->filters[f->m->pos-1]->output_size(f->m->hdl->filters[f->m->pos-1]->fm); }
uint64_t filter_input_bytes(filter * f) { return f->m->hdl->filters[f->m->pos-1]->output_bytes(f->m->hdl->filters[f->m->pos-1]->fm); }



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
        auto state_hash = m->db->uri_hasher(m->hdl->local_uri.data(), f->id_hr(f->fm),f->state_id(f->fm));
        auto [itr,insert] = m->db->instance_map.insert({state_hash, nullptr});
        m->db->instance=itr->second;
        

        
        if(insert || f->database_close_always(f->fm)){
            if(itr->second){
                m->db->free(itr->second);
            } 
            itr->second=m->db->initialize(/*m->db*/);
            m->db->instance=itr->second;
            
            m->db->sw_uniformed(m->db->instance,f->output_is_uniformed(f->fm));
//            m->db->sw_key_is_uniformed(m->db->instance,f->input_is_uniformed(f));
            m->db->sw_key_is_uniformed(m->db->instance,filter_input_is_uniformed(f));
            
            filter_database_handler_set_option(m->db,m->option ? m->option : f->m->hdl->db_options);
            m->db->set_uri(m->db->instance, m->hdl->local_uri.data(), f->id_hr(f->fm),f->state_id(f->fm));
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
int flow_set_input_high(flow * fhdl,void * value){ fhdl->first_member->high = value;return 0; }
int flow_set_input_low(flow * fhdl,void * value){ fhdl->first_member->low = value;return 0; }

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
            if(!f->state_next(f->fm)){
                state+=1;
                f->state_reset(f->fm);
                continue;
            }
            break;
        }
        if(state == fhdl->filters.size()-fhdl->start_offset) break;
    }
    return 0;
}


#include <numeric>


int tmain_kautil_flow_static_tmp(const char * database_so,const char * so_filter){
    
    {
        
        remove("R:\\flow\\build\\android\\filter.arithmetic.subtract\\KautilFilterArithmeticSubtract.0.0.1\\4724af5.sqlite");

        auto fhdl = flow_initialize();
        flow_set_local_uri(fhdl,"./");
        flow_set_database(fhdl,database_so,FILTER_DATABASE_OPTION_OVERWRITE | FILTER_DATABASE_OPTION_WITHOUT_ROWID);
        {
            auto input_len = 10; 
            std::vector<double> arr(10);
            
            flow_push_with_library(fhdl,so_filter);
            flow_push_with_library(fhdl,so_filter);
            
            std::iota(arr.begin(),arr.end(),0);
            
            flow_set_input_high(fhdl,&arr.front());
            flow_set_input_low(fhdl,&arr.back());
            //flow_set_input(fhdl,arr.data(), sizeof(decltype(arr)::value_type),arr.size(),[](auto l, auto r){ return l <r; });
            flow_set_input(fhdl,arr.data(), sizeof(decltype(arr)::value_type),arr.size());
            flow_execute_all_state(fhdl);
            
            std::iota(arr.begin(),arr.end(),123);
            flow_set_input(fhdl,arr.data(), sizeof(decltype(arr)::value_type),arr.size());
            flow_execute_all_state(fhdl);
            
        }
        flow_free(fhdl);
    }
    
    
    return 0;
}




