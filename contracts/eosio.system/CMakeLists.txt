add_contract(eosio.system eosio.system 
   ${CMAKE_CURRENT_SOURCE_DIR}/src/eosio.system.cpp 
   ${CMAKE_CURRENT_SOURCE_DIR}/src/onblock.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/producer.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/transfer.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/native.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/vote.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/bppunish.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/freeze.cpp)

target_include_directories(eosio.system PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/../eosio.freeze/include)

set_target_properties(eosio.system PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/eosio.system.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/eosio.system.contracts.md @ONLY )

target_compile_options( eosio.system PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
