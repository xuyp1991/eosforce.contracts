add_contract(sys.match sys.match 
   ${CMAKE_CURRENT_SOURCE_DIR}/src/sys.match.cpp)

target_include_directories(sys.match PUBLIC 
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../eosio.token/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../include)

set_target_properties(sys.match PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/sys.match.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/sys.match.contracts.md @ONLY )
target_compile_options( sys.match PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
