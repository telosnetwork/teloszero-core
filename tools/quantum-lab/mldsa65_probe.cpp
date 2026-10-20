// Standalone ML-DSA-65 envelope experiment. Requires OpenSSL >= 3.5.
// This is not TelosZero consensus code and never writes private key material.
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
constexpr char context[] = "TelosZero-QuantumLab-TX-v1";
constexpr char other_context[] = "TelosZero-QuantumLab-OTHER";

template <typename T, void (*Free)(T*)>
using owner = std::unique_ptr<T, decltype(Free)>;

void require(bool condition, const char* message) {
   if (!condition) throw std::runtime_error(message);
}

OSSL_PARAM context_parameter(const char* value) {
   return OSSL_PARAM_construct_octet_string(
      OSSL_SIGNATURE_PARAM_CONTEXT_STRING,
      const_cast<char*>(value), std::strlen(value));
}

bool verify(EVP_PKEY* public_key, const std::vector<unsigned char>& signature,
            const std::array<unsigned char, 32>& digest, const char* domain) {
   owner<EVP_PKEY_CTX, EVP_PKEY_CTX_free> context_handle(
      EVP_PKEY_CTX_new_from_pkey(nullptr, public_key, nullptr), EVP_PKEY_CTX_free);
   owner<EVP_SIGNATURE, EVP_SIGNATURE_free> algorithm(
      EVP_SIGNATURE_fetch(nullptr, "ML-DSA-65", nullptr), EVP_SIGNATURE_free);
   require(context_handle && algorithm, "ML-DSA verifier unavailable");
   OSSL_PARAM parameters[] = {context_parameter(domain), OSSL_PARAM_construct_end()};
   require(EVP_PKEY_verify_message_init(context_handle.get(), algorithm.get(), parameters) == 1,
           "ML-DSA verifier initialization failed");
   return EVP_PKEY_verify(context_handle.get(), signature.data(), signature.size(),
                          digest.data(), digest.size()) == 1;
}
} // namespace

int main() {
   try {
      owner<EVP_PKEY_CTX, EVP_PKEY_CTX_free> generator(
         EVP_PKEY_CTX_new_from_name(nullptr, "ML-DSA-65", nullptr), EVP_PKEY_CTX_free);
      require(generator && EVP_PKEY_keygen_init(generator.get()) == 1,
              "ML-DSA-65 keygen unavailable");
      EVP_PKEY* generated = nullptr;
      require(EVP_PKEY_keygen(generator.get(), &generated) == 1, "keygen failed");
      owner<EVP_PKEY, EVP_PKEY_free> key(generated, EVP_PKEY_free);

      size_t public_size = 0;
      require(EVP_PKEY_get_octet_string_param(key.get(), OSSL_PKEY_PARAM_PUB_KEY,
                                               nullptr, 0, &public_size) == 1,
              "public-key size lookup failed");
      std::vector<unsigned char> public_bytes(public_size);
      require(EVP_PKEY_get_octet_string_param(key.get(), OSSL_PKEY_PARAM_PUB_KEY,
                                               public_bytes.data(), public_bytes.size(),
                                               &public_size) == 1,
              "public-key export failed");
      public_bytes.resize(public_size);
      require(public_bytes.size() == 1952, "unexpected ML-DSA-65 public-key size");

      owner<EVP_PKEY_CTX, EVP_PKEY_CTX_free> importer(
         EVP_PKEY_CTX_new_from_name(nullptr, "ML-DSA-65", nullptr), EVP_PKEY_CTX_free);
      require(importer && EVP_PKEY_fromdata_init(importer.get()) == 1,
              "public-key importer unavailable");
      OSSL_PARAM import_parameters[] = {
         OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                                           public_bytes.data(), public_bytes.size()),
         OSSL_PARAM_construct_end()
      };
      EVP_PKEY* imported = nullptr;
      require(EVP_PKEY_fromdata(importer.get(), &imported, EVP_PKEY_PUBLIC_KEY,
                                import_parameters) == 1, "public-key import failed");
      owner<EVP_PKEY, EVP_PKEY_free> public_only(imported, EVP_PKEY_free);

      std::array<unsigned char, 32> digest{};
      for (size_t i = 0; i < digest.size(); ++i) digest[i] = static_cast<unsigned char>(i);
      owner<EVP_PKEY_CTX, EVP_PKEY_CTX_free> signer(
         EVP_PKEY_CTX_new_from_pkey(nullptr, key.get(), nullptr), EVP_PKEY_CTX_free);
      owner<EVP_SIGNATURE, EVP_SIGNATURE_free> algorithm(
         EVP_SIGNATURE_fetch(nullptr, "ML-DSA-65", nullptr), EVP_SIGNATURE_free);
      require(signer && algorithm, "ML-DSA signer unavailable");
      OSSL_PARAM sign_parameters[] = {context_parameter(context), OSSL_PARAM_construct_end()};
      require(EVP_PKEY_sign_message_init(signer.get(), algorithm.get(), sign_parameters) == 1,
              "ML-DSA signer initialization failed");
      size_t signature_size = 0;
      require(EVP_PKEY_sign(signer.get(), nullptr, &signature_size,
                            digest.data(), digest.size()) == 1,
              "signature-size lookup failed");
      std::vector<unsigned char> signature(signature_size);
      require(EVP_PKEY_sign(signer.get(), signature.data(), &signature_size,
                            digest.data(), digest.size()) == 1,
              "ML-DSA signing failed");
      signature.resize(signature_size);
      require(signature.size() == 3309, "unexpected ML-DSA-65 signature size");

      require(verify(public_only.get(), signature, digest, context),
              "valid signature rejected");
      require(!verify(public_only.get(), signature, digest, other_context),
              "wrong context accepted");
      digest[0] ^= 1;
      require(!verify(public_only.get(), signature, digest, context),
              "wrong digest accepted");
      digest[0] ^= 1;
      signature[0] ^= 1;
      require(!verify(public_only.get(), signature, digest, context),
              "corrupted signature accepted");

      std::cout << "ML-DSA-65 probe passed: public key " << public_bytes.size()
                << " bytes, signature " << signature.size()
                << " bytes, bare envelope " << public_bytes.size() + signature.size()
                << " bytes. No chain authorization was exercised.\n";
      return 0;
   } catch (const std::exception& error) {
      std::cerr << "ML-DSA-65 probe failed: " << error.what() << '\n';
      return 1;
   }
}
