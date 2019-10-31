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
      //todo
      //const asset min_staked(10000000,CORE_SYMBOL);
      
      // check if exc_acc has freezed 1000 SYS
      // eosiosystem::system_contract sys_contract(config::system_account_name);
      // check(sys_contract.get_freezed(exc_acc) >= min_staked, "must freeze 1000 or more CDX!");
      
      exchanges exc_tbl(_self,_self.value);
      
      exc_tbl.emplace( name{exc_acc}, [&]( auto& e ) {
         e.exc_acc      = exc_acc;
      });
   }

   ACTION exchange::createtrade(name trade_pair_name, asset base_coin, asset quote_coin, account_name exc_acc) {
      require_auth( exc_acc );
      checkExcAcc(exc_acc);
      check(base_coin.symbol.raw() != quote_coin.symbol.raw(), "base coin and quote coin must be different");
      check(base_coin.symbol.precision() < 0x0F && quote_coin.symbol.precision() < 0x0F,"the contract does not support large precision coin");
      trading_pairs trading_pairs_table(_self,exc_acc);

      auto pair = trading_pairs_table.find(trade_pair_name.value);
      check( pair == trading_pairs_table.end(),"trading pair already created" );

      uint64_t order_scope1 = 0,order_scope2 = 0;
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
            p.order_id = 0;
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
            p.order_id = 0;
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

   ACTION exchange::feecreate(name fee_name,name fee_type,uint32_t rate,uint32_t rate_base,uint32_t rate_quote, asset coin_card, account_name exc_acc) {
      require_auth( exc_acc );

      checkExcAcc(exc_acc);
      checkfeetype(fee_type);
      trade_fees trade_fee_tbl(_self,exc_acc);

      auto fee_info = trade_fee_tbl.find(fee_name.value);
      if ( fee_info == trade_fee_tbl.end() ) {
         trade_fee_tbl.emplace( name{exc_acc}, [&]( auto& p ) {
            p.fee_name = fee_name;
            p.fee_card  = coin_card;
            p.fee_type = fee_type;
            p.rate = rate;
            p.rate_base = rate_base;
            p.rate_quote = rate_quote;
         });
      }
      else {
         trade_fee_tbl.modify(fee_info, name{exc_acc}, [&]( auto& p ) {
            p.fee_card  = coin_card;
            p.fee_type = fee_type;
            p.rate = rate;
            p.rate_base = rate_base;
            p.rate_quote = rate_quote;
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

      trading_pairs_table.modify(trade_pair, name{exc_acc}, [&]( auto& s ) {
         s.fee_name = fee_name;
      });

   }

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

   ACTION exchange::openorder(account_name traders, asset base_coin, asset quote_coin,name trade_pair_name, account_name exc_acc) {
      require_auth( _self );
      require_auth( exc_acc );
      checkExcAcc(exc_acc);

      trading_pairs trading_pairs_tbl(_self,exc_acc);
      auto trade_pair =  trading_pairs_tbl.find(trade_pair_name.value);
      check(trade_pair != trading_pairs_tbl.end(), "can not find the trade pair");

      check( ( base_coin.symbol == trade_pair->base.symbol && quote_coin.symbol == trade_pair->quote.symbol && base_coin > trade_pair->base )
         ||( base_coin.symbol == trade_pair->quote.symbol && quote_coin.symbol == trade_pair->base.symbol && base_coin > trade_pair->quote ) ,"the order do not match the tradepair" );
      
      orderscopes order_scope_table(_self,_self.value);
      uint128_t scopekey_base =  make_128_key(base_coin.symbol.raw(),quote_coin.symbol.raw(),trade_pair->base.symbol.raw());
      
      auto idx_scope = order_scope_table.get_index<"idxkey"_n>();
      auto itr_base = idx_scope.find(scopekey_base);
      check( itr_base != idx_scope.end() ,"can not find order scope");

      auto current_block_num = eosio::current_block_num();
      auto base_scope = itr_base->id;
      orderbooks orderbook_table( _self,base_scope );
      auto order_id = itr_base->order_id;
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

      idx_scope.modify( itr_base,name{},[&]( auto& p ) { 
         p.order_id++;
      });

      uint128_t scopekey_quote =  make_128_key(quote_coin.symbol.raw(),base_coin.symbol.raw(),trade_pair->base.symbol.raw());
      auto itr_quote = idx_scope.find(scopekey_quote);
      check( itr_quote != idx_scope.end() ,"can not find order scope");

      match_action temp { 
         _self,
         {  { name{exc_acc}, eosforce::active_permission } }  
      };
      temp.send( base_scope,order_id,itr_quote->id,trade_pair_name,exc_acc );
   }

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

      auto base_price = cal_price(base_order->base.amount,base_order->quote.amount,base_order->base.symbol.precision(),base_order->quote.symbol.precision());//static_cast<int128_t>(base_order->base.amount) * 1000000 / base_order->quote.amount;   //to be modify
      orderbooks orderbook_table_quote( _self,scope_quote );
      auto deal_scope = scope_base > scope_quote ? scope_base : scope_quote;

      auto current_block = eosio::current_block_num();
      bool is_not_done = true;
      auto undone_base = base_order->undone_base;
      auto undone_quote = base_order->undone_quote;

      auto done_base = asset(0,undone_base.symbol);
      auto done_quote = asset(0,undone_quote.symbol);

      bool base_coin = coin.symbol.raw() == undone_base.symbol.raw();

      vector<recorddeal_param> deal_param;

      while(is_not_done) {
         auto idx = orderbook_table_quote.get_index<"pricekey"_n>();
         auto itr_up = idx.upper_bound( base_price );
         auto itr_begin = idx.cbegin();

         // if ( base_coin ) {
         //    undone_quote = asset(static_cast<int64_t>(static_cast<uint128_t>(undone_base.amount) * base_order->quote.amount / base_order->base.amount),undone_quote.symbol);
         // }
         // else {
         //    undone_base = asset(static_cast<int64_t>(static_cast<uint128_t>(undone_quote.amount) * base_order->base.amount / base_order->quote.amount),undone_base.symbol);
         // }

         if ( itr_begin != itr_up ) {

            order_deal_info order_deal_base = order_deal_info{scope_quote,itr_begin->id,itr_begin->exc_acc,itr_begin->maker};
            order_deal_info order_deal_quote = order_deal_info{scope_base,base_order->id,base_order->exc_acc,base_order->maker};
            asset temp_done_base,temp_done_quote;
            if ( ( base_coin && itr_begin->undone_quote <= undone_base ) || ( !base_coin && itr_begin->undone_base <= undone_quote) ) {
               if ( base_coin ) {
                  temp_done_quote = itr_begin->undone_quote;
                  temp_done_base = asset( static_cast<int128_t>(temp_done_quote.amount) * static_cast<int128_t>(itr_begin->base.amount) 
                  / itr_begin->quote.amount,itr_begin->undone_base.symbol );
               }
               else {
                  temp_done_base = itr_begin->undone_base;
                  temp_done_quote = asset( static_cast<int128_t>(temp_done_base.amount) * static_cast<int128_t>(itr_begin->quote.amount) 
                  / itr_begin->base.amount,itr_begin->undone_quote.symbol );
               }
               //这里添加真正需要成交的数量
               undone_base -= temp_done_quote;
               undone_quote -= temp_done_base;

               done_base += temp_done_quote;
               done_quote += temp_done_base;

               recorddeal_param temp_deal{deal_scope,order_deal_base,order_deal_quote,temp_done_base,temp_done_quote,current_block,exc_acc};
               deal_param.push_back(temp_deal);

               dealfee(temp_done_quote,itr_begin->receiver,fee_name,exc_acc,!base_coin);

               idx.erase( itr_begin );

               if ( ( !base_coin && undone_quote.amount == 0 ) || ( base_coin && undone_base.amount == 0 ) ) {
                  is_not_done = false;
               }
            }
            else {
               //这里添加真正需要成交的数量
               if ( base_coin ) {
                  temp_done_quote = undone_base;
                  temp_done_base = asset( static_cast<int128_t>(temp_done_quote.amount) * static_cast<int128_t>(itr_begin->base.amount) 
                  / itr_begin->quote.amount,itr_begin->undone_base.symbol );
               }
               else {
                  temp_done_base = undone_quote;
                  temp_done_quote = asset( static_cast<int128_t>(temp_done_base.amount) * static_cast<int128_t>(itr_begin->quote.amount) 
                  / itr_begin->base.amount,itr_begin->undone_quote.symbol );
               }
               // auto quote_order_quote = asset( static_cast<int128_t>(undone_quote.amount) * static_cast<int128_t>(itr_begin->undone_quote.amount) 
               //    / itr_begin->undone_base.amount,itr_begin->undone_quote.symbol );
               dealfee(temp_done_quote,itr_begin->receiver,fee_name,exc_acc,!base_coin);

               idx.modify(itr_begin, name{exc_acc}, [&]( auto& s ) {
                  s.undone_base -= temp_done_base;
                  s.undone_quote -= temp_done_quote;
               });

               recorddeal_param temp_deal{deal_scope,order_deal_base,order_deal_quote,temp_done_base,temp_done_quote,current_block,exc_acc};
               deal_param.push_back(temp_deal);
               
               undone_base -= temp_done_quote;
               undone_quote -= temp_done_base;

               done_base += temp_done_quote;
               done_quote += temp_done_base;

               is_not_done = false;
            }
         }
         else {
            is_not_done = false;
         }
      }

      if ( deal_param.size() > 0 ) {
         recorddeal_action temp { 
            _self,
            {  { name{exc_acc}, eosforce::active_permission },{ get_self(), eosforce::active_permission } }  
         };
         temp.send( deal_param );
      }

      if ( done_quote.amount > 0 ) {
         if( deal_scope == scope_quote ) {
            record_price_info(deal_scope,done_base,done_quote,current_block,exc_acc);
         }
         else {
            record_price_info(deal_scope,done_quote,done_base,current_block,exc_acc);
         }

         dealfee(done_quote,base_order->receiver,fee_name,exc_acc,base_coin);
      }
      
      if ( ( base_coin && undone_base.amount > 0 ) || ( !base_coin && undone_quote.amount > 0 ) ) {
         
         // if ( base_coin ) {
         //    undone_quote = asset(static_cast<int64_t>(static_cast<uint128_t>(undone_base.amount) * base_order->quote.amount / base_order->base.amount),undone_quote.symbol);
         // }
         // else {
         //    undone_base = asset(static_cast<int64_t>(static_cast<uint128_t>(undone_quote.amount) * base_order->base.amount / base_order->quote.amount),undone_base.symbol);
         // }

         orderbook_table.modify(base_order,name{exc_acc},[&]( auto& s ) {
            s.undone_base = undone_base;
            s.undone_quote = undone_quote;
         });
      }
      else {
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
         s.freezen = asset(0,quantity.symbol);
      });
   }

   ACTION exchange::recorddeal( vector<recorddeal_param> &params ) {
      require_auth( _self );
      for(auto recorddeal_info : params)
      {
         record_deal_info(recorddeal_info.deal_scope,recorddeal_info.deal_base,recorddeal_info.deal_quote,recorddeal_info.base
                           ,recorddeal_info.quote,recorddeal_info.current_block,recorddeal_info.exc_acc);
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

   bool exchange::paycardfee(const asset& quantity,const account_name& from,const account_name& to) {
      deposits deposit_table(_self,from);
      auto exist_from = deposit_table.find( quantity.symbol.raw() );
      if ( exist_from == deposit_table.end() ) {
         return false;
      }

      if (exist_from->balance < quantity) {
         return false;
      }

      deposits deposit_table_to(_self,to);
      auto exist_to = deposit_table_to.find( quantity.symbol.raw() );
      if ( exist_to == deposit_table_to.end() ) {
         return false;
      }
      
      deposit_table_to.modify(exist_to, name{}, [&]( auto& s ) { 
         s.balance += quantity;
      });

      deposit_table.modify(exist_from, name{}, [&]( auto& s ) { 
         s.balance -= quantity;
      });
      return true;
   }

   void exchange::dealfee(const asset &quantity,const account_name &to,const name &fee_name,const account_name &exc_acc,bool base_coin) {
      if ( fee_name.value == name{}.value ) {
         transdeposit(quantity,to);
         return ;
      }

      trade_fees trade_fee_tbl(_self,exc_acc);
      auto trade_fee =  trade_fee_tbl.find(fee_name.value);
      check(trade_fee != trade_fee_tbl.end(), "can not find the trade fee info");

      asset exc_quantity,trader_quantity,min_fee;
      auto card_rate = base_coin ? trade_fee->rate_base : trade_fee->rate_quote;
      bool cardfee_success = true;
      switch(trade_fee->fee_type.value) {
         case name{"f.null"_n}.value :
            transdeposit(quantity,to);
            break;
         case name{"f.ratio"_n}.value :
            exc_quantity = quantity * trade_fee->rate / FEE_ACCURACY;
            trader_quantity = quantity - exc_quantity;
            transdeposit(trader_quantity,to);
            transdeposit(exc_quantity,exc_acc);
            break;
         case name{"p.ratio"_n}.value :
            exc_quantity = asset(quantity.amount * card_rate / FEE_ACCURACY,trade_fee->fee_card.symbol);
            cardfee_success = paycardfee(exc_quantity,to,exc_acc);
            if ( !cardfee_success ) {
               exc_quantity = quantity * trade_fee->rate / FEE_ACCURACY;
               trader_quantity = quantity - exc_quantity;
               transdeposit(exc_quantity,exc_acc);
            }
            else {
               trader_quantity = quantity;
            }
            transdeposit(trader_quantity,to);
            break;
         default :
            transdeposit(quantity,to);
            break;
      }
   }


} /// namespace eosio
