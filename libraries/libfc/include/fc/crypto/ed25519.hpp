#pragma once
#include <fc/crypto/sha256.hpp>

#include <array>
#include <cstddef>

/**
 * Ed25519 (RFC 8032, pure Ed25519) through libsodium, vendored and pinned (libraries/libfc/libsodium).
 *
 * Verification is crypto_sign_ed25519_verify_detached: s must be canonical (s < L), the public key a canonical
 * encoding, neither R nor the public key of small order, and the equation is checked without the cofactor.
 */
namespace fc::ed25519 {
   constexpr size_t public_key_size = 32;
   constexpr size_t signature_size  = 64;

   using public_key = std::array<char, public_key_size>;
   using signature  = std::array<char, signature_size>;

   /// true if the 64-byte `sig` is a valid signature of `msg` by the 32-byte `pub`
   bool verify(const char* sig, const char* msg, size_t msg_size, const char* pub);

   /// The public key of the key pair made from a 32-byte seed. For tests and tools.
   public_key public_key_from_seed(const fc::sha256& seed);

   /// The signature of `msg` by the key pair made from a 32-byte seed. For tests and tools.
   signature sign(const fc::sha256& seed, const char* msg, size_t msg_size);
} // namespace fc::ed25519
