

cmake_minimum_required(VERSION 3.20)
set(__version 0.0.1)
project(KautilFlow.${__version}
        LANGUAGES C CXX
        VERSION ${__version}
        )
unset(__version)


CMakeSyncLocalRepositoryWithSymLink(
    LOCAL "D:/git.local/local"
    REPOS filter.arithmetic.subtract flow.db.sqlite3
    DESTINATION ${CMAKE_BINARY_DIR}/test
)

include(${CMAKE_CURRENT_LIST_DIR}/flow/flow.cmake)





