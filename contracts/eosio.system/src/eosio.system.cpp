#include <utility>

#include <eosio.system.hpp>

namespace eosio {

   // WARNNING : EOSForce is different to eosio, which system will not call native contract in chain
   // so native is just to make abi, system_contract no need contain navtive
   system_contract::system_contract( name s, name code, datastream<const char*> ds ) 
      : contract( s, code, ds )
      {}

   system_contract::~system_contract() {}
} /// namespace eosio
