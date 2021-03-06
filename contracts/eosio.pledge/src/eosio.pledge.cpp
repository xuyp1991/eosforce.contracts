#include <eosio.pledge/eosio.pledge.hpp>
#include <../../eosio.system/include/eosio.system.hpp>

namespace eosio {

   pledge::pledge( name s, name code, datastream<const char*> ds ) 
      : contract( s, code, ds )
      , _pt_tbl( get_self(), get_self().value ) {}

   pledge::~pledge() {}

   void pledge::addtype( const name& pledge_name,
                         const account_name& deduction_account,
                         const account_name& ram_payer,
                         const asset& quantity,
                         const string& memo ) {
      require_auth( eosforce::system_account );

      auto old_type = _pt_tbl.find( pledge_name.value );
      check( old_type == _pt_tbl.end(), "the pledge type has exist" );

      _pt_tbl.emplace( eosforce::system_account, [&]( pledge_type& s ) {
         s.pledge_name       = pledge_name;
         s.deduction_account = deduction_account;
         s.pledge            = quantity;
      } );
   }

   void pledge::addpledge( const account_name& from,
                           const account_name& to,
                           const asset& quantity,
                           const string& memo ) {
      if( to != get_self().value || memo.length() == 0 ) {
         return;
      }

      require_auth( name{from} );

      const auto pledge_name = name{ memo };
      check_pledge_typ( pledge_name, quantity.symbol );

      pledges ple_tbl( get_self(), from );
      const auto pledge = ple_tbl.find( pledge_name.value );
      if( pledge == ple_tbl.end() ) {
         ple_tbl.emplace( name{from}, [&]( pledge_info& b ) {
            b.pledge_name = pledge_name;
            b.pledge      = quantity;
            b.deduction   = asset( 0, quantity.symbol );
         } );
      }
      else {
         ple_tbl.modify( pledge, name{}, [&]( pledge_info& b ) {
            b.pledge += quantity;
         } );
      }
   }

   void pledge::deduction( const name& pledge_name,
                           const account_name& debitee,
                           const asset& quantity,
                           const string& memo ) {
      const auto deductor = check_pledge_typ( pledge_name, quantity.symbol );
      require_auth( deductor );

      check( 0 < quantity.amount, "the quantity should be a positive number" );

      pledges ple_tbl( get_self(), debitee );
      auto pledge = ple_tbl.find( pledge_name.value );
      if( pledge == ple_tbl.end() ) {
         ple_tbl.emplace( name{debitee}, [&]( pledge_info& b ) {
            b.pledge_name = pledge_name;
            b.pledge      = - quantity;
            b.deduction   = quantity;
         } );
      } else {
         ple_tbl.modify( pledge, name{}, [&]( pledge_info& b ) {
            b.pledge    -= quantity;
            b.deduction += quantity;
         } );
      }
   }

   void pledge::withdraw( const name& pledge_name,
                          const account_name& pledger,
                          const asset& quantity,
                          const string& memo ) {
      require_auth( name{pledger} );
      check_pledge_typ( pledge_name, quantity.symbol );

      pledges ple_tbl( get_self(), pledger );
      const auto pledge = ple_tbl.find( pledge_name.value );
      check( pledge != ple_tbl.end(), "the pledge do not exist" );
      check( quantity < pledge->pledge, "the quantity is biger then the pledge" );
      check( asset( 0, pledge->pledge.symbol ) < quantity, "the quantity must be a positive number" );

      ple_tbl.modify( pledge, name{}, [&]( pledge_info& b ) { 
         b.pledge -= quantity;
      });

      transfer_action temp { 
         eosforce::system_account, 
         {  { get_self(), eosforce::active_permission } } 
      };
      temp.send( get_self().value, pledger, quantity, std::string( "withdraw pledge" ) );
   }

   void pledge::getreward( const account_name& rewarder,
                           const asset& quantity,
                           const string& memo ) {
      require_auth( name{rewarder} );

      rewards rew_tbl( get_self(), rewarder );
      auto reward_inf = rew_tbl.find( quantity.symbol.code().raw() );
      check( reward_inf != rew_tbl.end(), "the reward do not exist" );
      check( reward_inf->reward.amount > 0, "the reward do not enough to get" );

      transfer_action temp{
         eosforce::system_account, 
         {  { eosforce::pledge_account, eosforce::active_permission } }
      };
      temp.send( eosforce::pledge_account.value, rewarder, reward_inf->reward, std::string("get reward") );

      rew_tbl.modify( reward_inf, name{}, [&]( reward_info& b ) {
         b.reward -= b.reward;
      });
   }

   void pledge::allotreward( const name& pledge_name,
                             const account_name& pledger, 
                             const account_name& rewarder,
                             const asset& quantity,
                             const string& memo ) {
      const auto deductor = check_pledge_typ( pledge_name, quantity.symbol );
      require_auth( deductor );

      pledges ple_tbl( get_self(), pledger );
      auto pledge = ple_tbl.find( pledge_name.value );
      check( pledge != ple_tbl.end(), "the pledge do not exist" );
      auto pre_allot = pledge->deduction;
      if ( pledge->pledge < asset( 0, pledge->pledge.symbol ) ) {
         pre_allot += pledge->pledge;
      }
      check( 0 < quantity.amount, "the quantity must be a positive number" );
      check( quantity <= pre_allot, "the quantity is biger then the deduction" );
      ple_tbl.modify( pledge, name{}, [&]( pledge_info& b ) { 
         b.deduction -= quantity;
      });

      rewards rew_tbl( get_self(), rewarder );
      auto reward_inf = rew_tbl.find( quantity.symbol.code().raw() );
      if( reward_inf == rew_tbl.end() ) {
         rew_tbl.emplace( deductor, [&]( reward_info& b ) { 
            b.reward = quantity;
         } );
      } else {
         rew_tbl.modify( reward_inf, name{}, [&]( reward_info& b ) {
            b.reward += quantity;
         } );
      }
   }

   void pledge::open( const name& pledge_name,
                      const account_name& payer,
                      const string& memo ) {
      require_auth( name{payer} );
      auto type = _pt_tbl.find( pledge_name.value );
      check( type != _pt_tbl.end(), "the pledge type do not exist" );

      pledges ple_tbl( get_self(), payer );
      auto pledge = ple_tbl.find( pledge_name.value );
      if ( pledge != ple_tbl.end() ) {
         return ;
      }
      
      // check(pledge == ple_tbl.end(),"the pledge record has exist");
      ple_tbl.emplace( name{payer}, [&]( pledge_info& b ) { 
         b.pledge_name = pledge_name;
         b.pledge      = asset(0, type->pledge.symbol);
         b.deduction   = asset(0, type->pledge.symbol);
      });

   }
   void pledge::close( const name& pledge_name,
                       const account_name& payer,
                       const string& memo ) {
      require_auth( name{payer} );
      auto type = _pt_tbl.find( pledge_name.value );
      check( type != _pt_tbl.end(), "the pledge type do not exist" );

      pledges ple_tbl(get_self(),payer);
      auto pledge = ple_tbl.find(pledge_name.value);
      check(pledge != ple_tbl.end(),"the pledge record has exist");
      check(pledge->pledge == asset(0,type->pledge.symbol),"the pledge is not 0,can not be closed");
      check(pledge->deduction == asset(0,type->pledge.symbol),"the deduction is not 0,can not be closed");

      ple_tbl.erase( pledge );
   }

   void pledge::amendpledge( const name& pledge_name,
                            const account_name& pledger,
                            const asset& pledge,
                            const asset& deduction,
                            const string& memo ) {
      require_auth( get_self() );
      pledges ple_tbl(get_self(),pledger);
      auto pledge_inf = ple_tbl.find(pledge_name.value);
      check(pledge_inf != ple_tbl.end(),"the pledge record has exist");

      check(pledge_inf->pledge.symbol == pledge.symbol,"the pledge is not 0,can not be closed");
      check(pledge_inf->deduction.symbol == deduction.symbol,"the deduction is not 0,can not be closed");

      ple_tbl.modify( pledge_inf, name{}, [&]( pledge_info& b ) { 
         b.pledge = pledge;
         b.deduction = deduction;
      } );
   }

   void pledge::dealreward( const name& pledge_name,
                            const account_name& pledger,
                            const vector<account_name>& rewarder, 
                            const string& memo ) {
      const auto pt = _pt_tbl.find( pledge_name.value );
      check( pt != _pt_tbl.end(), "the pledge type do not exist" );
      check( rewarder.size() > 0, "the rewarder must exist" );
      const auto deduction = pt->deduction_account;
      require_auth( name{deduction} );

      pledges ple_tbl( get_self(), pledger );
      auto pledge = ple_tbl.find( pledge_name.value );
      check( pledge != ple_tbl.end(), "the pledge do not exist" );
      auto pre_allot = pledge->deduction;
      if ( pledge->pledge < asset( 0, pledge->pledge.symbol ) ) {
         pre_allot += pledge->pledge;
      }
      check( 0 < pre_allot.amount, "the quantity must be a positive number" );
      ple_tbl.modify( pledge, name{}, [&]( pledge_info& b ) { 
         b.deduction -= pre_allot;
      });

      auto report_reward = pre_allot / 2;
      auto approve_reward = pre_allot / (2 * (rewarder.size() - 1));

      auto reward_account = rewarder[0];
      reward_assign(deduction,reward_account,report_reward - approve_reward);

      auto isize = rewarder.size();
      for (const auto& reward:rewarder) {
         reward_assign(deduction,reward,approve_reward);
      }

   }

   void pledge::reward_assign(const account_name &deductor,const account_name &rewarder,const asset &quantity) {

      rewards rew_tbl( get_self(), rewarder );
      auto reward_inf = rew_tbl.find( quantity.symbol.code().raw() );
      if( reward_inf == rew_tbl.end() ) {
         rew_tbl.emplace( name{deductor}, [&]( reward_info& b ) { 
            b.reward = quantity;
         } );
      } else {
         rew_tbl.modify( reward_inf, name{}, [&]( reward_info& b ) {
            b.reward += quantity;
         } );
      }
   }


} /// namespace eosio
