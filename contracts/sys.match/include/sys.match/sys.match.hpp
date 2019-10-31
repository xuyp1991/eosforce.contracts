/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosforce/assetage.hpp>

#include <string>


namespace match {
   using std::string;
   using std::vector;
   using eosforce::CORE_SYMBOL;
   using eosio::name;
   using eosio::indexed_by;
   using eosio::const_mem_fun;
   using eosio::account_name;
   using eosio::asset;
   using eosio::time_point_sec;
   using eosio::contract;
   using eosio::check;
   using eosio::require_auth;

   static constexpr uint32_t PRICE_ACCURACY = 1000000;
   static constexpr uint32_t FEE_ACCURACY = 10000;


   inline uint128_t make_128_key(const uint64_t &a,const uint64_t &b) {
      return uint128_t(a)<<64 | b;
   }

   inline uint128_t make_trade_128_key(const uint64_t &a,const uint64_t &b) {
      if (a < b) return uint128_t(a)<<64 | b;
      return uint128_t(b)<<64 | a;
   }

   inline uint128_t make_128_key(const uint64_t &a,const uint64_t &b,const uint64_t &c) {
      if ( a == c ) return uint128_t(a)<<64 | b | 0x10;
      return uint128_t(a)<<64 | b;
   }

   inline uint128_t make_trade_128_key(const uint64_t &a,const uint64_t &b,const uint64_t &c) {
      if (a < b) {
         if ( a == c ) {
            return uint128_t(a)<<64 | b | 0x10;
         } 
         return uint128_t(a)<<64 | b;
      }
      if ( a == c ) {
         return uint128_t(b)<<64 | a | 0x10; 
      } 
      return uint128_t(b)<<64 | a;
   }

   inline uint128_t cal_price(const int64_t &a,const int64_t &b,const uint8_t precision_a,const uint8_t precision_b) {
      return static_cast<int128_t>(a) * PRICE_ACCURACY * eosforce::utils::precision_base(precision_b) / eosforce::utils::precision_base(precision_a) / b;
   }

   struct [[eosio::table, eosio::contract("sys.match")]] trading_pair{
      name pair_name;
      
      asset base;
      asset quote;
      
      uint32_t    frozen;
      name        fee_name;

      uint64_t    order_scope1;
      uint64_t    order_scope2;
      
      uint64_t primary_key() const { return pair_name.value; }
      uint128_t by_pair_sym() const { return make_trade_128_key( base.symbol.raw() , quote.symbol.raw(),base.symbol.raw() ); }
   };
   typedef eosio::multi_index<"pairs"_n, trading_pair,
      indexed_by< "idxkey"_n, const_mem_fun<trading_pair, uint128_t, &trading_pair::by_pair_sym>>
         > trading_pairs; 

   struct [[eosio::table, eosio::contract("sys.match")]] exc {
      account_name exc_acc;
      
      uint64_t primary_key() const { return exc_acc; }
   };
   typedef eosio::multi_index<"exchanges"_n, exc> exchanges;

   struct [[eosio::table, eosio::contract("sys.match")]] order {
      uint64_t        id;
      name            pair_name;
      account_name    maker;
      account_name    receiver;
      asset           base;
      asset           quote;
      asset           undone_base;
      asset           undone_quote;
      uint32_t        orderstatus;
      uint32_t        order_block_num;
      account_name    exc_acc;

      uint64_t primary_key() const { return id; }
      uint128_t get_price() const {
         return cal_price(quote.amount , base.amount,quote.symbol.precision(),base.symbol.precision());
      }
      uint64_t get_maker() const { return maker; }
   };
   typedef eosio::multi_index<"orderbook"_n, order
      ,indexed_by< "pricekey"_n, const_mem_fun<order, uint128_t, &order::get_price>>
      ,indexed_by< "bymaker"_n, const_mem_fun<order, uint64_t, &order::get_maker>>
   > orderbooks;

   struct [[eosio::table, eosio::contract("sys.match")]] trade_scope {
      uint64_t id;
      asset base;
      asset quote;

      asset coin;
      uint64_t order_id;
      
      uint64_t primary_key() const { return id; }
      uint128_t by_pair_sym() const { return make_128_key( base.symbol.raw() , quote.symbol.raw(),coin.symbol.raw() ); }
   };
   typedef eosio::multi_index<"orderscope"_n, trade_scope,
      indexed_by< "idxkey"_n, const_mem_fun<trade_scope, uint128_t, &trade_scope::by_pair_sym>>
         > orderscopes; 

   struct order_deal_info{
      uint64_t    order_scope;
      uint64_t    order_id;
      account_name exc_acc;
      account_name trader;
   };

   struct [[eosio::table, eosio::contract("sys.match")]] deal_info {
      uint64_t    id;
      order_deal_info order_base;
      order_deal_info order_quote;

      asset           base;
      asset           quote;
      
      uint32_t    deal_block;
      uint64_t primary_key() const { return id; }
   };
   typedef eosio::multi_index<"deal"_n, deal_info> deals;

   struct [[eosio::table, eosio::contract("sys.match")]] record_deal_info {
      uint64_t    id;
      asset       base;
      asset       quote;
      uint32_t    deal_block;
      uint64_t primary_key() const { return id; }
   };
   typedef eosio::multi_index<"recorddeal"_n, record_deal_info> record_deals;

   struct [[eosio::table, eosio::contract("sys.match")]] record_price_info {
      uint64_t        id;
      asset           base;
      asset           quote;
      uint32_t        start_block;
      uint64_t primary_key() const { return id; }
   };
   typedef eosio::multi_index<"recordprice"_n, record_price_info> record_prices;

   constexpr static auto fee_type_data = std::array<name, 7>{{
      name{"f.null"_n},
      name{"f.ratio"_n},
      name{"p.ratio"_n},
   }};

   struct [[eosio::table, eosio::contract("sys.match")]]  fee_info {
      name        fee_name;
      
      name        fee_type;
      uint32_t    rate;
      uint32_t    rate_base;
      uint32_t    rate_quote;
      
      asset fee_card;
      
      uint64_t primary_key() const { return fee_name.value; }
   };
   typedef eosio::multi_index<"tradefee"_n, fee_info> trade_fees;

   struct [[eosio::table, eosio::contract("sys.match")]]  deposit_info {
      asset balance;
      asset freezen;
      uint64_t primary_key() const { return balance.symbol.raw(); }
   };
   typedef eosio::multi_index<"deposit"_n, deposit_info> deposits;

   class [[eosio::contract("sys.match")]] exchange : public contract {
      public:
         struct recorddeal_param {
            uint64_t deal_scope;
            order_deal_info deal_base;
            order_deal_info deal_quote;
            asset base;
            asset quote;
            uint32_t current_block;
            account_name exc_acc;
         };
      public:
         using contract::contract;

         ACTION regex(account_name exc_acc);
         ACTION createtrade(name trade_pair_name, asset base_coin, asset quote_coin, account_name exc_acc);
         ACTION feecreate(name fee_name,name fee_type,uint32_t rate,uint32_t rate_base,uint32_t rate_quote, asset coin_card, account_name exc_acc);
         ACTION setfee(name trade_pair_name, name fee_name, account_name exc_acc);

         ACTION openorder(account_name traders, asset base_coin, asset quote_coin,name trade_pair_name, account_name exc_acc);
         ACTION match(uint64_t scope_base,uint64_t base_id,uint64_t scope_quote, name trade_pair_name, account_name exc_acc);

         ACTION cancelorder( uint64_t orderscope,uint64_t orderid);
         ACTION opendeposit( const account_name &user,const asset &quantity,const string &memo );
         ACTION claimdeposit( const account_name &user,const asset &quantity,const string &memo );

         ACTION depositorder(const account_name &traders,const asset &base_coin,const asset &quote_coin,const name &trade_pair_name,const account_name &exc_acc);

         ACTION recorddeal( vector<recorddeal_param> &params );

         [[eosio::on_notify("eosio::transfer")]]
         void onforcetrans( const account_name& from,
                         const account_name& to,
                         const asset& quantity,
                         const string& memo );

         [[eosio::on_notify("eosio.token::transfer")]]
         void ontokentrans( const account_name& from,
               const account_name& to,
               const asset& quantity,
               const string& memo );

         using regex_action            = eosio::action_wrapper<"regex"_n,           &exchange::regex>;
         using createtrade_action      = eosio::action_wrapper<"createtrade"_n,     &exchange::createtrade>;
         using feecreate_action        = eosio::action_wrapper<"feecreate"_n,       &exchange::feecreate>;
         using setfee_action           = eosio::action_wrapper<"setfee"_n,          &exchange::setfee>;
         using openorder_action        = eosio::action_wrapper<"openorder"_n,       &exchange::openorder>;
         using match_action            = eosio::action_wrapper<"match"_n,           &exchange::match>;
         using cancelorder_action      = eosio::action_wrapper<"cancelorder"_n,     &exchange::cancelorder>;
         using depositorder_action     = eosio::action_wrapper<"depositorder"_n,    &exchange::depositorder>;
         using recorddeal_action       = eosio::action_wrapper<"recorddeal"_n,      &exchange::recorddeal>;

      private:
         void checkExcAcc(account_name exc_acc);
         void record_deal_info(const uint64_t &deal_scope,const order_deal_info &deal_base,const order_deal_info &deal_quote,
                              const asset &base,const asset &quote,const uint32_t &current_block,const account_name &ram_payer );
         void record_price_info(const uint64_t &deal_scope,const asset &base,const asset &quote,const uint32_t &current_block,const account_name &ram_payer);
         void transfer_to_other(const asset& quantity,const account_name& to);
         inline void checkfeetype(name fee_type);
         void dealfee(const asset &quantity,const account_name &to,const name &fee_name,const account_name &exc_acc,bool base_coin);
         void transdeposit(const asset& quantity,const account_name& to);
         bool paycardfee(const asset& quantity,const account_name& from,const account_name& to);
   };
   /** @}*/ // end exchange
} /// end match
