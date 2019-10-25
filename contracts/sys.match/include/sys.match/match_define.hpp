#ifndef CODEX_INCLUED_SYS_MATCH_DEFINES_HPP_
#define CODEX_INCLUED_SYS_MATCH_DEFINES_HPP_
#pragma once

#include <string>
#include <vector>

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosforce/assetage.hpp>

namespace match{

   using std::string;
   using std::vector;

   static constexpr auto match_order = "m.order"_n;
   static constexpr auto match_deposit = "m.deposit"_n;
   static constexpr auto match_donate = "m.donate"_n;

   uint64_t char_to_symbol( char c ) {
      if( c >= 'a' && c <= 'z' )
         return (c - 'a') + 6;
      if( c >= '1' && c <= '5' )
         return (c - '1') + 1;
      return 0;
   }

   uint64_t string_to_name( const char* str ) {
      uint64_t name = 0;
      int i = 0;
      for ( ; str[i] && i < 12; ++i) {
         name |= (char_to_symbol(str[i]) & 0x1f) << (64 - 5 * (i + 1));
      }

      if (i == 12)
         name |= char_to_symbol(str[12]) & 0x0F;
      return name;
   }

   void splitMemo( std::vector<std::string>& results, const std::string& memo, char separator ) {
      auto start = memo.cbegin();
      auto end = memo.cend();

      for( auto it = start; it != end; ++it ) {
         if( *it == separator ) {
            results.emplace_back(start, it);
            start = it + 1;
         }
      }
      if (start != end) results.emplace_back(start, end);
   }



   inline std::string trim(const std::string str) {
      auto s = str;
      s.erase(s.find_last_not_of(" ") + 1);
      s.erase(0, s.find_first_not_of(" "));
      
      return s;
   }

   constexpr uint64_t string_to_symbol_c(uint8_t precision, const char* str) {
      uint32_t len = 0;
      while (str[len]) ++len;

      uint64_t result = 0;
      // No validation is done at compile time
      for (uint32_t i = 0; i < len; ++i) {
         result |= (uint64_t(str[i]) << (8*(1+i)));
      }

      result |= uint64_t(precision);
      return result;
   }
   
   asset asset_from_string(const std::string& from) {

      std::string s = trim(from);
      const char * str1 = s.c_str();
   
      // Find space in order to split amount and symbol
      const char * pos = strchr(str1, ' ');
      check((pos != NULL), "Asset's amount and symbol should be separated with space");
      auto space_pos = pos - str1;
      auto symbol_str = trim(s.substr(space_pos + 1));
      auto amount_str = s.substr(0, space_pos);
      check((amount_str[0] != '-'), "now do not support negetive asset");
   
      // Ensure that if decimal point is used (.), decimal fraction is specified
      const char * str2 = amount_str.c_str();
      const char *dot_pos = strchr(str2, '.');
      if (dot_pos != NULL) {
         check((dot_pos - str2 != amount_str.size() - 1), "Missing decimal fraction after decimal point");
      }

      uint32_t precision_digits;
      if (dot_pos != NULL) {
         precision_digits = amount_str.size() - (dot_pos - str2 + 1);
      } else {
         precision_digits = 0;
      }
   
      eosio::symbol sym(string_to_symbol_c((uint8_t)precision_digits, symbol_str.c_str()));
     
      // Parse amount
      int64_t int_part, fract_part;
      if (dot_pos != NULL) {
         int_part = ::atoll(amount_str.substr(0, dot_pos - str2).c_str());
         fract_part = ::atoll(amount_str.substr(dot_pos - str2 + 1).c_str());
      } else {
         int_part = ::atoll(amount_str.c_str());
         fract_part = 0;
      }
      int64_t amount = int_part * eosforce::utils::precision_base(precision_digits);
      amount += fract_part;
   
      return eosio::asset(amount, sym);
   }

//auto a = asset::from_string( s );
   struct transfer_memo{
      name type;
      asset dest;
      name pair_name;
      account_name exc_acc;
      explicit transfer_memo( const std::string& memo ) {
         std::vector<std::string> memoParts;
         memoParts.reserve( 8 );
         splitMemo( memoParts, memo, ';' );

         this->type = name{ string_to_name( memoParts[0].c_str() ) };
         if ( this->type == match_order ) {
            this->dest = asset_from_string(memoParts[1]);
            this->pair_name = name{string_to_name( memoParts[2].c_str() )};
            this->exc_acc = string_to_name(memoParts[3].c_str());
         }
         else if ( this->type == match_deposit ) {
            this->dest = asset_from_string(memoParts[1]);
         }
         else if ( this->type == match_donate ) {

         }
         else {
            check(false,"wrong transfer memo");
         }

      }
   };
}


#endif 