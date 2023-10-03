### kautil_flow
* manage the chain of calculations.
* using shared library.
* for backward compatibility, using selfdefined function table and fixing the function names in it.  
* possible to support multiple kind of database via shared library.


### peseudo example (database and filter is another repo)
```c++
int main(){
    
    const char * database_so = "path_to_shared_library_for_database_module";
    const char * so_filter = "path_to_shared_library_for_filters";
    {
        auto fhdl = flow_initialize();
        flow_set_local_uri(fhdl,"./");
        flow_set_database(fhdl,database_so,FILTER_DATABASE_OPTION_OVERWRITE | FILTER_DATABASE_OPTION_WITHOUT_ROWID);
        {
            auto input_len = 10; 
            std::vector<double> arr(10);
            
            flow_push_with_library(fhdl,so_filter);
            flow_push_with_library(fhdl,so_filter);
            
            std::iota(arr.begin(),arr.end(),0);
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
```

