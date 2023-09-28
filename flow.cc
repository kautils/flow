#include "./flow.h"
#include "stdint.h"
#include <vector>
#include <string>

#include "kautil/sqlite3/sqlite3.h"
#include "libgen.h"
#include "sys/stat.h"



filter_database_handler* filter_database_handler_initialize(filter * f);
struct filter_database_handler{
    filter_database_handler* (*alloc)(filter * f)=filter_database_handler_initialize;
    int (*set_uri)(void * hdl,const char * prfx,const char * id)=0;
    void (*free)(void * hdl)=0;
    int (*setup)(void * hdl)=0;
    int (*set_output)(void * hdl,const void * begin,const void * end)=0;
    int (*set_input)(void * hdl,const void * begin,const void * end)=0;
    int (*set_io_length)(void * hdl,uint64_t)=0;
    int (*save)(void * hdl)=0;
    int (*sw_overwrite)(void * hdl,bool)=0;
    int (*sw_without_rowid)(void * hdl,bool)=0;
    int type=0;
    void * instance=0;
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
    
};

struct filter_first_member;
struct filter_handler{ 
    filter * previous=0;
    filter * current=0;
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
        f->setup_database=filter_database_setup;
        f->save=filter_database_save;
        f->m= filter_lookup_table_get_value(flookup,"member");
        
        f->option=FILTER_DATABASE_OPTION_OVERWRITE | FILTER_DATABASE_OPTION_WITHOUT_ROWID;
        f->setup_database(f);
    }
    return f;
}

int filter_handler_push_with_lookup_table(filter_handler * fhdl
                                          ,filter_lookup_table * (*filter_lookup_table_initialize)()
                                          ,void (*filter_lookup_table_free)(filter_lookup_table * f)
                                          ){
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
            return 0;
        }
    }
    return 1;
}


int filter_handler_execute(filter_handler * fhdl){
    for(auto i = 1; i < fhdl->filters.size(); ++i){
        auto f = fhdl->filters[i];
        f->hdl->previous = f->hdl->current;
        f->hdl->current = f;
        f->main(f);
        f->save(f);
    }
    return 0;
}


void filter_handler_set_io_length(filter_handler * hdl,uint64_t len){ hdl->io_len=len; }
void filter_handler_set_local_uri(filter_handler * hdl,const char * uri){ hdl->local_uri = uri; }
int filter_handler_push(filter_handler * fhdl,filter* f){
    if(0==(fhdl->filters.empty())){
        fhdl->previous= nullptr;
        fhdl->current= f;
    }
    fhdl->filters.push_back(f);
    return 0;
}

struct filter_first_member{ void * o=0;uint64_t o_bytes=0; }__attribute__((aligned(8)));
void* filter_first_member_output(filter * f){ return reinterpret_cast<filter_first_member*>(f->m)->o; }
uint64_t filter_first_member_output_bytes(filter * f){ return reinterpret_cast<filter_first_member*>(f->m)->o_bytes; }
int filter_handler_input(filter_handler * fhdl,void * data,int32_t block_size){
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
        delete e;
    }
    delete fhdl->first_member;
    delete fhdl; 
}




void* filter_database_sqlite_initialize();
int filter_database_sqlite_set_uri(void * whdl,const char * prfx,const char * id);
void filter_database_sqlite_free(void* whdl);
int filter_database_sqlite_setup(void * whdl);
int filter_database_sqlite_set_output(void * whdl,const void * begin,const void * end);
int filter_database_sqlite_set_input(void * whdl,const void * begin,const void * end);
int filter_database_sqlite_set_io_length(void * whdl,uint64_t len);
int filter_database_sqlite_save(void * whdl);
int filter_database_sqlite_sw_overwrite(void * whdl,bool sw); 
int filter_database_sqlite_sw_without_rowid(void * whdl,bool sw); 


filter_database_handler* filter_database_handler_initialize(filter * f){
    if(f->database_type){
        auto res = new filter_database_handler{};
        switch(f->database_type(f)){
            case FILTER_DATABASE_TYPE_SQLITE3:{
                res->type = f->database_type(f);
                res->instance = filter_database_sqlite_initialize();
                res->set_uri = filter_database_sqlite_set_uri;
                res->set_output = filter_database_sqlite_set_output;
                res->set_input = filter_database_sqlite_set_input;
                res->set_io_length = filter_database_sqlite_set_io_length;
                res->setup = filter_database_sqlite_setup;
                res->save = filter_database_sqlite_save;
                res->sw_overwrite = filter_database_sqlite_sw_overwrite;
                res->sw_without_rowid = filter_database_sqlite_sw_without_rowid;
                
                res->free = filter_database_sqlite_free;
                return res;
            };
        }
    }// database_type
    return nullptr;
}


int filter_database_handler_set_option(filter_database_handler * hdl,int op){
    auto op_ow = op & FILTER_DATABASE_OPTION_OVERWRITE;
    auto op_norowid = op & FILTER_DATABASE_OPTION_WITHOUT_ROWID;
    if(hdl->sw_without_rowid)hdl->sw_without_rowid(hdl->instance,op_ow);
    if(hdl->sw_overwrite)hdl->sw_overwrite(hdl->instance,op_norowid);
    return 0;    
}

int filter_database_setup(filter * f) { 
    auto instance = (void * ) 0;
    if(!f->db) f->db = filter_database_handler_initialize(f); 
    else{
        f->db->free(f->db->instance);
        f->db=f->db->alloc(f);
    }
    instance = f->db->instance;
    
    filter_database_handler_set_option(f->db,f->option);
    f->db->set_io_length(instance, f->hdl->io_len);
    f->db->set_uri(instance, f->hdl->local_uri.data(), f->id_hr(f));

    f->db->last_option = f->option;
    return f->db->setup(instance); 
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



void* filter_input(filter * f) { 
    if(f->hdl->previous){
        return f->hdl->previous->output(f->hdl->previous);
    }
    return nullptr; 
}

uint64_t filter_input_bytes(filter * f) { 
    return f->hdl->previous->output_bytes(f->hdl->previous);
}



void * filter_lookup_table_get_value(filter_lookup_table * flookup_table,const char * key){
    // sorting is bad for this process. i neither want  not to change the order of the flookup_table nor to copy it 
    auto arr = reinterpret_cast<char **>(flookup_table);
    for(;;arr+=sizeof(filter_lookup_elem)/sizeof(uintptr_t)){
        if(nullptr==arr[0]) break;
        if(!strcmp(key,arr[0]))return reinterpret_cast<void*>(arr[1]);
    }
    return nullptr;
}




// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



struct io_data{ const void * begin;const void * end; };
struct filter_database_sqlite3_handler{
    kautil::database::Sqlite3Stmt * create;
    kautil::database::Sqlite3Stmt * insert;
    kautil::database::Sqlite3 * sql=0;
    kautil::database::sqlite3::Options * op=0;
    std::string uri_prfx;
    std::string id;

    io_data i;
    io_data o;
    uint64_t io_len=0;
    bool is_overwrite=false;
    bool is_without_rowid=false;
};



constexpr static const char * kCreateSt             = "create table if not exists m([rowid] interger primary key,[k] blob,[v] blob,unique([k])) ";
constexpr static const char * kCreateStWithoutRowid = "create table if not exists m([k] blob primary key,[v] blob) without rowid ";
static const char * kInsertSt = "insert or ignore into m(k,v) values(?,?)";
static const char * kInsertStOw = "insert or replace into m(k,v) values(?,?)";


filter_database_sqlite3_handler* get_instance(void * whdl){
    return reinterpret_cast<filter_database_sqlite3_handler*>(whdl);
}

void* filter_database_sqlite_initialize(){
    auto res = new filter_database_sqlite3_handler;
    res->op = kautil::database::sqlite3::sqlite_options(); 
    return res;
}

void filter_database_sqlite_free(void* whdl){
    auto m=get_instance(whdl);
    delete m->op;
    delete m->sql;
    delete m;
}


//int filter_database_sqlite_reset(void* whdl){ return 0; }

int filter_database_sqlite_set_uri(void * whdl,const char * prfx,const char * id){
    auto m=get_instance(whdl);
    m->uri_prfx = prfx; 
    m->id = id; 
    return 0;
}

int filter_database_sqlite_setup(void * whdl){
    auto m=get_instance(whdl);
    if(!m->sql) {
        auto path = m->uri_prfx +"/" + m->id + ".sqlite";
        {
            struct stat st;
            auto buf = std::string(path.data());
            auto dir = dirname(buf.data());
            if(stat(dir,&st)){
                if(mkdir(dir)){
                    printf("fail");
                    return 1;
                }
            } 
        }
        m->op = kautil::database::sqlite3::sqlite_options(); 
        m->sql = new kautil::database::Sqlite3{path.data(),m->op->sqlite_open_create()|m->op->sqlite_open_readwrite()|m->op->sqlite_open_nomutex()};
        m->create = m->sql->compile(m->is_without_rowid ? kCreateStWithoutRowid : kCreateSt);
        if(m->create){
            auto res_crt = m->create->step(true);
            res_crt |= ((m->create->step(true) == m->op->sqlite_ok()));
            if(res_crt){
                return !bool(m->insert = m->sql->compile(m->is_overwrite ? kInsertStOw : kInsertSt));
            }
        }
    }
    m->sql->error_msg();
    delete m->op;
    delete m->sql;
    m->op=nullptr;
    m->sql=nullptr;
    
    return 1;
}


int filter_database_sqlite_set_output(void * whdl,const void * begin,const void * end){
    auto m=get_instance(whdl);
    m->o.begin = begin;
    m->o.end = end;
    return m->o.begin < m->o.end; 
}

int filter_database_sqlite_set_input(void * whdl,const void * begin,const void * end){
    auto m=get_instance(whdl);
    m->i.begin = begin;
    m->i.end = end;
    return m->i.begin < m->i.end; 
}

int filter_database_sqlite_set_io_length(void * whdl,uint64_t len){
    auto m=get_instance(whdl);
    m->io_len= len;
    return 0;
}

int filter_database_sqlite_sw_overwrite(void * whdl,bool sw){
    auto m=get_instance(whdl);
    m->is_overwrite=sw;
    return 0;
}

int filter_database_sqlite_sw_without_rowid(void * whdl,bool sw){
    auto m=get_instance(whdl);
    m->is_without_rowid=sw;
    return 0;
}

int filter_database_sqlite_save(void * whdl){
    auto m=get_instance(whdl);
    if(auto begin_i = reinterpret_cast<const char*>(m->i.begin)){
        auto end_i = reinterpret_cast<const char*>(m->i.end);
        auto block_i = (end_i - begin_i) / m->io_len;
        if(auto begin_o = reinterpret_cast<const char*>(m->o.begin)){
            auto end_o = reinterpret_cast<const char*>(m->o.end);
            auto block_o = (end_o - begin_o) / m->io_len;
            auto fail = false;
            if(begin_i < begin_o){
                m->sql->begin_transaction();
                for(;begin_i != end_i; begin_i+=block_i,begin_o+=block_o){
                    auto res_stmt = !m->insert->set_blob(1,begin_i,block_i);
                    res_stmt |= !m->insert->set_blob(2,begin_o,block_o);
                    
                    auto res_step = m->insert->step(true);
                    res_step |= res_step == m->op->sqlite_ok();
                    
                    if((fail=res_stmt+!res_step))break;
                }
                if(fail) m->sql->roll_back();
                m->sql->end_transaction();
                return !fail;
            }
        }
    }else m->sql->error_msg();
    return 1;
}



struct filter_lookup_table_database_sqlite{
    filter_lookup_elem initialize{.key="initialize",.value=(void*)filter_database_sqlite_initialize};
    filter_lookup_elem free{.key="free",.value=(void*)filter_database_sqlite_free};
    filter_lookup_elem set_uri{.key="set_uri",.value=(void*)filter_database_sqlite_set_uri};
    filter_lookup_elem setup{.key="setup",.value=(void*)filter_database_sqlite_setup};
    filter_lookup_elem set_output{.key="set_output",.value=(void*)filter_database_sqlite_set_output};
    filter_lookup_elem set_input{.key="set_input",.value=(void*)filter_database_sqlite_set_input};
    filter_lookup_elem set_io_length{.key="set_io_length",.value=(void*)filter_database_sqlite_set_io_length};
    filter_lookup_elem sw_overwrite{.key="sw_overwrite",.value=(void*)filter_database_sqlite_sw_overwrite};
    filter_lookup_elem sw_rowid{.key="sw_without_rowid",.value=(void*)filter_database_sqlite_sw_without_rowid};
    filter_lookup_elem save{.key="save",.value=(void*)filter_database_sqlite_save};
    filter_lookup_elem member{.key="member",.value=(void*)filter_database_sqlite_initialize()};
    filter_lookup_elem sentinel{.key=nullptr,.value=nullptr};
} __attribute__((aligned(sizeof(uintptr_t))));


extern "C" uint64_t __lookup_tb_pointer_size(){ return sizeof(uintptr_t);}
extern "C" filter_lookup_table * __lookup_tb_initialize(){ 
    auto res= new filter_lookup_table_database_sqlite{}; 
    return reinterpret_cast<filter_lookup_table*>(res);
}
extern "C" void __lookup_tb_free(filter_lookup_table * f){
    auto entity = reinterpret_cast<filter_lookup_table_database_sqlite*>(f);
    delete reinterpret_cast<filter_database_sqlite3_handler*>(entity->member.value);
    delete entity; 
}




int tmain_kautil_flow_static(){
    
    printf("AAAAAAAAAAA\n");
    
    return 0;
}


