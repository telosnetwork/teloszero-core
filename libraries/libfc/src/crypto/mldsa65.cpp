#include <fc/crypto/mldsa65.hpp>
#include <fc/exception/exception.hpp>

#include <openssl/bytestring.h>
#include <openssl/mldsa.h>

namespace fc::crypto::mldsa65 {
namespace {

constexpr char transaction_context[] = "TelosZero-QuantumLab-TX-v1";
constexpr size_t context_length = sizeof(transaction_context) - 1;
static_assert(MLDSA65_PUBLIC_KEY_BYTES == public_key_bytes);
static_assert(MLDSA65_SIGNATURE_BYTES == signature_bytes);
static_assert(sizeof(public_key_data) == public_key_bytes);
static_assert(sizeof(signature_bytes_data) == signature_bytes);
static_assert(sizeof(signature_data) == signature_payload_bytes);
static_assert(MLDSA_SEED_BYTES == private_seed_bytes);

bool parse_public_key(const public_key_data& encoded, MLDSA65_public_key& parsed) {
   CBS input;
   CBS_init(&input, encoded.data, encoded.size());
   return MLDSA65_parse_public_key(&parsed, &input) == 1 && CBS_len(&input) == 0;
}

} // namespace

bool public_key_shim::valid() const {
   MLDSA65_public_key parsed{};
   return parse_public_key(_data, parsed);
}

public_key_shim signature_shim::recover(const fc::sha256& digest, bool check_canonical) const {
   (void)check_canonical;
   MLDSA65_public_key parsed{};
   FC_ASSERT(parse_public_key(_data.pubkey, parsed), "invalid ML-DSA-65 public key");
   const auto message = digest.to_uint8_span();
   FC_ASSERT(MLDSA65_verify(
      &parsed, _data.sig.data, _data.sig.size(),
      message.data(), message.size(),
      reinterpret_cast<const uint8_t*>(transaction_context), context_length) == 1,
      "invalid ML-DSA-65 transaction signature");
   return public_key_shim(_data.pubkey);
}

private_key_shim private_key_shim::generate() {
   public_key_data public_key;
   private_seed_data seed;
   MLDSA65_private_key expanded{};
   FC_ASSERT(MLDSA65_generate_key(public_key.data, seed.data, &expanded) == 1,
             "ML-DSA-65 key generation failed");
   return private_key_shim(seed);
}

public_key_shim private_key_shim::get_public_key() const {
   MLDSA65_private_key expanded{};
   MLDSA65_public_key public_key{};
   FC_ASSERT(MLDSA65_private_key_from_seed(&expanded, _data.data, _data.size()) == 1,
             "ML-DSA-65 seed expansion failed");
   FC_ASSERT(MLDSA65_public_from_private(&public_key, &expanded) == 1,
             "ML-DSA-65 public key derivation failed");
   public_key_data encoded;
   CBB out;
   FC_ASSERT(CBB_init_fixed(&out, encoded.data, encoded.size()) == 1,
             "ML-DSA-65 public key buffer initialization failed");
   const bool encoded_ok = MLDSA65_marshal_public_key(&out, &public_key) == 1 &&
                           CBB_len(&out) == encoded.size();
   CBB_cleanup(&out);
   FC_ASSERT(encoded_ok, "ML-DSA-65 public key encoding failed");
   return public_key_shim(encoded);
}

signature_shim private_key_shim::sign(const fc::sha256& digest, bool require_canonical) const {
   (void)require_canonical;
   MLDSA65_private_key expanded{};
   FC_ASSERT(MLDSA65_private_key_from_seed(&expanded, _data.data, _data.size()) == 1,
             "ML-DSA-65 seed expansion failed");
   signature_data encoded;
   encoded.pubkey = get_public_key()._data;
   const auto message = digest.to_uint8_span();
   FC_ASSERT(MLDSA65_sign(
      encoded.sig.data, &expanded, message.data(), message.size(),
      reinterpret_cast<const uint8_t*>(transaction_context), context_length) == 1,
      "ML-DSA-65 signing failed");
   return signature_shim(encoded);
}

} // namespace fc::crypto::mldsa65
