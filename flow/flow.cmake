

if(NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeKautilHeader.cmake)
    file(DOWNLOAD https://raw.githubusercontent.com/kautils/CMakeKautilHeader/v0.0.1/CMakeKautilHeader.cmake ${CMAKE_BINARY_DIR}/CMakeKautilHeader.cmake)
endif()
include(${CMAKE_BINARY_DIR}/CMakeKautilHeader.cmake)
git_clone(https://raw.githubusercontent.com/kautils/CMakeLibrarytemplate/v0.0.1/CMakeLibrarytemplate.cmake)
git_clone(https://raw.githubusercontent.com/kautils/CMakeFetchKautilModule/v0.0.1/CMakeFetchKautilModule.cmake)


CMakeFetchKautilModule(sharedlib
        GIT https://github.com/kautils/sharedlib.git
        REMOTE origin
        TAG v0.0.1
        CMAKE_CONFIGURE_MACRO -DCMAKE_CXX_FLAGS="-O2" -DCMAKE_CXX_STANDARD=23
        CMAKE_BUILD_OPTION -j ${${m}_thread_cnt}
        )
find_package(KautilSharedLib.0.0.1.static REQUIRED)


set(module_name flow)
unset(srcs)
file(GLOB srcs ${CMAKE_CURRENT_LIST_DIR}/*.cc)
set(${module_name}_common_pref
    MODULE_PREFIX kautil
    MODULE_NAME ${module_name}
    INCLUDES $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}> $<INSTALL_INTERFACE:include> ${CMAKE_CURRENT_LIST_DIR} 
    SOURCES ${srcs}
    LINK_LIBS  kautil::sharedlib::0.0.1::static 
    EXPORT_NAME_PREFIX ${PROJECT_NAME}
    EXPORT_VERSION ${PROJECT_VERSION}
    EXPORT_VERSION_COMPATIBILITY AnyNewerVersion
        
    DESTINATION_INCLUDE_DIR include/kautil/flow
    DESTINATION_CMAKE_DIR cmake
    DESTINATION_LIB_DIR lib
)

CMakeLibraryTemplate(${module_name} EXPORT_LIB_TYPE static ${${module_name}_common_pref} )
CMakeLibraryTemplate(${module_name} EXPORT_LIB_TYPE shared ${${module_name}_common_pref} )



set(__t ${${module_name}_static_tmain})
add_executable(${__t})
target_sources(${__t} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/unit_test.cc)
target_link_libraries(${__t} PRIVATE ${${module_name}_static})
target_compile_definitions(${__t} PRIVATE ${${module_name}_static_tmain_ppcs})
target_compile_definitions(${__t} PRIVATE SO_FILE="$<TARGET_FILE:kautil_flow_db_sqlite3_0.0.1_shared>")
add_dependencies(${__t} kautil_flow_db_sqlite3_0.0.1_shared)

set(__t ${${module_name}_static_tmain}_tmp)
add_executable(${__t})
target_sources(${__t} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/unit_test.cc)
target_link_libraries(${__t} PRIVATE ${${module_name}_static})
target_compile_definitions(${__t} PRIVATE ${${module_name}_static_tmain_ppcs}_TMP)
target_compile_definitions(${__t} PRIVATE SO_FILE_DB="$<TARGET_FILE:kautil_flow_db_sqlite3_0.0.1_shared>")
target_compile_definitions(${__t} PRIVATE SO_FILE_FILTER="$<TARGET_FILE:kautil_filter_althmetic_subtract_0.0.1_shared>")
add_dependencies(${__t} kautil_flow_db_sqlite3_0.0.1_shared)
add_dependencies(${__t} kautil_filter_althmetic_subtract_0.0.1_shared)
