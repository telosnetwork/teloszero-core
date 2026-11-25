// ed25519_verify host function and the ED25519_VERIFY protocol feature.

#include <eosio/testing/tester.hpp>

#include <fc/crypto/ed25519.hpp>
#include <fc/crypto/hex.hpp>

#include <boost/test/unit_test.hpp>

using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;

namespace {

   // Calls ed25519_verify and asserts its result. Action data: i32 expected result, u32 signature size,
   // u32 public key size, then the signature, the public key and the message (the rest).
   const char ed25519_verify_wast[] = R"=====(
(module
 (import "env" "action_data_size" (func $action_data_size (result i32)))
 (import "env" "read_action_data" (func $read_action_data (param i32 i32) (result i32)))
 (import "env" "eosio_assert" (func $eosio_assert (param i32 i32)))
 (import "env" "ed25519_verify" (func $ed25519_verify (param i32 i32 i32 i32 i32 i32) (result i32)))
 (memory $0 3)
 (data (i32.const 0) "ed25519_verify returned an unexpected result\00")
 (export "apply" (func $apply))
 (func $apply (param $receiver i64) (param $account i64) (param $action i64)
  (local $size i32) (local $sig i32) (local $pub i32) (local $msg i32)
  (set_local $size (call $action_data_size))
  (drop (call $read_action_data (i32.const 64) (get_local $size)))
  (set_local $sig (i32.const 76))
  (set_local $pub (i32.add (get_local $sig) (i32.load (i32.const 68))))
  (set_local $msg (i32.add (get_local $pub) (i32.load (i32.const 72))))
  (call $eosio_assert
   (i32.eq
    (call $ed25519_verify
     (get_local $msg) (i32.sub (i32.add (i32.const 64) (get_local $size)) (get_local $msg))
     (get_local $sig) (i32.load (i32.const 68))
     (get_local $pub) (i32.load (i32.const 72)))
    (i32.load (i32.const 64)))
   (i32.const 0))
 )
)
)=====";

   const account_name verifier = "edverify"_n;

   // ed25519_verify results
   constexpr int32_t ok = 0, fail = -1;

   std::vector<char> unhex(const std::string& hex) {
      std::vector<char> bytes(hex.size() / 2);
      fc::from_hex(hex, bytes.data(), bytes.size());
      return bytes;
   }

   std::vector<char> verify_call(int32_t expected, const std::vector<char>& sig, const std::vector<char>& pub,
                                 const std::vector<char>& msg) {
      std::vector<char> data(12);
      const uint32_t sig_size = sig.size(), pub_size = pub.size();
      std::memcpy(data.data(), &expected, 4);
      std::memcpy(data.data() + 4, &sig_size, 4);
      std::memcpy(data.data() + 8, &pub_size, 4);
      data.insert(data.end(), sig.begin(), sig.end());
      data.insert(data.end(), pub.begin(), pub.end());
      data.insert(data.end(), msg.begin(), msg.end());
      return data;
   }

   template <typename T>
   void activate_ed25519_verify(T& t) {
      const auto& pfm = t.control->get_protocol_feature_manager();
      const auto d = pfm.get_builtin_digest(builtin_protocol_feature_t::ed25519_verify);
      BOOST_REQUIRE(d);
      t.preactivate_protocol_features({*d});
      t.produce_block();
   }

   template <typename T>
   auto push_verify(T& t, const std::vector<char>& data) {
      signed_transaction trx;
      trx.actions.emplace_back(vector<permission_level>{{verifier, config::active_name}}, verifier, "verify"_n, data);
      t.set_transaction_headers(trx);
      trx.sign(T::get_private_key(verifier, "active"), t.get_chain_id());
      return t.push_transaction(trx);
   }

   // RFC 8032 section 7.1, test vectors 1-3: public key, message, signature
   const std::vector<std::array<std::string, 3>> rfc8032 = {
      {"d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "",
       "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"},
      {"3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
       "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00"},
      {"fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025", "af82",
       "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a"},
   };

} // namespace

BOOST_AUTO_TEST_SUITE(ed25519_verify_tests)

// The host function cannot be imported until the protocol feature is activated.
BOOST_AUTO_TEST_CASE_TEMPLATE(ed25519_verify_needs_protocol_feature, T, testers) try {
   T t(setup_policy::preactivate_feature_and_new_bios);
   BOOST_REQUIRE(!t.control->is_builtin_activated(builtin_protocol_feature_t::ed25519_verify));
   t.create_accounts({verifier});
   t.produce_block();

   BOOST_CHECK_EXCEPTION(t.set_code(verifier, ed25519_verify_wast), wasm_exception,
                         fc_exception_message_is("env.ed25519_verify unresolveable"));

   activate_ed25519_verify(t);
   t.set_code(verifier, ed25519_verify_wast);
   t.produce_block();

   const auto& v = rfc8032[0];
   push_verify(t, verify_call(ok, unhex(v[2]), unhex(v[0]), unhex(v[1])));
   t.produce_block();
} FC_LOG_AND_RETHROW()

// setup_policy::full activates every builtin protocol feature, ED25519_VERIFY included
BOOST_AUTO_TEST_CASE_TEMPLATE(ed25519_verify_host_function, T, validating_testers) try {
   T t;
   BOOST_REQUIRE(t.control->is_builtin_activated(builtin_protocol_feature_t::ed25519_verify));
   t.create_accounts({verifier});
   t.set_code(verifier, ed25519_verify_wast);
   t.produce_block();

   auto call = [&](const std::vector<char>& data) { return push_verify(t, data); };

   for (const auto& [pub, msg, sig] : rfc8032) {
      call(verify_call(ok, unhex(sig), unhex(pub), unhex(msg)));
      auto bad = unhex(sig);
      bad[5] ^= 1;
      call(verify_call(fail, bad, unhex(pub), unhex(msg)));
      call(verify_call(fail, unhex(sig), unhex(pub), unhex(msg + "00")));
      // another vector's key
      call(verify_call(fail, unhex(sig), unhex(pub == rfc8032[0][0] ? rfc8032[1][0] : rfc8032[0][0]), unhex(msg)));
   }
   // the contract checks the result: a wrong expectation fails the action
   BOOST_CHECK_EXCEPTION(call(verify_call(fail, unhex(rfc8032[1][2]), unhex(rfc8032[1][0]), unhex(rfc8032[1][1]))),
                         eosio_assert_message_exception,
                         eosio_assert_message_is("ed25519_verify returned an unexpected result"));

   // non-canonical s: s + L for vector 1 is rejected (no malleability)
   {
      auto sig = unhex(rfc8032[0][2]);
      // L = 2^252 + 27742317777372353535851937790883648493, little endian
      const auto L = unhex("edd3f55c1a631258d69cf7a2def9de1400000000000000000000000000000010");
      unsigned carry = 0;
      for (size_t i = 0; i < 32; ++i) {
         const unsigned sum = (unsigned char)sig[32 + i] + (unsigned char)L[i] + carry;
         sig[32 + i] = char(sum & 0xff);
         carry = sum >> 8;
      }
      call(verify_call(fail, sig, unhex(rfc8032[0][0]), unhex(rfc8032[0][1])));
   }

   // small-order public key (the identity point) with a signature made of it: rejected
   call(verify_call(fail, unhex("0100000000000000000000000000000000000000000000000000000000000000"
                                "0000000000000000000000000000000000000000000000000000000000000000"),
                    unhex("0100000000000000000000000000000000000000000000000000000000000000"), unhex("72")));

   // sizes: the signature must be 64 bytes and the key 32, else -1
   const auto sig = unhex(rfc8032[2][2]), pub = unhex(rfc8032[2][0]), msg = unhex(rfc8032[2][1]);
   call(verify_call(fail, std::vector<char>(sig.begin(), sig.end() - 1), pub, msg));
   call(verify_call(fail, [&] { auto s = sig; s.push_back(0); return s; }(), pub, msg));
   call(verify_call(fail, sig, std::vector<char>(pub.begin(), pub.end() - 1), msg));
   call(verify_call(fail, sig, [&] { auto p = pub; p.push_back(0); return p; }(), msg));
   call(verify_call(fail, {}, {}, {}));

   // messages up to 64 KiB verify; a longer one is refused even with a valid signature
   const auto seed = fc::sha256::hash(std::string("ed25519_verify"));
   const auto key = fc::ed25519::public_key_from_seed(seed);
   for (size_t size : {size_t(1000), size_t(config::ed25519_verify_max_message_size),
                       size_t(config::ed25519_verify_max_message_size + 1)}) {
      const std::vector<char> long_msg(size, 'a');
      const auto long_sig = fc::ed25519::sign(seed, long_msg.data(), long_msg.size());
      BOOST_REQUIRE(fc::ed25519::verify(long_sig.data(), long_msg.data(), long_msg.size(), key.data()));
      call(verify_call(size <= config::ed25519_verify_max_message_size ? ok : fail,
                       std::vector<char>(long_sig.begin(), long_sig.end()),
                       std::vector<char>(key.begin(), key.end()), long_msg));
   }
   t.produce_block();
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
