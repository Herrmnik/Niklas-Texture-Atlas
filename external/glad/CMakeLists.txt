# library
set( LIB_NAME glad )

# create upper case name
string( TOUPPER ${LIB_NAME} LIB_NAME_UC )

#configure_file( version.h.in "${CMAKE_BINARY_DIR}/${LIB_NAME}/version.h" )

# get source files
file( GLOB SRC_FILES "*.c" )

# get include files
file( GLOB PRIVATE_INC_FILES "*.h" )
file( GLOB PUBLIC_INC_FILES  "../../../include/${LIB_NAME}/*.h" )
set( INC_FILES ${PRIVATE_INC_FILES} ${PUBLIC_INC_FILES} )

# library
add_library( ${LIB_NAME} STATIC ${SRC_FILES} ${INC_FILES} )
target_compile_definitions( ${LIB_NAME} PRIVATE ${LIB_NAME_UC}_SOURCE )
target_compile_definitions( ${LIB_NAME} PUBLIC $<$<AND:$<PLATFORM_ID:Windows>,$<STREQUAL:$<TARGET_PROPERTY:${LIB_NAME},TYPE>,SHARED_LIBRARY>>:${LIB_NAME_UC}_DYN_LINK> )
target_include_directories( ${LIB_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include> $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}> )

# Example to configure library specific warning levels.
#target_compile_options( ${LIB_NAME} PRIVATE $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wall -Werror -Wno-unused-function -Wno-enum-compare-switch -Wno-unknown-attributes>
#                                            $<$<CXX_COMPILER_ID:GNU>:-Wall -Werror -fdiagnostics-show-option -Wno-unused-function -Wno-sign-compare>
#                                            $<$<CXX_COMPILER_ID:MSVC>:/W3 /WX /wd4251> )

#target_include_directories( ${LIB_NAME} PRIVATE           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include> $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
#                                        SYSTEM INTERFACE  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include> $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}> )

# How to enable clang-tidy checks on target glad
if( GLSAMPLE_ENABLE_CLANG_TIDY AND TARGET BBuildEnv::clang-tidy )
  get_target_property( clang_tidy_cmd BBuildEnv::clang-tidy IMPORTED_LOCATION )
  set_target_properties( ${LIB_NAME} PROPERTIES CXX_CLANG_TIDY "${clang_tidy_cmd}" )
endif()

target_link_libraries( ${LIB_NAME} Threads::Threads )

# example: place header files in different folders
source_group( "Header Files"          FILES ${PUBLIC_INC_FILES} )
source_group( "Header Files\\private" FILES ${PRIVATE_INC_FILES} )
source_group( "Resource Files"        FILES ${RESOURCE_FILE} )

# set the folder where to place the projects
set_target_properties( ${LIB_NAME} PROPERTIES FOLDER lib )
