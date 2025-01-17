/**
 *  @file
 *  @copyright defined in bitconch/LICENSE.txt
 */
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <bitconchio/testing/tester.hpp>
#include <bitconchio/chain/abi_serializer.hpp>
#include <bitconchio/chain/wasm_bitconchio_constraints.hpp>
#include <bitconchio/chain/resource_limits.hpp>
#include <bitconchio/chain/exceptions.hpp>
#include <bitconchio/chain/wast_to_wasm.hpp>
#include <bitconchio/chain_plugin/chain_plugin.hpp>

#include <contracts.hpp>

#include <fc/io/fstream.hpp>

#include <Runtime/Runtime.h>

#include <fc/variant_object.hpp>
#include <fc/io/json.hpp>

#include <array>
#include <utility>

#ifdef NON_VALIDATING_TEST
#define TESTER tester
#else
#define TESTER validating_tester
#endif

using namespace bitconchio;
using namespace bitconchio::chain;
using namespace bitconchio::testing;
using namespace fc;

BOOST_AUTO_TEST_SUITE(get_table_tests)

transaction_trace_ptr
issue_tokens( TESTER& t, account_name issuer, account_name to, const asset& amount,
              std::string memo = "", account_name token_contract = N(bccio.token) )
{
   signed_transaction trx;

   trx.actions.emplace_back( t.get_action( token_contract, N(issue),
                                           vector<permission_level>{{issuer, config::active_name}},
                                           mutable_variant_object()
                                             ("to", issuer.to_string())
                                             ("quantity", amount)
                                             ("memo", memo)
   ) );

   trx.actions.emplace_back( t.get_action( token_contract, N(transfer),
                                           vector<permission_level>{{issuer, config::active_name}},
                                           mutable_variant_object()
                                             ("from", issuer.to_string())
                                             ("to", to.to_string())
                                             ("quantity", amount)
                                             ("memo", memo)
   ) );

   t.set_transaction_headers(trx);
   trx.sign( t.get_private_key( issuer, "active" ), t.control->get_chain_id()  );
   return t.push_transaction( trx );
}

BOOST_FIXTURE_TEST_CASE( get_scope_test, TESTER ) try {
   produce_blocks(2);

   create_accounts({ N(bccio.token), N(bccio.ram), N(bccio.ramfee), N(bccio.stake),
      N(bccio.bpay), N(bccio.vpay), N(bccio.saving), N(bccio.names) });

   std::vector<account_name> accs{N(inita), N(initb), N(initc), N(initd)};
   create_accounts(accs);
   produce_block();

   set_code( N(bccio.token), contracts::bitconchio_token_wasm() );
   set_abi( N(bccio.token), contracts::bitconchio_token_abi().data() );
   produce_blocks(1);

   // create currency
   auto act = mutable_variant_object()
         ("issuer",       "bccio")
         ("maximum_supply", bitconchio::chain::asset::from_string("1000000000.0000 SYS"));
   push_action(N(bccio.token), N(create), N(bccio.token), act );

   // issue
   for (account_name a: accs) {
      issue_tokens( *this, config::system_account_name, a, bitconchio::chain::asset::from_string("999.0000 SYS") );
   }
   produce_blocks(1);

   // iterate over scope
   bitconchio::chain_apis::read_only plugin(*(this->control), fc::microseconds::maximum());
   bitconchio::chain_apis::read_only::get_table_by_scope_params param{N(bccio.token), N(accounts), "inita", "", 10};
   bitconchio::chain_apis::read_only::get_table_by_scope_result result = plugin.read_only::get_table_by_scope(param);

   BOOST_REQUIRE_EQUAL(4u, result.rows.size());
   BOOST_REQUIRE_EQUAL("", result.more);
   if (result.rows.size() >= 4) {
      BOOST_REQUIRE_EQUAL(name(N(bccio.token)), result.rows[0].code);
      BOOST_REQUIRE_EQUAL(name(N(inita)), result.rows[0].scope);
      BOOST_REQUIRE_EQUAL(name(N(accounts)), result.rows[0].table);
      BOOST_REQUIRE_EQUAL(name(N(bccio)), result.rows[0].payer);
      BOOST_REQUIRE_EQUAL(1u, result.rows[0].count);

      BOOST_REQUIRE_EQUAL(name(N(initb)), result.rows[1].scope);
      BOOST_REQUIRE_EQUAL(name(N(initc)), result.rows[2].scope);
      BOOST_REQUIRE_EQUAL(name(N(initd)), result.rows[3].scope);
   }

   param.lower_bound = "initb";
   param.upper_bound = "initc";
   result = plugin.read_only::get_table_by_scope(param);
   BOOST_REQUIRE_EQUAL(2u, result.rows.size());
   BOOST_REQUIRE_EQUAL("", result.more);
   if (result.rows.size() >= 2) {
      BOOST_REQUIRE_EQUAL(name(N(initb)), result.rows[0].scope);
      BOOST_REQUIRE_EQUAL(name(N(initc)), result.rows[1].scope);
   }

   param.limit = 1;
   result = plugin.read_only::get_table_by_scope(param);
   BOOST_REQUIRE_EQUAL(1u, result.rows.size());
   BOOST_REQUIRE_EQUAL("initc", result.more);

   param.table = name(0);
   result = plugin.read_only::get_table_by_scope(param);
   BOOST_REQUIRE_EQUAL(1u, result.rows.size());
   BOOST_REQUIRE_EQUAL("initc", result.more);

   param.table = N(invalid);
   result = plugin.read_only::get_table_by_scope(param);
   BOOST_REQUIRE_EQUAL(0u, result.rows.size());
   BOOST_REQUIRE_EQUAL("", result.more);

} FC_LOG_AND_RETHROW() /// get_scope_test

BOOST_FIXTURE_TEST_CASE( get_table_test, TESTER ) try {
   produce_blocks(2);

   create_accounts({ N(bccio.token), N(bccio.ram), N(bccio.ramfee), N(bccio.stake),
      N(bccio.bpay), N(bccio.vpay), N(bccio.saving), N(bccio.names) });

   std::vector<account_name> accs{N(inita), N(initb)};
   create_accounts(accs);
   produce_block();

   set_code( N(bccio.token), contracts::bitconchio_token_wasm() );
   set_abi( N(bccio.token), contracts::bitconchio_token_abi().data() );
   produce_blocks(1);

   // create currency
   auto act = mutable_variant_object()
         ("issuer",       "bccio")
         ("maximum_supply", bitconchio::chain::asset::from_string("1000000000.0000 SYS"));
   push_action(N(bccio.token), N(create), N(bccio.token), act );

   // issue
   for (account_name a: accs) {
      issue_tokens( *this, config::system_account_name, a, bitconchio::chain::asset::from_string("10000.0000 SYS") );
   }
   produce_blocks(1);

   // create currency 2
   act = mutable_variant_object()
         ("issuer",       "bccio")
         ("maximum_supply", bitconchio::chain::asset::from_string("1000000000.0000 AAA"));
   push_action(N(bccio.token), N(create), N(bccio.token), act );
   // issue
   for (account_name a: accs) {
      issue_tokens( *this, config::system_account_name, a, bitconchio::chain::asset::from_string("9999.0000 AAA") );
   }
   produce_blocks(1);

   // create currency 3
   act = mutable_variant_object()
         ("issuer",       "bccio")
         ("maximum_supply", bitconchio::chain::asset::from_string("1000000000.0000 CCC"));
   push_action(N(bccio.token), N(create), N(bccio.token), act );
   // issue
   for (account_name a: accs) {
      issue_tokens( *this, config::system_account_name, a, bitconchio::chain::asset::from_string("7777.0000 CCC") );
   }
   produce_blocks(1);

   // create currency 3
   act = mutable_variant_object()
         ("issuer",       "bccio")
         ("maximum_supply", bitconchio::chain::asset::from_string("1000000000.0000 BBB"));
   push_action(N(bccio.token), N(create), N(bccio.token), act );
   // issue
   for (account_name a: accs) {
      issue_tokens( *this, config::system_account_name, a, bitconchio::chain::asset::from_string("8888.0000 BBB") );
   }
   produce_blocks(1);

   // get table: normal case
   bitconchio::chain_apis::read_only plugin(*(this->control), fc::microseconds::maximum());
   bitconchio::chain_apis::read_only::get_table_rows_params p;
   p.code = N(bccio.token);
   p.scope = "inita";
   p.table = N(accounts);
   p.json = true;
   p.index_position = "primary";
   bitconchio::chain_apis::read_only::get_table_rows_result result = plugin.read_only::get_table_rows(p);
   BOOST_REQUIRE_EQUAL(4u, result.rows.size());
   BOOST_REQUIRE_EQUAL(false, result.more);
   if (result.rows.size() >= 4) {
      BOOST_REQUIRE_EQUAL("9999.0000 AAA", result.rows[0]["balance"].as_string());
      BOOST_REQUIRE_EQUAL("8888.0000 BBB", result.rows[1]["balance"].as_string());
      BOOST_REQUIRE_EQUAL("7777.0000 CCC", result.rows[2]["balance"].as_string());
      BOOST_REQUIRE_EQUAL("10000.0000 SYS", result.rows[3]["balance"].as_string());
   }

   // get table: reverse ordered
   p.reverse = true;
   result = plugin.read_only::get_table_rows(p);
   BOOST_REQUIRE_EQUAL(4u, result.rows.size());
   BOOST_REQUIRE_EQUAL(false, result.more);
   if (result.rows.size() >= 4) {
      BOOST_REQUIRE_EQUAL("9999.0000 AAA", result.rows[3]["balance"].as_string());
      BOOST_REQUIRE_EQUAL("8888.0000 BBB", result.rows[2]["balance"].as_string());
      BOOST_REQUIRE_EQUAL("7777.0000 CCC", result.rows[1]["balance"].as_string());
      BOOST_REQUIRE_EQUAL("10000.0000 SYS", result.rows[0]["balance"].as_string());
   }

   // get table: reverse ordered, with ram payer
   p.reverse = true;
   p.show_payer = true;
   result = plugin.read_only::get_table_rows(p);
   BOOST_REQUIRE_EQUAL(4u, result.rows.size());
   BOOST_REQUIRE_EQUAL(false, result.more);
   if (result.rows.size() >= 4) {
      BOOST_REQUIRE_EQUAL("9999.0000 AAA", result.rows[3]["data"]["balance"].as_string());
      BOOST_REQUIRE_EQUAL("8888.0000 BBB", result.rows[2]["data"]["balance"].as_string());
      BOOST_REQUIRE_EQUAL("7777.0000 CCC", result.rows[1]["data"]["balance"].as_string());
      BOOST_REQUIRE_EQUAL("10000.0000 SYS", result.rows[0]["data"]["balance"].as_string());
      BOOST_REQUIRE_EQUAL("bccio", result.rows[0]["payer"].as_string());
      BOOST_REQUIRE_EQUAL("bccio", result.rows[1]["payer"].as_string());
      BOOST_REQUIRE_EQUAL("bccio", result.rows[2]["payer"].as_string());
      BOOST_REQUIRE_EQUAL("bccio", result.rows[3]["payer"].as_string());
   }
   p.show_payer = false;

   // get table: normal case, with bound
   p.lower_bound = "BBB";
   p.upper_bound = "CCC";
   p.reverse = false;
   result = plugin.read_only::get_table_rows(p);
   BOOST_REQUIRE_EQUAL(2u, result.rows.size());
   BOOST_REQUIRE_EQUAL(false, result.more);
   if (result.rows.size() >= 2) {
      BOOST_REQUIRE_EQUAL("8888.0000 BBB", result.rows[0]["balance"].as_string());
      BOOST_REQUIRE_EQUAL("7777.0000 CCC", result.rows[1]["balance"].as_string());
   }

   // get table: reverse case, with bound
   p.lower_bound = "BBB";
   p.upper_bound = "CCC";
   p.reverse = true;
   result = plugin.read_only::get_table_rows(p);
   BOOST_REQUIRE_EQUAL(2u, result.rows.size());
   BOOST_REQUIRE_EQUAL(false, result.more);
   if (result.rows.size() >= 2) {
      BOOST_REQUIRE_EQUAL("8888.0000 BBB", result.rows[1]["balance"].as_string());
      BOOST_REQUIRE_EQUAL("7777.0000 CCC", result.rows[0]["balance"].as_string());
   }

   // get table: normal case, with limit
   p.lower_bound = p.upper_bound = "";
   p.limit = 1;
   p.reverse = false;
   result = plugin.read_only::get_table_rows(p);
   BOOST_REQUIRE_EQUAL(1u, result.rows.size());
   BOOST_REQUIRE_EQUAL(true, result.more);
   if (result.rows.size() >= 1) {
      BOOST_REQUIRE_EQUAL("9999.0000 AAA", result.rows[0]["balance"].as_string());
   }

   // get table: reverse case, with limit
   p.lower_bound = p.upper_bound = "";
   p.limit = 1;
   p.reverse = true;
   result = plugin.read_only::get_table_rows(p);
   BOOST_REQUIRE_EQUAL(1u, result.rows.size());
   BOOST_REQUIRE_EQUAL(true, result.more);
   if (result.rows.size() >= 1) {
      BOOST_REQUIRE_EQUAL("10000.0000 SYS", result.rows[0]["balance"].as_string());
   }

   // get table: normal case, with bound & limit
   p.lower_bound = "BBB";
   p.upper_bound = "CCC";
   p.limit = 1;
   p.reverse = false;
   result = plugin.read_only::get_table_rows(p);
   BOOST_REQUIRE_EQUAL(1u, result.rows.size());
   BOOST_REQUIRE_EQUAL(true, result.more);
   if (result.rows.size() >= 1) {
      BOOST_REQUIRE_EQUAL("8888.0000 BBB", result.rows[0]["balance"].as_string());
   }

   // get table: reverse case, with bound & limit
   p.lower_bound = "BBB";
   p.upper_bound = "CCC";
   p.limit = 1;
   p.reverse = true;
   result = plugin.read_only::get_table_rows(p);
   BOOST_REQUIRE_EQUAL(1u, result.rows.size());
   BOOST_REQUIRE_EQUAL(true, result.more);
   if (result.rows.size() >= 1) {
      BOOST_REQUIRE_EQUAL("7777.0000 CCC", result.rows[0]["balance"].as_string());
   }

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( get_table_by_seckey_test, TESTER ) try {
   produce_blocks(2);

   create_accounts({ N(bccio.token), N(bccio.ram), N(bccio.ramfee), N(bccio.stake),
      N(bccio.bpay), N(bccio.vpay), N(bccio.saving), N(bccio.names) });

   std::vector<account_name> accs{N(inita), N(initb), N(initc), N(initd)};
   create_accounts(accs);
   produce_block();

   set_code( N(bccio.token), contracts::bitconchio_token_wasm() );
   set_abi( N(bccio.token), contracts::bitconchio_token_abi().data() );
   produce_blocks(1);

   // create currency
   auto act = mutable_variant_object()
         ("issuer",       "bccio")
         ("maximum_supply", bitconchio::chain::asset::from_string("1000000000.0000 SYS"));
   push_action(N(bccio.token), N(create), N(bccio.token), act );

   // issue
   for (account_name a: accs) {
      issue_tokens( *this, config::system_account_name, a, bitconchio::chain::asset::from_string("10000.0000 SYS") );
   }
   produce_blocks(1);

   set_code( config::system_account_name, contracts::bitconchio_system_wasm() );
   set_abi( config::system_account_name, contracts::bitconchio_system_abi().data() );

   base_tester::push_action(config::system_account_name, N(init),
                            config::system_account_name,  mutable_variant_object()
                            ("version", 0)
                            ("core", CORE_SYM_STR));

   // bidname
   auto bidname = [this]( const account_name& bidder, const account_name& newname, const asset& bid ) {
      return push_action( N(bccio), N(bidname), bidder, fc::mutable_variant_object()
                          ("bidder",  bidder)
                          ("newname", newname)
                          ("bid", bid)
                          );
   };

   bidname(N(inita), N(com), bitconchio::chain::asset::from_string("10.0000 SYS"));
   bidname(N(initb), N(org), bitconchio::chain::asset::from_string("11.0000 SYS"));
   bidname(N(initc), N(io), bitconchio::chain::asset::from_string("12.0000 SYS"));
   bidname(N(initd), N(html), bitconchio::chain::asset::from_string("14.0000 SYS"));
   produce_blocks(1);

   // get table: normal case
   bitconchio::chain_apis::read_only plugin(*(this->control), fc::microseconds::maximum());
   bitconchio::chain_apis::read_only::get_table_rows_params p;
   p.code = N(bccio);
   p.scope = "bccio";
   p.table = N(namebids);
   p.json = true;
   p.index_position = "secondary"; // ordered by high_bid
   p.key_type = "i64";
   bitconchio::chain_apis::read_only::get_table_rows_result result = plugin.read_only::get_table_rows(p);
   BOOST_REQUIRE_EQUAL(4u, result.rows.size());
   BOOST_REQUIRE_EQUAL(false, result.more);
   if (result.rows.size() >= 4) {
      BOOST_REQUIRE_EQUAL("html", result.rows[0]["newname"].as_string());
      BOOST_REQUIRE_EQUAL("initd", result.rows[0]["high_bidder"].as_string());
      BOOST_REQUIRE_EQUAL("140000", result.rows[0]["high_bid"].as_string());

      BOOST_REQUIRE_EQUAL("io", result.rows[1]["newname"].as_string());
      BOOST_REQUIRE_EQUAL("initc", result.rows[1]["high_bidder"].as_string());
      BOOST_REQUIRE_EQUAL("120000", result.rows[1]["high_bid"].as_string());

      BOOST_REQUIRE_EQUAL("org", result.rows[2]["newname"].as_string());
      BOOST_REQUIRE_EQUAL("initb", result.rows[2]["high_bidder"].as_string());
      BOOST_REQUIRE_EQUAL("110000", result.rows[2]["high_bid"].as_string());

      BOOST_REQUIRE_EQUAL("com", result.rows[3]["newname"].as_string());
      BOOST_REQUIRE_EQUAL("inita", result.rows[3]["high_bidder"].as_string());
      BOOST_REQUIRE_EQUAL("100000", result.rows[3]["high_bid"].as_string());
   }

   // reverse search, with show ram payer
   p.reverse = true;
   p.show_payer = true;
   result = plugin.read_only::get_table_rows(p);
   BOOST_REQUIRE_EQUAL(4u, result.rows.size());
   BOOST_REQUIRE_EQUAL(false, result.more);
   if (result.rows.size() >= 4) {
      BOOST_REQUIRE_EQUAL("html", result.rows[3]["data"]["newname"].as_string());
      BOOST_REQUIRE_EQUAL("initd", result.rows[3]["data"]["high_bidder"].as_string());
      BOOST_REQUIRE_EQUAL("140000", result.rows[3]["data"]["high_bid"].as_string());
      BOOST_REQUIRE_EQUAL("initd", result.rows[3]["payer"].as_string());

      BOOST_REQUIRE_EQUAL("io", result.rows[2]["data"]["newname"].as_string());
      BOOST_REQUIRE_EQUAL("initc", result.rows[2]["data"]["high_bidder"].as_string());
      BOOST_REQUIRE_EQUAL("120000", result.rows[2]["data"]["high_bid"].as_string());
      BOOST_REQUIRE_EQUAL("initc", result.rows[2]["payer"].as_string());

      BOOST_REQUIRE_EQUAL("org", result.rows[1]["data"]["newname"].as_string());
      BOOST_REQUIRE_EQUAL("initb", result.rows[1]["data"]["high_bidder"].as_string());
      BOOST_REQUIRE_EQUAL("110000", result.rows[1]["data"]["high_bid"].as_string());
      BOOST_REQUIRE_EQUAL("initb", result.rows[1]["payer"].as_string());

      BOOST_REQUIRE_EQUAL("com", result.rows[0]["data"]["newname"].as_string());
      BOOST_REQUIRE_EQUAL("inita", result.rows[0]["data"]["high_bidder"].as_string());
      BOOST_REQUIRE_EQUAL("100000", result.rows[0]["data"]["high_bid"].as_string());
      BOOST_REQUIRE_EQUAL("inita", result.rows[0]["payer"].as_string());
   }

   // limit to 1 (get the highest bidname)
   p.reverse = false;
   p.show_payer = false;
   p.limit = 1;
   result = plugin.read_only::get_table_rows(p);
   BOOST_REQUIRE_EQUAL(1u, result.rows.size());
   BOOST_REQUIRE_EQUAL(true, result.more);
   if (result.rows.size() >= 1) {
      BOOST_REQUIRE_EQUAL("html", result.rows[0]["newname"].as_string());
      BOOST_REQUIRE_EQUAL("initd", result.rows[0]["high_bidder"].as_string());
      BOOST_REQUIRE_EQUAL("140000", result.rows[0]["high_bid"].as_string());
   }

   // limit to 1 reverse, (get the lowest bidname)
   p.reverse = true;
   p.show_payer = false;
   p.limit = 1;
   result = plugin.read_only::get_table_rows(p);
   BOOST_REQUIRE_EQUAL(1u, result.rows.size());
   BOOST_REQUIRE_EQUAL(true, result.more);
   if (result.rows.size() >= 1) {
      BOOST_REQUIRE_EQUAL("com", result.rows[0]["newname"].as_string());
      BOOST_REQUIRE_EQUAL("inita", result.rows[0]["high_bidder"].as_string());
      BOOST_REQUIRE_EQUAL("100000", result.rows[0]["high_bid"].as_string());
   }

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
