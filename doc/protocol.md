### lookup table :
  * element size : sizeof(uintptr_t)*2
  * nullend
```c++
struct some_table_element{
    const char * key;
    void * value;
} __attribute__((aligned(sizeof(uintptr_t))));
struct some_table{
    some_table_element func0={.key="func0",.value=(void*)pointer_of_func0};
    some_table_element func1={.key="func1",.value=(void*)pointer_of_func1};
    ...
    some_table_element funcN={.key="funcN",.value=(void*)pointer_of_funcN};
    some_table_element sentinel{.key=0,value=0};
};
```

### non_uniformed / uniformed
  * if a filter is defined as niformed, the outputs are treated as continuous array internally
  * if a filter is defined as non-niformed, the outputs are treated array of 16 bites element
    * they treated as like below programatically.
    ```c++
    ...
    auto elem =reinter_pretcast<uint64_t*>(output_elem);
    auto data  =elem[0];
    auto size  =elem[1];
    some_db_write_function(data,size);
    ...
    ```
