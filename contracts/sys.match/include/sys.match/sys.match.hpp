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

 //自带排序 有问题
   inline uint128_t make_128_key(const uint64_t &a,const uint64_t &b) {
      return uint128_t(a)<<64 | b;
   }

   inline uint128_t make_trade_128_key(const uint64_t &a,const uint64_t &b) {
      if (a < b) return uint128_t(a)<<64 | b;
      return uint128_t(b)<<64 | a;
   }

   struct [[eosio::table, eosio::contract("sys.match")]] trading_pair{
      uint64_t id;
      
      asset base;
      asset quote;
      
      uint32_t    frozen;
      int64_t     fee_id;
      
      uint64_t primary_key() const { return id; }
      uint128_t by_pair_sym() const { return make_trade_128_key( base.symbol.raw() , quote.symbol.raw() ); }
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
      uint64_t        pair_id;
      account_name    maker;
      account_name    receiver;
      asset           base;
      asset           quote;
      uint32_t        orderstatus;
      time_point_sec  timestamp;
      account_name    exc_acc;

      uint64_t primary_key() const { return id; }
      uint64_t get_price() const {
         return quote.amount * 1000000 / base.amount;
      }
   };
   typedef eosio::multi_index<"orderbook"_n, order
      ,indexed_by< "pricekey"_n, const_mem_fun<order, uint64_t, &order::get_price>>
   > orderbooks;

   struct [[eosio::table, eosio::contract("sys.match")]] trade_scope {
      uint64_t id;
      asset base;
      asset quote;
      
      uint64_t primary_key() const { return id; }
      uint128_t by_pair_sym() const { return make_128_key( base.symbol.raw() , quote.symbol.raw() ); }
   };
   typedef eosio::multi_index<"orderscope"_n, trade_scope,
      indexed_by< "idxkey"_n, const_mem_fun<trade_scope, uint128_t, &trade_scope::by_pair_sym>>
         > orderscopes; 

   //所有的成交放在一起，如何查询自己的成交？
   struct [[eosio::table, eosio::contract("sys.match")]] deal_info {
      uint64_t    id;
      uint64_t    order1_id;
      uint64_t    order2_id;

      asset           base;
      asset           quote;
      
      uint32_t    deal_block;
      uint64_t primary_key() const { return id; }
   };
   typedef eosio::multi_index<"deal"_n, deal_info> deals;

   struct [[eosio::table, eosio::contract("sys.match")]] record_deal_info {
      uint32_t    deal_block;
      asset       base;
      asset       quote;
      
      uint64_t primary_key() const { return deal_block; }
   };
   typedef eosio::multi_index<"recorddeal"_n, record_deal_info> record_deals;

   struct [[eosio::table, eosio::contract("sys.match")]] record_price_info {
      uint64_t        id;
      asset       base;
      asset       quote;
      uint32_t        start_block;
      uint64_t primary_key() const { return id; }
   };
   typedef eosio::multi_index<"recordprice"_n, record_price_info> record_prices;

   struct [[eosio::table, eosio::contract("sys.match")]]  fee_info {
      uint64_t    id;
      
      uint32_t    type;
      uint32_t    rate;
      
      asset       fees_base;
      asset       fees_quote;
      
      uint64_t primary_key() const { return id; }
   };
   typedef eosio::multi_index<"tradefee"_n, fee_info> trade_fees;

   class [[eosio::contract("sys.match")]] exchange : public contract {
      public:
         using contract::contract;

         ACTION regex(account_name exc_acc);
         ACTION createtrade( asset base_coin, asset quote_coin, account_name exc_acc);
         ACTION feecreate(uint32_t type,uint32_t rate, asset base_coin, asset quote_coin, account_name exc_acc);
         ACTION setfee(uint64_t trade_pair_id, uint64_t trade_fee_id, account_name exc_acc);

         ACTION makeorder(account_name traders,asset base,asset quote,uint64_t trade_pair_id, account_name exc_acc);
         ACTION openorder(account_name traders, asset base_coin, asset quote_coin,uint64_t trade_pair_id, account_name exc_acc);
         ACTION test(account_name traders);
         ACTION match(uint64_t scope_base,uint64_t base_id,uint64_t scope_quote, account_name exc_acc);

         [[eosio::on_notify("eosio::transfer")]]
         void onforcetrans( const account_name& from,
                         const account_name& to,
                         const asset& quantity,
                         const string& memo );

         [[eosio::on_notify("eosio.token::transfer")]]
         void onrelaytrans( const account_name from,
               const account_name to,
               const name chain,
               const asset quantity,
               const string memo );

      private:
         void checkExcAcc(account_name exc_acc);
         uint64_t get_order_scope(const uint64_t &a,const uint64_t &b);

   };
   /** @}*/ // end of @defgroup eosiomsig eosio.msig
} /// namespace eosio