/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <sys.match/sys.match.hpp>
#include <eosio/system.hpp>

namespace match {
   void exchange::checkExcAcc(account_name exc_acc) {
      exchanges exc_tbl(_self,_self.value);
      auto itr1 = exc_tbl.find(exc_acc);
      check(itr1 != exc_tbl.end(), "exechange account has not been registered!");
   }

   uint64_t exchange::get_order_scope(const uint64_t &a,const uint64_t &b) {
      orderscopes order_scope_table(_self,_self.value);
      uint128_t scopekey =  make_128_key(a,b);
      auto idx = order_scope_table.get_index<"idxkey"_n>();
      auto itr = idx.find(scopekey);
      
      check(itr != idx.end(),"can not find scope");

      return itr->id;
   }

   // void exchange::match(const uint64_t &order_scope,const uint64_t &order_id,const account_name &exc_acc ) {
   //    orderbook order_book_table(_self,order_scope);
   //    auto order = order_book_table.find(order_id);
   //    check( order != order_book_table.end(),"can not find the order" );
   //    //价格获取 小数点后6位
   //    auto price = order->base.coin.amount * 1000000 / order->quote.coin.amount;
   //    auto relative_price = order->quote.coin.amount * 1000000 / order->base.coin.amount;
   //    //选取相对的订单进行匹配
   //    auto relative_order_scope = get_order_scope(order->quote.coin.symbol.raw(),order->base.coin.symbol.raw()); 
   //    orderbook relative_order_book_table(_self,relative_order_scope);
   //    //需要打印出来才能知道效果
   //    auto idx = relative_order_book_table.get_index<"pricekey"_n>();
   //    auto itr_low = idx.lower_bound( price );
   //    auto itr_up = idx.upper_bound( price );
   //    if ( itr_low != idx.end() ) {
   //       printf(itr_low->id);
   //    }
   //    else {
   //       printf("not found");
   //    }
   //    //match成功则执行done命令，done执行成交相关指令

   // }

   ACTION exchange::regex(account_name exc_acc){
      require_auth( exc_acc );
      printf("-----------------test-----------------------");
      //押金以后再考虑
      //const asset min_staked(10000000,CORE_SYMBOL);
      
      // check if exc_acc has freezed 1000 SYS
      // eosiosystem::system_contract sys_contract(config::system_account_name);
      // check(sys_contract.get_freezed(exc_acc) >= min_staked, "must freeze 1000 or more CDX!");
      
      exchanges exc_tbl(_self,_self.value);
      
      exc_tbl.emplace( name{exc_acc}, [&]( auto& e ) {
         e.exc_acc      = exc_acc;
      });
   }
//这个地方出问题了
   ACTION exchange::createtrade( asset base_coin, asset quote_coin, account_name exc_acc) {
      require_auth( exc_acc );
      printf("-----------------test-----------------------");
      checkExcAcc(exc_acc);
      check(base_coin.symbol.raw() != quote_coin.symbol.raw(), "base coin and quote coin must be different");
      trading_pairs trading_pairs_table(_self,exc_acc);

      uint128_t idxkey = make_trade_128_key(base_coin.symbol.raw(), quote_coin.symbol.raw());
      auto idx = trading_pairs_table.get_index<"idxkey"_n>();
      auto itr = idx.find(idxkey);
      check(itr == idx.end(), "trading pair already created");

      auto pk = trading_pairs_table.available_primary_key();
      trading_pairs_table.emplace( name{exc_acc}, [&]( auto& p ) {
         p.id = pk;
         p.base  = base_coin;
         p.quote  = quote_coin;
         p.frozen       = 0;
         p.fee_id       = -1;
      });

      //插入order scope  需要插入两个

      orderscopes order_scope_table(_self,_self.value);
      uint128_t scopekey_base =  make_128_key(base_coin.symbol.raw(),quote_coin.symbol.raw());
      auto idx_scope = order_scope_table.get_index<"idxkey"_n>();
      auto itr_base = idx_scope.find(scopekey_base);
      if ( itr_base == idx_scope.end() ) {
         auto orderscope = order_scope_table.available_primary_key();
         order_scope_table.emplace( name{exc_acc}, [&]( auto& p ) {
            p.id = orderscope;
            p.base = base_coin;
            p.quote = quote_coin;
         });
      }

      uint128_t scopekey_quote =  make_128_key(quote_coin.symbol.raw(),base_coin.symbol.raw());
      auto itr_quote = idx_scope.find(scopekey_quote);
      if ( itr_quote == idx_scope.end() ) {
         auto orderscope = order_scope_table.available_primary_key();
         order_scope_table.emplace( name{exc_acc}, [&]( auto& p ) {
            p.id = orderscope;
            p.base = quote_coin;
            p.quote = base_coin;
         });
      }
   }
//目前仅支持按比例收取
   ACTION exchange::feecreate(uint32_t type,uint32_t rate, asset base_coin, asset quote_coin, account_name exc_acc) {
      require_auth( exc_acc );

      checkExcAcc(exc_acc);
      trade_fees trade_fee_tbl(_self,exc_acc);
      auto pk = trade_fee_tbl.available_primary_key();
      trade_fee_tbl.emplace( name{exc_acc}, [&]( auto& p ) {
         p.id = pk;
         p.fees_base  = base_coin;
         p.fees_quote  = quote_coin;
         p.type = type;
         p.rate = rate;
      });
   }

   ACTION exchange::setfee(uint64_t trade_pair_id, uint64_t trade_fee_id, account_name exc_acc) {
      require_auth( exc_acc );

      checkExcAcc(exc_acc);
      trading_pairs trading_pairs_table(_self,exc_acc);
      auto trade_pair =  trading_pairs_table.find(trade_pair_id);
      check(trade_pair != trading_pairs_table.end(), "can not find the trade pair");

      trade_fees trade_fee_tbl(_self,exc_acc);
      auto trade_fee =  trade_fee_tbl.find(trade_pair_id);
      check(trade_fee != trade_fee_tbl.end(), "can not find the trade fee info");

      trading_pairs_table.modify(trade_pair, name{exc_acc}, [&]( auto& s ) {
         s.fee_id = trade_fee_id;
      });

   }

   void exchange::onforcetrans( const account_name& from,
                                 const account_name& to,
                                 const asset& quantity,
                                 const string& memo ) {
      if ( from == _self.value ) {
         return ;
      }
   }

   void exchange::onrelaytrans( const account_name from,
                                 const account_name to,
                                 const name chain,
                                 const asset quantity,
                                 const string memo ) {
      if ( from == _self.value ) {
         return ;
      }
   }

   ACTION exchange::makeorder(account_name traders,asset base,asset quote,uint64_t trade_pair_id, account_name exc_acc) {
      require_auth( traders );
      checkExcAcc(exc_acc);

      trading_pairs trading_pairs_table(_self,exc_acc);
      auto trade_pair =  trading_pairs_table.find(trade_pair_id);
      check(trade_pair != trading_pairs_table.end(), "can not find the trade pair");

      check( ( base == trade_pair->base && quote == trade_pair->quote )
         ||( base == trade_pair->quote && quote == trade_pair->base ) ,"the order do not match the tradepair" );

      
   }
//会不会有人open一大堆，但是不转帐？后续对排序等的影响   这个合约最重要的是内存由谁来支付的问题，这个是用户来支付的场景，合约帐户支付或者运营商支付更合理
   ACTION exchange::openorder(account_name traders, asset base_coin, asset quote_coin,uint64_t trade_pair_id, account_name exc_acc) {
      require_auth( traders );
      checkExcAcc(exc_acc);
     // printf(exc_acc,"---",trade_pair_id);

      trading_pairs trading_pairs_tbl(_self,exc_acc);
      auto trade_pair =  trading_pairs_tbl.find(trade_pair_id);
      check(trade_pair != trading_pairs_tbl.end(), "can not find the trade pair");

      check( ( base_coin.symbol == trade_pair->base.symbol && quote_coin.symbol == trade_pair->quote.symbol )
         ||( base_coin.symbol == trade_pair->quote.symbol && quote_coin.symbol == trade_pair->base.symbol ) ,"the order do not match the tradepair" );
      
      orderscopes order_scope_table(_self,_self.value);
      uint128_t scopekey_base =  make_128_key(base_coin.symbol.raw(),quote_coin.symbol.raw());
      eosio::print_f("% ---",scopekey_base);
      auto idx_scope = order_scope_table.get_index<"idxkey"_n>();
      auto itr_base = idx_scope.find(scopekey_base);
      check( itr_base != idx_scope.end() ,"can not find order scope");

      // auto timestamp = time_point_sec(uint32_t(current_time() / 1000000ll));
      auto timestamp = time_point_sec(0);
      orderbooks orderbook_table( _self,itr_base->id );
      orderbook_table.emplace( name{traders}, [&]( auto& p ) {
         p.id = orderbook_table.available_primary_key();
         p.pair_id = trade_pair_id;
         p.maker = traders;
         p.receiver = traders;
         p.base = base_coin;
         p.quote = quote_coin;
         p.orderstatus = 0;
         p.exc_acc = exc_acc;
         p.timestamp = timestamp;
      });
   }

   ACTION exchange::test(account_name traders) {
      orderscopes order_scope_table(_self,_self.value);
      auto itr = order_scope_table.begin();
      while (itr != order_scope_table.end()) {
         eosio::print_f("%----",itr->by_pair_sym());
         itr++;
      }
   }

   ACTION exchange::match(uint64_t scope_base,uint64_t base_id,uint64_t scope_quote, account_name exc_acc) {
      orderbooks orderbook_table( _self,scope_base );
      auto base_order = orderbook_table.find(base_id);
      check( base_order != orderbook_table.end(),"can not find base order" );

      auto base_price = static_cast<int128_t>(base_order->base.amount) * 1000000 / base_order->quote.amount;
      
      orderbooks orderbook_table_quote( _self,scope_quote );
      auto idx = orderbook_table_quote.get_index<"pricekey"_n>();
      auto itr_up = idx.upper_bound( base_price );

      for (auto itr1 = idx.cbegin();itr1 != itr_up; ++itr1) {
         eosio::print_f("%----low id \t",itr1->id);
      }
   }


} /// namespace eosio