#include <fc/crypto/ed25519.hpp>
#include <fc/exception/exception.hpp>

#include <sodium.h>

namespace fc::ed25519 {

   static_assert(crypto_sign_ed25519_PUBLICKEYBYTES == public_key_size);
   static_assert(crypto_sign_ed25519_BYTES == signature_size);
   static_assert(crypto_sign_ed25519_SEEDBYTES == sizeof(fc::sha256));

   namespace {
      // sodium_init is thread safe and idempotent. Ed25519 itself needs no runtime setup; this is for the rest of
      // the library, called once before the first use.
      void ensure_sodium() {
         static const int result = sodium_init();
         FC_ASSERT(result >= 0, "libsodium failed to initialize");
      }

      struct key_pair {
         public_key    pub;
         unsigned char secret[crypto_sign_ed25519_SECRETKEYBYTES];
      };

      key_pair from_seed(const fc::sha256& seed) {
         ensure_sodium();
         key_pair kp;
         FC_ASSERT(crypto_sign_ed25519_seed_keypair((unsigned char*)kp.pub.data(), kp.secret,
                                                    (const unsigned char*)seed.data()) == 0);
         return kp;
      }
   } // namespace

   bool verify(const char* sig, const char* msg, size_t msg_size, const char* pub) {
      ensure_sodium();
      return crypto_sign_ed25519_verify_detached((const unsigned char*)sig, (const unsigned char*)msg, msg_size,
                                                 (const unsigned char*)pub) == 0;
   }

   public_key public_key_from_seed(const fc::sha256& seed) {
      return from_seed(seed).pub;
   }

   signature sign(const fc::sha256& seed, const char* msg, size_t msg_size) {
      auto kp = from_seed(seed);
      signature sig;
      FC_ASSERT(crypto_sign_ed25519_detached((unsigned char*)sig.data(), nullptr, (const unsigned char*)msg, msg_size,
                                             kp.secret) == 0);
      sodium_memzero(kp.secret, sizeof(kp.secret));
      return sig;
   }

} // namespace fc::ed25519
