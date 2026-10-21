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
constexpr unsigned char mldsa_variant_tag = 3;
constexpr size_t mldsa_public_size = 1952;
constexpr size_t mldsa_signature_size = 3309;
constexpr size_t mldsa_wire_size = 1 + mldsa_public_size + mldsa_signature_size;

template <typename T, void (*Free)(T*)>
using owner = std::unique_ptr<T, decltype(Free)>;

void require(bool condition, const char* message) {
   if (!condition) throw std::runtime_error(message);
}

// Experimental fixed-width fc::raw variant-3 representation. The full
// TelosZero serializer must independently confirm this byte layout.
std::vector<unsigned char> encode_envelope(
   const std::vector<unsigned char>& public_key,
   const std::vector<unsigned char>& signature) {
   require(public_key.size() == mldsa_public_size, "invalid envelope public-key length");
   require(signature.size() == mldsa_signature_size, "invalid envelope signature length");
   std::vector<unsigned char> encoded;
   encoded.reserve(mldsa_wire_size);
   encoded.push_back(mldsa_variant_tag);
   encoded.insert(encoded.end(), public_key.begin(), public_key.end());
   encoded.insert(encoded.end(), signature.begin(), signature.end());
   return encoded;
}

bool decode_envelope(const std::vector<unsigned char>& encoded,
                     std::vector<unsigned char>& public_key,
                     std::vector<unsigned char>& signature) {
   public_key.clear();
   signature.clear();
   if (encoded.size() != mldsa_wire_size || encoded[0] != mldsa_variant_tag)
      return false;
   public_key.assign(encoded.begin() + 1, encoded.begin() + 1 + mldsa_public_size);
   signature.assign(encoded.begin() + 1 + mldsa_public_size, encoded.end());
   return true;
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
      require(public_bytes.size() == mldsa_public_size,
              "unexpected ML-DSA-65 public-key size");

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
      require(signature.size() == mldsa_signature_size,
              "unexpected ML-DSA-65 signature size");

      auto encoded = encode_envelope(public_bytes, signature);
      require(encoded.size() == mldsa_wire_size, "unexpected envelope size");
      std::vector<unsigned char> decoded_public, decoded_signature;
      require(decode_envelope(encoded, decoded_public, decoded_signature),
              "valid envelope rejected");
      require(decoded_public == public_bytes && decoded_signature == signature,
              "envelope round-trip changed bytes");
      auto invalid_envelope = encoded;
      invalid_envelope[0] = 2;
      require(!decode_envelope(invalid_envelope, decoded_public, decoded_signature),
              "wrong variant accepted");
      require(decoded_public.empty() && decoded_signature.empty(),
              "invalid envelope retained decoded bytes");
      invalid_envelope = encoded;
      invalid_envelope.pop_back();
      require(!decode_envelope(invalid_envelope, decoded_public, decoded_signature),
              "truncated envelope accepted");
      invalid_envelope = encoded;
      invalid_envelope.push_back(0);
      require(!decode_envelope(invalid_envelope, decoded_public, decoded_signature),
              "trailing envelope byte accepted");
      require(decode_envelope(encoded, decoded_public, decoded_signature),
              "valid envelope rejected after malformed input");

      owner<EVP_PKEY_CTX, EVP_PKEY_CTX_free> importer(
         EVP_PKEY_CTX_new_from_name(nullptr, "ML-DSA-65", nullptr), EVP_PKEY_CTX_free);
      require(importer && EVP_PKEY_fromdata_init(importer.get()) == 1,
              "public-key importer unavailable");
      OSSL_PARAM import_parameters[] = {
         OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                                           decoded_public.data(), decoded_public.size()),
         OSSL_PARAM_construct_end()
      };
      EVP_PKEY* imported = nullptr;
      require(EVP_PKEY_fromdata(importer.get(), &imported, EVP_PKEY_PUBLIC_KEY,
                                import_parameters) == 1, "public-key import failed");
      owner<EVP_PKEY, EVP_PKEY_free> public_only(imported, EVP_PKEY_free);

      require(verify(public_only.get(), decoded_signature, digest, context),
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
                << " bytes, tagged envelope " << encoded.size()
                << " bytes. No chain authorization was exercised.\n";
      return 0;
   } catch (const std::exception& error) {
      std::cerr << "ML-DSA-65 probe failed: " << error.what() << '\n';
      return 1;
   }
}
