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
//运营商支付，内部调用
   ACTION exchange::openorder(account_name traders, asset base_coin, asset quote_coin,uint64_t trade_pair_id, account_name exc_acc) {
      require_auth( _self );
      require_auth( exc_acc );
      checkExcAcc(exc_acc);

      trading_pairs trading_pairs_tbl(_self,exc_acc);
      auto trade_pair =  trading_pairs_tbl.find(trade_pair_id);
      check(trade_pair != trading_pairs_tbl.end(), "can not find the trade pair");

      check( ( base_coin.symbol == trade_pair->base.symbol && quote_coin.symbol == trade_pair->quote.symbol )
         ||( base_coin.symbol == trade_pair->quote.symbol && quote_coin.symbol == trade_pair->base.symbol ) ,"the order do not match the tradepair" );
      
      orderscopes order_scope_table(_self,_self.value);
      uint128_t scopekey_base =  make_128_key(base_coin.symbol.raw(),quote_coin.symbol.raw());
      
      auto idx_scope = order_scope_table.get_index<"idxkey"_n>();
      auto itr_base = idx_scope.find(scopekey_base);
      check( itr_base != idx_scope.end() ,"can not find order scope");

      auto current_block_num = eosio::current_block_num();
      orderbooks orderbook_table( _self,itr_base->id );
      auto order_id = orderbook_table.available_primary_key();
      orderbook_table.emplace( name{exc_acc}, [&]( auto& p ) {
         p.id = order_id;
         p.pair_id = trade_pair_id;
         p.maker = traders;
         p.receiver = traders;
         p.base = base_coin;
         p.quote = quote_coin;
         p.orderstatus = 0;
         p.exc_acc = exc_acc;
         p.order_block_num = current_block_num;
      });

      uint128_t scopekey_quote =  make_128_key(quote_coin.symbol.raw(),base_coin.symbol.raw());
      auto itr_quote = idx_scope.find(scopekey_quote);
      check( itr_quote != idx_scope.end() ,"can not find order scope");

      match_action temp { 
         _self,
         {  { name{exc_acc}, eosforce::active_permission } }  
      };
      temp.send( itr_base->id,order_id,itr_quote->id,exc_acc );
   }

//成交数据处理，费用处理
//成交，一定要满足一定条件再落，毕竟占用内存比较多，而且内存的支付由谁来？
   ACTION exchange::match(uint64_t scope_base,uint64_t base_id,uint64_t scope_quote, account_name exc_acc) {
      require_auth( exc_acc );
      orderbooks orderbook_table( _self,scope_base );
      auto base_order = orderbook_table.find(base_id);
      check( base_order != orderbook_table.end(),"can not find base order" );

      auto base_price = static_cast<int128_t>(base_order->base.amount) * 1000000 / base_order->quote.amount;
      orderbooks orderbook_table_quote( _self,scope_quote );
      auto scopekey_deal = make_trade_128_key(base_order->base.symbol.raw(),base_order->quote.symbol.raw());

      orderscopes order_scope_table(_self,_self.value);
      auto idx_deal = order_scope_table.get_index<"idxkey"_n>();
      auto itr_deal = idx_deal.find(scopekey_deal);
      check( itr_deal != idx_deal.end() ,"can not find order scope");
      auto deal_scope = itr_deal->id;

      auto current_block = eosio::current_block_num();
      bool is_not_done = true;
      auto undone_base = base_order->base;
      auto undone_quote = base_order->quote;

      while(is_not_done) {
         auto idx = orderbook_table_quote.get_index<"pricekey"_n>();
         auto itr_up = idx.upper_bound( base_price );
         auto itr_begin = idx.cbegin();
         if ( itr_begin != itr_up ) {
            //deal
            auto quote_price = static_cast<int128_t>(itr_begin->quote.amount) * 1000000 / itr_begin->base.amount;
            //eosio::print_f("quote_price--%--%--%--%--%--%\t",quote_price,itr_begin->base,itr_begin->quote,itr_begin->id,undone_base,undone_quote);
            order_deal_info order_deal_base = order_deal_info{itr_begin->id,itr_begin->exc_acc,itr_begin->maker};
            order_deal_info order_deal_quote = order_deal_info{base_order->id,base_order->exc_acc,base_order->maker};
            //部分成交
            if ( itr_begin->base <= undone_quote ) {
               //修改表格     或者这里不修改，到循环结束再修改
               undone_base -= itr_begin->quote;
               undone_quote -= itr_begin->base;

               record_deal_info(deal_scope,order_deal_base,order_deal_quote,itr_begin->base,itr_begin->quote,current_block,exc_acc);

               //打币
               idx.erase( itr_begin );

               if ( undone_quote.amount == 0 ) {
                  is_not_done = false;
               }
            }
            //全部成交  quote单有剩余
            else {
               //base 还是quote要考虑一下
               auto quote_order_quote = asset( static_cast<int128_t>(undone_quote.amount) * static_cast<int128_t>(itr_begin->quote.amount) 
                  / itr_begin->base.amount,itr_begin->quote.symbol );
               idx.modify(itr_begin, name{exc_acc}, [&]( auto& s ) {
                  s.base -= undone_quote;
                  s.quote -= quote_order_quote;
               });
               record_deal_info(deal_scope,order_deal_base,order_deal_quote,undone_quote,quote_order_quote,current_block,exc_acc);
               //打币
               undone_base -= quote_order_quote;
               undone_quote -= undone_quote;
               is_not_done = false;
            }
         }
         else {
            is_not_done = false;
         }
      }
//      eosio::print_f("deal_info--%--%\t",undone_base,undone_quote);
      //记录这次成交相关价格信息
      if( deal_scope == scope_quote ) {
         auto deal_base = base_order->quote - undone_quote;
         auto deal_quote = base_order->base - undone_base;
         record_price_info(deal_scope,deal_base,deal_quote,current_block,exc_acc);
      }
      else {
         auto deal_quote = base_order->quote - undone_quote;
         auto deal_base = base_order->base - undone_base;
         record_price_info(deal_scope,deal_base,deal_quote,current_block,exc_acc);
      }
      
      //先打币，再改表
      if ( undone_quote.amount > 0 ) {
         orderbook_table.modify(base_order,name{exc_acc},[&]( auto& s ) {
            s.base = undone_base;
            s.quote = undone_quote;
         });
      }
      else {
         //返币
         orderbook_table.erase(base_order);
      }

   }

   void exchange::record_deal_info(const uint64_t &deal_scope,const order_deal_info &deal_base,const order_deal_info &deal_quote,
                                    const asset &base,const asset &quote,const uint32_t &current_block,const account_name &ram_payer ){
      deals deal_table(_self,deal_scope);
      deal_table.emplace( name{ram_payer}, [&]( auto& p ) {
         p.id = deal_table.available_primary_key();
         p.order_base = deal_base;
         p.order_quote = deal_quote;
        
         p.base = base;
         p.quote = quote;
         p.deal_block = current_block;
      });
   }

   void exchange::record_price_info(const uint64_t &deal_scope,const asset &base,const asset &quote,const uint32_t &current_block,const account_name &ram_payer) {
      record_deals record_deal_table(_self,deal_scope);
      record_deal_table.emplace( name{ram_payer}, [&]( auto& p ) {
         p.id = record_deal_table.available_primary_key();
         p.base = base;
         p.quote =  quote;
         p.deal_block = current_block; 
      });

      record_prices record_price_table(_self,_self.value);
      auto price_info = record_price_table.find(deal_scope);
      if ( price_info == record_price_table.end() ) {
         record_price_table.emplace( name{ram_payer}, [&]( auto& p ){
            p.id = deal_scope;
            p.quote = quote;
            p.base = base;
            p.start_block = current_block; 
         });
      }
      else {
         record_price_table.modify(price_info,name{},[&]( auto& s ) {
            s.base += base;
            s.quote += quote;
         });
      }
   }


} /// namespace eosio
