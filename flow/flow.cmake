set(${PROJECT_NAME}_m_evacu ${m})
set(m ${PROJECT_NAME})
list(APPEND ${m}_unsetter )

set(${m}_kautil_cmake_heeder CMakeKautilHeader.v0.0.cmake)
if(DEFINED KAUTIL_THIRD_PARTY_DIR AND EXISTS "${KAUTIL_THIRD_PARTY_DIR}/${${m}_kautil_cmake_heeder}")
    include("${KAUTIL_THIRD_PARTY_DIR}/${${m}_kautil_cmake_heeder}")
else()
    if(NOT EXISTS ${CMAKE_BINARY_DIR}/${${m}_kautil_cmake_heeder})
        file(DOWNLOAD https://raw.githubusercontent.com/kautils/CMakeKautilHeader/v0.0/CMakeKautilHeader.cmake ${CMAKE_BINARY_DIR}/${${m}_kautil_cmake_heeder})
    endif()
    include(${CMAKE_BINARY_DIR}/${${m}_kautil_cmake_heeder})
endif()
git_clone(https://raw.githubusercontent.com/kautils/CMakeLibrarytemplate/v0.0/CMakeLibrarytemplate.cmake)
git_clone(https://raw.githubusercontent.com/kautils/CMakeFetchKautilModule/v1.0/CMakeFetchKautilModule.cmake)


list(APPEND ${m}_unsetter ${m}_sharedlib_ver ${m}_cache_ver)
set(${m}_sharedlib_ver v0.0)
set(${m}_cache_ver v2.0)
CMakeFetchKautilModule(${m}_sharedlib
        GIT https://github.com/kautils/sharedlib.git
        REMOTE origin
        BRANCH ${${m}_sharedlib_ver}
        PROJECT_SUFFIX ${${m}_sharedlib_ver} 
        CMAKE_CONFIGURE_MACRO -DCMAKE_CXX_FLAGS="-O2" -DCMAKE_CXX_STANDARD=23
        CMAKE_BUILD_OPTION -j ${${m}_thread_cnt}
        )
find_package(KautilSharedLib.${${m}_sharedlib_ver}.static REQUIRED)

CMakeFetchKautilModule(${m}_kautil_cache 
        GIT https://github.com/kautils/cache.git 
        REMOTE origin 
        BRANCH ${${m}_cache_ver} 
        PROJECT_SUFFIX ${${m}_cache_ver})

find_package(KautilCache.${${m}_cache_ver}.interface REQUIRED)


set(module_name flow)
unset(srcs)
file(GLOB srcs ${CMAKE_CURRENT_LIST_DIR}/*.cc)
set(${module_name}_common_pref
    MODULE_PREFIX kautil
    MODULE_NAME ${module_name}
    INCLUDES $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}> $<INSTALL_INTERFACE:include> ${CMAKE_CURRENT_LIST_DIR} 
    SOURCES ${srcs}
    LINK_LIBS 
        kautil::sharedlib::${${m}_sharedlib_ver}::static
        kautil::cache::${${m}_cache_ver}::interface
        
    EXPORT_NAME_PREFIX ${PROJECT_NAME}
    EXPORT_VERSION ${PROJECT_VERSION}
    EXPORT_VERSION_COMPATIBILITY AnyNewerVersion
        
    DESTINATION_INCLUDE_DIR include/kautil/flow
    DESTINATION_CMAKE_DIR cmake
    DESTINATION_LIB_DIR lib
)

CMakeLibraryTemplate(${module_name} EXPORT_LIB_TYPE static ${${module_name}_common_pref} )
CMakeLibraryTemplate(${module_name} EXPORT_LIB_TYPE shared ${${module_name}_common_pref} )


set(__t ${${module_name}_static_tmain}_tmp)
add_executable(${__t})
target_sources(${__t} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/unit_test.cc)
target_link_libraries(${__t} PRIVATE ${${module_name}_static})

target_compile_definitions(${__t} PRIVATE ${${module_name}_static_tmain_ppcs}_TMP)
target_compile_definitions(${__t} PRIVATE SO_FILE_DB="$<TARGET_FILE:kautil_flow_db_sqlite3_0.0.1_shared>")
target_compile_definitions(${__t} PRIVATE SO_FILE_FILTER="$<TARGET_FILE:kautil_filter_example_0.0.1_shared>")

add_dependencies(${__t} kautil_flow_db_sqlite3_0.0.1_shared)
add_dependencies(${__t} kautil_filter_example_0.0.1_shared)

foreach(__v ${${m}_unsetter})
    unset(${__v})
endforeach()
unset(${m}_unsetter)
set(m ${${PROJECT_NAME}_m_evacu})

