// Exercises the exact BoringSSL revision bundled by TelosZero Core 1.2.2.
// Standalone primitive test; this is not transaction or consensus code.
#include <openssl/bytestring.h>
#include <openssl/mldsa.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace {
constexpr char context[] = "TelosZero-QuantumLab-TX-v1";
constexpr char other_context[] = "TelosZero-QuantumLab-OTHER";
constexpr size_t context_size = sizeof(context) - 1;

int run() {
   static_assert(MLDSA65_PUBLIC_KEY_BYTES == 1952);
   static_assert(MLDSA65_SIGNATURE_BYTES == 3309);
   std::array<uint8_t, MLDSA65_PUBLIC_KEY_BYTES> encoded_public{};
   std::array<uint8_t, MLDSA_SEED_BYTES> seed{};
   MLDSA65_private_key private_key{};
   if (MLDSA65_generate_key(encoded_public.data(), seed.data(), &private_key) != 1)
      return 1;

   std::array<uint8_t, 32> digest{};
   for (size_t i = 0; i < digest.size(); ++i)
      digest[i] = static_cast<uint8_t>(i);
   std::array<uint8_t, MLDSA65_SIGNATURE_BYTES> signature{};
   if (MLDSA65_sign(signature.data(), &private_key, digest.data(), digest.size(),
                    reinterpret_cast<const uint8_t*>(context), context_size) != 1)
      return 2;

   CBS input;
   CBS_init(&input, encoded_public.data(), encoded_public.size());
   MLDSA65_public_key public_key{};
   if (MLDSA65_parse_public_key(&public_key, &input) != 1 || CBS_len(&input) != 0)
      return 3;
   if (MLDSA65_verify(&public_key, signature.data(), signature.size(),
                      digest.data(), digest.size(),
                      reinterpret_cast<const uint8_t*>(context), context_size) != 1)
      return 4;
   if (MLDSA65_verify(&public_key, signature.data(), signature.size(),
                      digest.data(), digest.size(),
                      reinterpret_cast<const uint8_t*>(other_context),
                      sizeof(other_context) - 1) != 0)
      return 5;
   digest[0] ^= 1;
   if (MLDSA65_verify(&public_key, signature.data(), signature.size(),
                      digest.data(), digest.size(),
                      reinterpret_cast<const uint8_t*>(context), context_size) != 0)
      return 6;
   digest[0] ^= 1;
   signature[0] ^= 1;
   if (MLDSA65_verify(&public_key, signature.data(), signature.size(),
                      digest.data(), digest.size(),
                      reinterpret_cast<const uint8_t*>(context), context_size) != 0)
      return 7;

   CBS_init(&input, encoded_public.data(), encoded_public.size() - 1);
   if (MLDSA65_parse_public_key(&public_key, &input) != 0)
      return 8;
   std::cout << "Pinned BoringSSL ML-DSA-65 probe passed; no chain authorization exercised.\n";
   return 0;
}
} // namespace

int main() {
   const int result = run();
   if (result != 0)
      std::cerr << "Pinned BoringSSL ML-DSA-65 probe failed at check " << result << '\n';
   return result;
}
