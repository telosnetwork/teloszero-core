#pragma once

#include <fc/array.hpp>
#include <fc/crypto/common.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/reflect/reflect.hpp>

namespace fc::crypto::mldsa65 {

constexpr size_t public_key_bytes = 1952;
constexpr size_t signature_bytes = 3309;
constexpr size_t signature_payload_bytes = public_key_bytes + signature_bytes;
constexpr size_t private_seed_bytes = 32;

using public_key_data = fc::array<unsigned char, public_key_bytes>;
using signature_bytes_data = fc::array<unsigned char, signature_bytes>;
using private_seed_data = fc::array<unsigned char, private_seed_bytes>;

// Fixed arrays keep the variant-3 wire payload exactly 1952 + 3309 bytes.
struct signature_data {
   public_key_data pubkey;
   signature_bytes_data sig;

   bool operator==(const signature_data&) const = default;
   bool operator<(const signature_data& other) const {
      if (pubkey == other.pubkey)
         return sig < other.sig;
      return pubkey < other.pubkey;
   }
};

struct public_key_shim : fc::crypto::shim<public_key_data> {
   using fc::crypto::shim<public_key_data>::shim;
   bool valid() const;
};

struct signature_shim : fc::crypto::shim<signature_data> {
   using public_key_type = public_key_shim;
   using fc::crypto::shim<signature_data>::shim;
   public_key_type recover(const fc::sha256& digest, bool check_canonical) const;
};

struct private_key_shim : fc::crypto::shim<private_seed_data> {
   using public_key_type = public_key_shim;
   using fc::crypto::shim<private_seed_data>::shim;
   static private_key_shim generate();
   public_key_type get_public_key() const;
   signature_shim sign(const fc::sha256& digest, bool require_canonical) const;
};

} // namespace fc::crypto::mldsa65

FC_REFLECT(fc::crypto::mldsa65::signature_data, (pubkey)(sig))
FC_REFLECT_DERIVED(fc::crypto::mldsa65::public_key_shim,
                   (fc::crypto::shim<fc::crypto::mldsa65::public_key_data>),
                   BOOST_PP_SEQ_NIL)
FC_REFLECT_DERIVED(fc::crypto::mldsa65::signature_shim,
                   (fc::crypto::shim<fc::crypto::mldsa65::signature_data>),
                   BOOST_PP_SEQ_NIL)
FC_REFLECT_DERIVED(fc::crypto::mldsa65::private_key_shim,
                   (fc::crypto::shim<fc::crypto::mldsa65::private_seed_data>),
                   BOOST_PP_SEQ_NIL)
