
#ifdef TMAIN_KAUTIL_FLOW_STATIC

int tmain_kautil_flow_static(const char *);
int main(){
    return tmain_kautil_flow_static(SO_FILE);
}

#elif defined(TMAIN_KAUTIL_FLOW_STATIC_TMP) 

int tmain_kautil_flow_static_tmp(const char *,const char *);
int main(){
    return tmain_kautil_flow_static_tmp(SO_FILE_DB,SO_FILE_FILTER);
}

#endif