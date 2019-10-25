/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <sys.match/sys.match.hpp>
#include <sys.match/match_define.hpp>
#include <eosio/system.hpp>
#include <../../eosio.system/include/eosio.system.hpp>
#include <../../eosio.token/include/eosio.token/eosio.token.hpp>

namespace match {
   void exchange::checkExcAcc(account_name exc_acc) {
      exchanges exc_tbl(_self,_self.value);
      auto itr1 = exc_tbl.find(exc_acc);
      check(itr1 != exc_tbl.end(), "exechange account has not been registered!");
   }

   inline void exchange::checkfeetype(name fee_type) {
      for( const auto& i : fee_type_data ) {
            if( i == fee_type ) {
               return ;
            }
         }
      check(false,"fee type is not vaild");
   }

   ACTION exchange::regex(account_name exc_acc){
      require_auth( exc_acc );
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
   ACTION exchange::createtrade(name trade_pair_name, asset base_coin, asset quote_coin, account_name exc_acc) {
      require_auth( exc_acc );
      checkExcAcc(exc_acc);
      check(base_coin.symbol.raw() != quote_coin.symbol.raw(), "base coin and quote coin must be different");
      trading_pairs trading_pairs_table(_self,exc_acc);

      // uint128_t idxkey = make_128_key(base_coin.symbol.raw(), quote_coin.symbol.raw());//好像是不行
      // auto idx = trading_pairs_table.get_index<"idxkey"_n>();
      // auto itr = idx.find(idxkey);
      // check(itr == idx.end(), "trading pair already created");

      auto pair = trading_pairs_table.find(trade_pair_name.value);
      check( pair == trading_pairs_table.end(),"trading pair already created" );

      uint64_t order_scope1 = 0,order_scope2 = 0;
      //插入order scope  需要插入两个
      orderscopes order_scope_table(_self,_self.value);
      uint128_t scopekey_base =  make_128_key(base_coin.symbol.raw(),quote_coin.symbol.raw(),base_coin.symbol.raw());
      auto idx_scope = order_scope_table.get_index<"idxkey"_n>();
      auto itr_base = idx_scope.find(scopekey_base);
      if ( itr_base == idx_scope.end() ) {
         order_scope1 = order_scope_table.available_primary_key();
         order_scope_table.emplace( name{exc_acc}, [&]( auto& p ) {
            p.id = order_scope1;
            p.base = base_coin;
            p.quote = quote_coin;
            p.coin = base_coin;
         });
      }
      else {
         order_scope1 = itr_base->id;
      }

      uint128_t scopekey_quote =  make_128_key(quote_coin.symbol.raw(),base_coin.symbol.raw(),base_coin.symbol.raw());
      auto itr_quote = idx_scope.find(scopekey_quote);
      if ( itr_quote == idx_scope.end() ) {
         order_scope2 = order_scope_table.available_primary_key();
         order_scope_table.emplace( name{exc_acc}, [&]( auto& p ) {
            p.id = order_scope2;
            p.base = quote_coin;
            p.quote = base_coin;
            p.coin = base_coin;
         });
      }
      else {
         order_scope2 = itr_quote->id;
      }

      trading_pairs_table.emplace( name{exc_acc}, [&]( auto& p ) {
         p.pair_name = trade_pair_name;
         p.base  = base_coin;
         p.quote  = quote_coin;
         p.frozen       = 0;
         p.fee_name     = name{};
         p.order_scope1 = order_scope1;
         p.order_scope2 = order_scope2;
      });

   }
//目前仅支持按比例收取
   ACTION exchange::feecreate(name fee_name,name fee_type,uint32_t rate, asset base_coin, asset quote_coin, account_name exc_acc) {
      require_auth( exc_acc );

      checkExcAcc(exc_acc);
      checkfeetype(fee_type);
      trade_fees trade_fee_tbl(_self,exc_acc);

      auto fee_info = trade_fee_tbl.find(fee_name.value);
      if ( fee_info == trade_fee_tbl.end() ) {
         trade_fee_tbl.emplace( name{exc_acc}, [&]( auto& p ) {
            p.fee_name = fee_name;
            p.fees_base  = base_coin;
            p.fees_quote  = quote_coin;
            p.fee_type = fee_type;
            p.rate = rate;
         });
      }
      else {
         trade_fee_tbl.modify(fee_info, name{exc_acc}, [&]( auto& p ) {
            p.fees_base  = base_coin;
            p.fees_quote  = quote_coin;
            p.fee_type = fee_type;
            p.rate = rate;
         } );
      }
   }

   ACTION exchange::setfee(name trade_pair_name, name fee_name, account_name exc_acc) {
      require_auth( exc_acc );

      checkExcAcc(exc_acc);
      trading_pairs trading_pairs_table(_self,exc_acc);
      auto trade_pair =  trading_pairs_table.find(trade_pair_name.value);
      check(trade_pair != trading_pairs_table.end(), "can not find the trade pair");

      trade_fees trade_fee_tbl(_self,exc_acc);
      auto trade_fee =  trade_fee_tbl.find(fee_name.value);
      check(trade_fee != trade_fee_tbl.end(), "can not find the trade fee info");

      //check fee type
      if ( trade_fee->fee_type.value == name{"f.fix"}.value || trade_fee->fee_type.value == name{"f.ratiofix"}.value ) {
         check( trade_fee->fees_base.symbol.raw() == trade_pair->base.symbol.raw() && trade_fee->fees_quote.symbol.raw() == trade_pair->quote.symbol.raw(),
               "can not to set fee to the pair,the type is not match the fee base or/and fee quote" );
      }

      trading_pairs_table.modify(trade_pair, name{exc_acc}, [&]( auto& s ) {
         s.fee_name = fee_name;
      });

   }
//取消订单
   ACTION exchange::cancelorder( uint64_t orderscope,uint64_t orderid) {
      orderbooks orderbook_table( _self,orderscope );
      auto order_info = orderbook_table.find(orderid);
      check( order_info != orderbook_table.end(),"can not find the order");

      require_auth( order_info->maker );
      transfer_to_other(order_info->undone_base,order_info->maker);
      orderbook_table.erase(order_info);
   }

   ACTION exchange::claimdeposit( const account_name &user,const asset &quantity,const string &memo ) {
      require_auth( user );
      deposits deposit_table(_self,user);
      auto exist = deposit_table.find( quantity.symbol.raw() );
      check( exist != deposit_table.end(),"the deposit do not existed");
      check( exist->balance.amount >= quantity.amount,"you do not have enough deposit" );

      transfer_to_other(quantity,user);
      deposit_table.modify(exist, name{}, [&]( auto& s ) {
         s.balance -= quantity;
      });

   }

   ACTION exchange::depositorder(const account_name &traders,const asset &base_coin,const asset &quote_coin,
                                 const name &trade_pair_name,const account_name &exc_acc) {
      require_auth( traders );
      deposits deposit_table(_self,traders);
      auto exist = deposit_table.find( base_coin.symbol.raw() );
      check( exist != deposit_table.end(),"the deposit do not existed");
      check( exist->balance.amount >= base_coin.amount,"you do not have enough deposit" );

      deposit_table.modify(exist, name{}, [&]( auto& s ) {
         s.balance -= base_coin;
      });

      openorder_action  temp { 
         _self,
         {  { name{exc_acc}, eosforce::active_permission },{ get_self(), eosforce::active_permission } }  
      };
      temp.send( traders,base_coin,quote_coin,trade_pair_name,exc_acc );
   }
//转帐eosio 代币
   void exchange::onforcetrans( const account_name& from,
                                 const account_name& to,
                                 const asset& quantity,
                                 const string& memo ) {
      if ( from == _self.value ) {
         return ;
      }
      
      transfer_memo trans(memo);
      if ( trans.type == match_order ) {
         openorder_action  temp { 
            _self,
            {  { name{trans.exc_acc}, eosforce::active_permission },{ get_self(), eosforce::active_permission } }  
         };
         temp.send( from,quantity,trans.dest,trans.pair_name,trans.exc_acc );
      }
      else if ( trans.type == match_deposit ) {
         transdeposit(quantity,from);
      }
   }
//转帐其他代币
   void exchange::ontokentrans( const account_name& from,
                                 const account_name& to,
                                 const asset& quantity,
                                 const string& memo ) {
      if ( from == _self.value ) {
         return ;
      }

      transfer_memo trans(memo);
      if ( trans.type == match_order ) {
         openorder_action  temp { 
            _self,
            {  { name{trans.exc_acc}, eosforce::active_permission },{ get_self(), eosforce::active_permission } }  
         };
         temp.send( from,quantity,trans.dest,trans.pair_name,trans.exc_acc );
      }
      else if ( trans.type == match_deposit ) {
         transdeposit(quantity,from);
      }
   }

//运营商支付，内部调用
   ACTION exchange::openorder(account_name traders, asset base_coin, asset quote_coin,name trade_pair_name, account_name exc_acc) {
      require_auth( _self );
      require_auth( exc_acc );
      checkExcAcc(exc_acc);

      trading_pairs trading_pairs_tbl(_self,exc_acc);
      auto trade_pair =  trading_pairs_tbl.find(trade_pair_name.value);
      check(trade_pair != trading_pairs_tbl.end(), "can not find the trade pair");

      check( ( base_coin.symbol == trade_pair->base.symbol && quote_coin.symbol == trade_pair->quote.symbol )
         ||( base_coin.symbol == trade_pair->quote.symbol && quote_coin.symbol == trade_pair->base.symbol ) ,"the order do not match the tradepair" );
      
      orderscopes order_scope_table(_self,_self.value);
      uint128_t scopekey_base =  make_128_key(base_coin.symbol.raw(),quote_coin.symbol.raw(),trade_pair->base.symbol.raw());
      
      auto idx_scope = order_scope_table.get_index<"idxkey"_n>();
      auto itr_base = idx_scope.find(scopekey_base);
      check( itr_base != idx_scope.end() ,"can not find order scope");

      auto current_block_num = eosio::current_block_num();
      orderbooks orderbook_table( _self,itr_base->id );
      auto order_id = orderbook_table.available_primary_key();
      orderbook_table.emplace( name{exc_acc}, [&]( auto& p ) {
         p.id = order_id;
         p.pair_name = trade_pair_name;
         p.maker = traders;
         p.receiver = traders;
         p.base = base_coin;
         p.quote = quote_coin;
         p.undone_base = base_coin;
         p.undone_quote = quote_coin;
         p.orderstatus = 0;
         p.exc_acc = exc_acc;
         p.order_block_num = current_block_num;
      });

      uint128_t scopekey_quote =  make_128_key(quote_coin.symbol.raw(),base_coin.symbol.raw(),trade_pair->base.symbol.raw());
      auto itr_quote = idx_scope.find(scopekey_quote);
      check( itr_quote != idx_scope.end() ,"can not find order scope");

      match_action temp { 
         _self,
         {  { name{exc_acc}, eosforce::active_permission } }  
      };
      temp.send( itr_base->id,order_id,itr_quote->id,trade_pair_name,exc_acc );
   }

//成交数据处理，费用处理
//成交，一定要满足一定条件再落，毕竟占用内存比较多，而且内存的支付由谁来？吃单有问题，如果吃了比自己小的单子，是否就会改变自己的价格？这个问题明天再说
   ACTION exchange::match(uint64_t scope_base,uint64_t base_id,uint64_t scope_quote, name trade_pair_name, account_name exc_acc) {
      require_auth( exc_acc );
      
      trading_pairs trading_pairs_table(_self,exc_acc);
      auto trade_pair =  trading_pairs_table.find(trade_pair_name.value);
      check(trade_pair != trading_pairs_table.end(), "can not find the trade pair");
      auto fee_name = trade_pair->fee_name;

      orderbooks orderbook_table( _self,scope_base );
      auto base_order = orderbook_table.find(base_id);
      check( base_order != orderbook_table.end(),"can not find base order" );

      orderscopes order_scope_table(_self,_self.value);
      auto scope_order = order_scope_table.find(scope_base);
      check( scope_order != order_scope_table.end(),"can not find order scope" );
      auto coin = scope_order->coin;

      auto base_price = static_cast<int128_t>(base_order->base.amount) * 1000000 / base_order->quote.amount;
      orderbooks orderbook_table_quote( _self,scope_quote );
      auto deal_scope = scope_base > scope_quote ? scope_base : scope_quote;

      auto current_block = eosio::current_block_num();
      bool is_not_done = true;
      auto undone_base = base_order->undone_base;
      auto undone_quote = base_order->undone_quote;

      auto done_base = asset(0,undone_base.symbol);
      auto done_quote = asset(0,undone_quote.symbol);

      bool base_coin = coin.symbol.raw() == undone_base.symbol.raw();

      while(is_not_done) {
         auto idx = orderbook_table_quote.get_index<"pricekey"_n>();
         auto itr_up = idx.upper_bound( base_price );
         auto itr_begin = idx.cbegin();

         if ( base_coin ) {
            undone_quote = asset(static_cast<int64_t>(static_cast<uint128_t>(undone_base.amount) * base_order->quote.amount / base_order->base.amount),undone_quote.symbol);
         }
         else {
            undone_base = asset(static_cast<int64_t>(static_cast<uint128_t>(undone_quote.amount) * base_order->base.amount / base_order->quote.amount),undone_base.symbol);
         }

         if ( itr_begin != itr_up ) {

            order_deal_info order_deal_base = order_deal_info{itr_begin->id,itr_begin->exc_acc,itr_begin->maker};
            order_deal_info order_deal_quote = order_deal_info{base_order->id,base_order->exc_acc,base_order->maker};
            //部分成交
            if ( ( base_coin && itr_begin->undone_quote <= undone_base ) || ( !base_coin && itr_begin->undone_base <= undone_quote) ) {
               //修改表格     或者这里不修改，到循环结束再修改
               undone_base -= itr_begin->undone_quote;
               undone_quote -= itr_begin->undone_base;

               done_base += itr_begin->undone_quote;
               done_quote += itr_begin->undone_base;
               record_deal_info(deal_scope,order_deal_base,order_deal_quote,itr_begin->undone_base,itr_begin->undone_quote,current_block,exc_acc);
               //打币 给itr_begin->receiver 打币 itr_begin->undone_quote
               //transfer_to_other(itr_begin->undone_quote,itr_begin->receiver);
               dealfee(itr_begin->undone_quote,itr_begin->receiver,fee_name,exc_acc);
               idx.erase( itr_begin );

               if ( ( !base_coin && undone_quote.amount == 0 ) || ( base_coin && undone_base.amount == 0 ) ) {
                  is_not_done = false;
               }
            }
            //全部成交  quote单有剩余
            else {
               //base 还是quote要考虑一下
               auto quote_order_quote = asset( static_cast<int128_t>(undone_quote.amount) * static_cast<int128_t>(itr_begin->undone_quote.amount) 
                  / itr_begin->undone_base.amount,itr_begin->undone_quote.symbol );
               //打币 给itr_begin->receiver 打币 quote_order_quote
               //transfer_to_other(quote_order_quote,itr_begin->receiver,fee_name,exc_acc);
               dealfee(quote_order_quote,itr_begin->receiver,fee_name,exc_acc);
               //quote base coin 需要使用  以我的价格为准的，不需要修改
               idx.modify(itr_begin, name{exc_acc}, [&]( auto& s ) {
                  s.undone_base -= undone_quote;
                  s.undone_quote -= quote_order_quote;
               });
               record_deal_info(deal_scope,order_deal_base,order_deal_quote,undone_quote,quote_order_quote,current_block,exc_acc);
               
               done_base += quote_order_quote;
               done_quote += undone_quote;

               undone_base -= quote_order_quote;
               undone_quote -= undone_quote;

               is_not_done = false;
            }
         }
         else {
            is_not_done = false;
         }
      }


      
      //先打币，给base_order->receiver 打币done_quote，再改表
      if ( done_quote.amount > 0 ) {
         //记录这次成交相关价格信息
         if( deal_scope == scope_quote ) {
            record_price_info(deal_scope,done_base,done_quote,current_block,exc_acc);
         }
         else {
            record_price_info(deal_scope,done_quote,done_base,current_block,exc_acc);
         }

         dealfee(done_quote,base_order->receiver,fee_name,exc_acc);
      }
      
      if ( ( base_coin && undone_base.amount > 0 ) || ( !base_coin && undone_quote.amount > 0 ) ) {
         
         if ( base_coin ) {
            undone_quote = asset(static_cast<int64_t>(static_cast<uint128_t>(undone_base.amount) * base_order->quote.amount / base_order->base.amount),undone_quote.symbol);
         }
         else {
            undone_base = asset(static_cast<int64_t>(static_cast<uint128_t>(undone_quote.amount) * base_order->base.amount / base_order->quote.amount),undone_base.symbol);
         }

         orderbook_table.modify(base_order,name{exc_acc},[&]( auto& s ) {
            s.undone_base = undone_base;
            s.undone_quote = undone_quote;
         });
      }
      else {
         //返币
         if ( base_coin && undone_quote.amount > 0 ) {
            transdeposit(undone_quote,base_order->receiver);
         }
         else if( !base_coin && undone_base.amount > 0 ){
            transdeposit(undone_base,base_order->receiver);
         }
         orderbook_table.erase(base_order);
      }

   }

   ACTION exchange::opendeposit( const account_name &user,const asset &quantity,const string &memo ) {
      require_auth( user );

      deposits deposit_table(_self,user);
      auto exist = deposit_table.find( quantity.symbol.raw() );
      check( exist == deposit_table.end(),"the deposit has existed");

      deposit_table.emplace(name{user},[&]( auto& s ){
         s.balance = asset(0,quantity.symbol);
         s.frozen_balance = asset(0,quantity.symbol);
      });
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

   void exchange::transfer_to_other(const asset& quantity,const account_name& to) {
      if ( quantity.symbol == CORE_SYMBOL ) {
         eosio::transfer_action temp { 
            eosforce::system_account, 
            {  { get_self(), eosforce::active_permission } } 
         };
         temp.send( get_self().value, to, quantity, std::string( " match order deal transfer coin" ) );
      }
      else {
         eosio::transfer_action temp { 
            eosforce::token_account, 
            {  { get_self(), eosforce::active_permission } } 
         };
         temp.send( get_self().value, to, quantity, std::string( " match order deal transfer coin" ) );
      }
   }

   void exchange::transdeposit(const asset& quantity,const account_name& to) {

      deposits deposit_table(_self,to);
      auto exist = deposit_table.find( quantity.symbol.raw() );
      check( exist != deposit_table.end(),"the deposit do not existed");

      deposit_table.modify(exist, name{}, [&]( auto& s ) { 
         s.balance += quantity;
      });
   }

   void exchange::dealfee(const asset &quantity,const account_name &to,const name &fee_name,const account_name &exc_acc) {
      
      if ( fee_name.value == name{}.value ) {
         transdeposit(quantity,to);
         return ;
      }

      trade_fees trade_fee_tbl(_self,exc_acc);
      auto trade_fee =  trade_fee_tbl.find(fee_name.value);
      check(trade_fee != trade_fee_tbl.end(), "can not find the trade fee info");

      asset exc_quantity,trader_quantity,min_fee;
      switch(trade_fee->fee_name.value) {
         case name{"f.null"_n}.value:
            transdeposit(quantity,to);
            break;
         case name{"f.fix"_n}.value:
            exc_quantity = quantity.symbol.raw() == trade_fee->fees_base.symbol.raw() ? trade_fee->fees_base:trade_fee->fees_quote;
            trader_quantity = quantity - exc_quantity;
            transdeposit(trader_quantity,to);
            transdeposit(exc_quantity,exc_acc);
            break;
         case name{"f.ratio"_n}.value:
            exc_quantity = quantity * trade_fee->rate / 10000;
            trader_quantity = quantity - exc_quantity;
            transdeposit(trader_quantity,to);
            transdeposit(exc_quantity,exc_acc);
            break;
         case name{"f.ratiofix"_n}.value:
            exc_quantity = quantity * trade_fee->rate / 10000;
            min_fee = quantity.symbol.raw() == trade_fee->fees_base.symbol.raw() ? trade_fee->fees_base:trade_fee->fees_quote;
            exc_quantity = exc_quantity > min_fee ? exc_quantity : min_fee;
            trader_quantity = quantity - exc_quantity;
            transdeposit(trader_quantity,to);
            transdeposit(exc_quantity,exc_acc);
            break;
         case name{"p.fix"_n}.value:
            transdeposit(quantity,to);
            break;
         case name{"p.ratio"_n}.value:
            transdeposit(quantity,to);
            break;
         case name{"p.ratiofix"_n}.value:
            transdeposit(quantity,to);
            break;
         default:
            transdeposit(quantity,to);
            break;                        
      }
   }


} /// namespace eosio
