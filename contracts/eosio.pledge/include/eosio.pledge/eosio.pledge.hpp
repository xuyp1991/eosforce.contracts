#pragma once

#include <string>

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

#include <eosforce/assetage.hpp>

namespace eosio {
   using std::string;
   using eosforce::CORE_SYMBOL;

   struct [[eosio::table]] pledge_type {
      name         pledge_name       = name{};
      account_name deduction_account = eosforce::system_account.value;
      asset        pledge            = asset( 0, CORE_SYMBOL );

      uint64_t primary_key()const { return pledge_name.value; }
   };

   struct [[eosio::table]] pledge_info {
      name  pledge_name = name{};
      asset pledge      = asset( 0, CORE_SYMBOL );
      asset deduction   = asset( 0, CORE_SYMBOL );

      uint64_t primary_key()const { return pledge_name.value; }
   };

   struct [[eosio::table]] reward_info {
      asset reward = asset( 0, CORE_SYMBOL );

      uint64_t primary_key()const { return reward.symbol.code().raw(); }
   };

   typedef eosio::multi_index< "pledgetype"_n, pledge_type > pledgetypes;
   typedef eosio::multi_index< "pledge"_n,     pledge_info > pledges;
   typedef eosio::multi_index< "reward"_n,     reward_info > rewards;

   class [[eosio::contract("eosio.pledge")]] pledge : public contract {
      public:
         using contract::contract;

         pledge( name s, name code, datastream<const char*> ds );
         pledge( const pledge& ) = default;
         ~pledge();

      private:
         // tables for self, self, just some muit-used tables
         pledgetypes _pt_tbl;

      public:
         [[eosio::action]] void addtype( const name& pledge_name,
                                         const account_name& deduction_account,
                                         const account_name& ram_payer,
                                         const asset& quantity,
                                         const string& memo );

         [[eosio::on_notify("eosio::transfer")]]
         void addpledge( const account_name& from,
                         const account_name& to,
                         const asset& quantity,
                         const string& memo );

         [[eosio::action]] void deduction( const name& pledge_name,
                                           const account_name& debitee,
                                           const asset& quantity,
                                           const string& memo );

         [[eosio::action]] void withdraw( const name& pledge_name,
                                          const account_name& pledger,
                                          const asset& quantity,
                                          const string& memo );

         [[eosio::action]] void getreward( const account_name& rewarder,
                                           const asset& quantity,
                                           const string& memo );

         [[eosio::action]] void allotreward( const name& pledge_name,
                                             const account_name& pledger, 
                                             const account_name& rewarder,
                                             const asset& quantity,
                                             const string& memo );

         [[eosio::action]] void open( const name& pledge_name,
                                      const account_name& payer,
                                      const string& memo );

         [[eosio::action]] void close( const name& pledge_name,
                                       const account_name& payer,
                                       const string& memo );

         using addtype_action     = eosio::action_wrapper<"addtype"_n,     &pledge::addtype>;
         using deduction_action   = eosio::action_wrapper<"deduction"_n,   &pledge::deduction>;
         using withdraw_action    = eosio::action_wrapper<"withdraw"_n,    &pledge::withdraw>;
         using getreward_action   = eosio::action_wrapper<"getreward"_n,   &pledge::getreward>;
         using allotreward_action = eosio::action_wrapper<"allotreward"_n, &pledge::allotreward>;
         using open_action        = eosio::action_wrapper<"open"_n,        &pledge::open>;
         using close_action       = eosio::action_wrapper<"close"_n,       &pledge::close>;

         inline static asset get_pledge( const name& pledge_name, const account_name& pledger ) {
            pledges pledge_tbl( eosforce::pledge_account, pledger );
            const auto pledge_inf = pledge_tbl.find( pledge_name.value );
            if( pledge_inf == pledge_tbl.end() ) {
               return asset( 0, CORE_SYMBOL );
            } else {
               return pledge_inf->pledge;
            }
         }

         inline name check_pledge_typ( const name& n, const symbol& sym ) const {
            const auto p = _pt_tbl.find( n.value );
            check( p != _pt_tbl.end(), "the pledge type do not exist" );
            check( sym == p->pledge.symbol, "the symbol do not match" );
            return name{ p->deduction_account };
         }
   };

} /// namespace eosio
