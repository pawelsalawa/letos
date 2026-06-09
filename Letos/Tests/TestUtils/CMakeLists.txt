find_package(Qt6 REQUIRED COMPONENTS Core Test)

set(PROJECT_HEADERS
    collationmanagermock.h
    configmock.h
    dbattachermock.h
    dbmanagermock.h
    dbsqlite3mock.h
    extensionmanagermock.h
    functionmanagermock.h
    hippomocks.h
    mocks.h
    pluginmanagermock.h
    testutils_global.h
)

set(PROJECT_SOURCES
    collationmanagermock.cpp
    configmock.cpp
    dbattachermock.cpp
    dbmanagermock.cpp
    dbsqlite3mock.cpp
    extensionmanagermock.cpp
    functionmanagermock.cpp
    mocks.cpp
    pluginmanagermock.cpp
)

add_library(TestUtils SHARED ${PROJECT_HEADERS} ${PROJECT_SOURCES})
letos_set_common_properties(TestUtils)
target_compile_definitions(TestUtils PUBLIC
    TESTUTILS_LIBRARY
)
if(NOT CUSTOM_SQLITE3)
        find_package(SQLite3)
endif()
target_link_libraries(TestUtils PUBLIC
    Qt::Core
    SQLite3::SQLite3
    coreLetos
)
target_link_libraries(TestUtils PUBLIC
    Qt::Test
)
