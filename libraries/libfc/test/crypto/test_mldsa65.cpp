#include <boost/test/unit_test.hpp>

#include <fc/crypto/mldsa65.hpp>
#include <fc/crypto/public_key.hpp>
#include <fc/crypto/private_key.hpp>
#include <fc/crypto/signature.hpp>
#include <fc/io/datastream.hpp>
#include <fc/io/raw.hpp>

#include <openssl/mldsa.h>

#include <array>
#include <cstring>
#include <vector>
#include <string>

namespace {
constexpr char context[] = "TelosZero-QuantumLab-TX-v1";

fc::crypto::signature decode_signature(const std::vector<char>& wire) {
   fc::datastream<const char*> stream(wire.data(), wire.size());
   fc::crypto::signature result;
   fc::raw::unpack(stream, result);
   BOOST_REQUIRE_EQUAL(stream.remaining(), 0u);
   return result;
}
} // namespace

BOOST_AUTO_TEST_SUITE(mldsa65_account_signature)

BOOST_AUTO_TEST_CASE(valid_wire_and_negative_controls) {
   static_assert(MLDSA65_PUBLIC_KEY_BYTES == fc::crypto::mldsa65::public_key_bytes);
   static_assert(MLDSA65_SIGNATURE_BYTES == fc::crypto::mldsa65::signature_bytes);

   std::array<uint8_t, MLDSA65_PUBLIC_KEY_BYTES> public_bytes{};
   std::array<uint8_t, MLDSA_SEED_BYTES> seed{};
   MLDSA65_private_key private_key{};
   BOOST_REQUIRE_EQUAL(MLDSA65_generate_key(public_bytes.data(), seed.data(), &private_key), 1);

   const auto digest = fc::sha256::hash(std::string("quantum lab transaction digest"));
   const auto message = digest.to_uint8_span();
   std::array<uint8_t, MLDSA65_SIGNATURE_BYTES> signature_bytes{};
   BOOST_REQUIRE_EQUAL(
      MLDSA65_sign(signature_bytes.data(), &private_key,
                   message.data(), message.size(),
                   reinterpret_cast<const uint8_t*>(context), sizeof(context) - 1), 1);

   std::vector<char> wire(1 + public_bytes.size() + signature_bytes.size());
   wire[0] = 3;
   std::memcpy(wire.data() + 1, public_bytes.data(), public_bytes.size());
   std::memcpy(wire.data() + 1 + public_bytes.size(), signature_bytes.data(), signature_bytes.size());

   auto signature = decode_signature(wire);
   BOOST_CHECK_EQUAL(signature.which(), 3u);
   BOOST_CHECK_EQUAL(signature.variable_size(), 5261u);
   BOOST_CHECK_EQUAL(fc::raw::pack_size(signature), 5262u);
   BOOST_CHECK(fc::raw::pack(signature) == wire);

   auto public_key = fc::crypto::public_key(signature, digest);
   BOOST_CHECK_EQUAL(public_key.which(), 3u);
   BOOST_CHECK(public_key.valid());
   BOOST_CHECK_EQUAL(fc::raw::pack_size(public_key), 1953u);
   BOOST_CHECK_EQUAL(public_key.to_string({}).substr(0, 12), "PUB_MLDSA65_");
   BOOST_CHECK_EQUAL(signature.to_string({}).substr(0, 12), "SIG_MLDSA65_");
   BOOST_CHECK(fc::crypto::public_key(public_key.to_string({})) == public_key);
   BOOST_CHECK(fc::raw::pack(fc::crypto::signature(signature.to_string({}))) == wire);

   const auto wrong_digest = fc::sha256::hash(std::string("changed transaction digest"));
   BOOST_CHECK_THROW(fc::crypto::public_key(signature, wrong_digest), fc::exception);

   auto changed_signature = wire;
   changed_signature.back() ^= 1;
   BOOST_CHECK_THROW(fc::crypto::public_key(decode_signature(changed_signature), digest), fc::exception);

   auto changed_key = wire;
   changed_key[1] ^= 1;
   BOOST_CHECK_THROW(fc::crypto::public_key(decode_signature(changed_key), digest), fc::exception);

   auto truncated = wire;
   truncated.pop_back();
   fc::datastream<const char*> truncated_stream(truncated.data(), truncated.size());
   fc::crypto::signature unpacked;
   BOOST_CHECK_THROW(fc::raw::unpack(truncated_stream, unpacked), fc::exception);
}

BOOST_AUTO_TEST_CASE(seed_key_roundtrip_and_native_signing) {
   const auto digest = fc::sha256::hash(std::string("native signing transaction digest"));
   const auto key = fc::crypto::private_key::generate_mldsa65();
   const auto encoded = key.to_string({});
   BOOST_CHECK_EQUAL(encoded.substr(0, 12), "PVT_MLDSA65_");
   const fc::crypto::private_key restored(encoded);
   BOOST_CHECK(restored.get_public_key() == key.get_public_key());
   const auto signature = restored.sign(digest);
   BOOST_CHECK_EQUAL(signature.which(), 3u);
   BOOST_CHECK(fc::crypto::public_key(signature, digest) == key.get_public_key());
   BOOST_CHECK_THROW(restored.generate_shared_secret(key.get_public_key()), fc::exception);
}

BOOST_AUTO_TEST_SUITE_END()
