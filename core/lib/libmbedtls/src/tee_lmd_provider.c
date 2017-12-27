// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2017, ARM Limited
 */

#include <assert.h>
#include <compiler.h>
#include <crypto/aes-ccm.h>
#include <crypto/aes-gcm.h>
#include <crypto/crypto.h>
#include <kernel/panic.h>
#include "mbedtls.h"
#include <stdlib.h>
#include <string_ext.h>
#include <string.h>
#include <tee/tee_cryp_utl.h>
#include <utee_defines.h>

#if defined(_CFG_CRYPTO_WITH_ACIPHER) || defined(_CFG_CRYPTO_WITH_DRBG)
static unsigned long int next = 1;

/* Return next random integer */
static int _rand(void)
{
	next = next * 1103515245L + 12345;
	return (unsigned int) (next / 65536L) % 32768L;
}

static int mbd_rand(void *rng_state, unsigned char *output, size_t len)
{
	size_t use_len;
	int rnd;

	if (rng_state != NULL)
		rng_state  = NULL;

	while (len > 0) {
		use_len = len;
		if (use_len > sizeof(int))
			use_len = sizeof(int);

		rnd = _rand();
		memcpy(output, &rnd, use_len);
		output += use_len;
		len -= use_len;
	}
	return 0;
}
#endif /* defined(_CFG_CRYPTO_WITH_ACIPHER) ||
	* defined(_CFG_CRYPTO_WITH_DRBG)
	*/

#if defined(_CFG_CRYPTO_WITH_CIPHER) || defined(_CFG_CRYPTO_WITH_MAC) || \
	defined(_CFG_CRYPTO_WITH_AUTHENC)
/*
 * Get the Mbedtls chipher info given a TEE Algorithm "algo"
 * Return
 * - TEE_SUCCESS in case of success,
 * - NULL in case of error
 */
static const mbedtls_cipher_info_t
		*tee_algo_to_mbedtls_cipher_info(uint32_t algo,
						size_t key_len)
{
	/* Only support key_length is 128 bits of AES in Optee_os */
	switch (algo) {
#if defined(CFG_CRYPTO_AES)
	case TEE_ALG_AES_ECB_NOPAD:
		if (key_len == 128)
			return mbedtls_cipher_info_from_string("AES-128-ECB");
		else if (key_len == 192)
			return mbedtls_cipher_info_from_string("AES-192-ECB");
		else if (key_len == 256)
			return mbedtls_cipher_info_from_string("AES-256-ECB");
		else
			return NULL;
	case TEE_ALG_AES_CBC_NOPAD:
		if (key_len == 128)
			return mbedtls_cipher_info_from_string("AES-128-CBC");
		else if (key_len == 192)
			return mbedtls_cipher_info_from_string("AES-192-CBC");
		else if (key_len == 256)
			return mbedtls_cipher_info_from_string("AES-256-CBC");
		else
			return NULL;
	case TEE_ALG_AES_CTR:
		if (key_len == 128)
			return mbedtls_cipher_info_from_string("AES-128-CTR");
		else if (key_len == 192)
			return mbedtls_cipher_info_from_string("AES-192-CTR");
		else if (key_len == 256)
			return mbedtls_cipher_info_from_string("AES-256-CTR");
		else
			return NULL;
	case TEE_ALG_AES_CTS:
	case TEE_ALG_AES_XTS:
	case TEE_ALG_AES_CCM:
	case TEE_ALG_AES_GCM:
	case TEE_ALG_AES_CBC_MAC_NOPAD:
	case TEE_ALG_AES_CBC_MAC_PKCS5:
		return NULL;
#endif
#if defined(CFG_CRYPTO_DES)
	case TEE_ALG_DES_ECB_NOPAD:
		if (key_len == 64)
			return mbedtls_cipher_info_from_string("DES-ECB");
		else
			return NULL;
	case TEE_ALG_DES_CBC_MAC_NOPAD:
	case TEE_ALG_DES_CBC_MAC_PKCS5:
		return NULL;
	case TEE_ALG_DES_CBC_NOPAD:
		if (key_len == 64)
			return mbedtls_cipher_info_from_string("DES-CBC");
		else
			return NULL;
	case TEE_ALG_DES3_ECB_NOPAD:
		if (key_len == 128)
			return mbedtls_cipher_info_from_string("DES-EDE-ECB");
		else if (key_len == 192)
			return mbedtls_cipher_info_from_string("DES-EDE3-ECB");
		else
			return NULL;
	case TEE_ALG_DES3_CBC_MAC_NOPAD:
	case TEE_ALG_DES3_CBC_MAC_PKCS5:
		return NULL;
	case TEE_ALG_DES3_CBC_NOPAD:
		if (key_len == 128)
			return mbedtls_cipher_info_from_string("DES-EDE-CBC");
		else if (key_len == 192)
			return mbedtls_cipher_info_from_string("DES-EDE3-CBC");
		else
			return NULL;
#endif
	default:
		return NULL;
	}
}
#endif	/* defined(_CFG_CRYPTO_WITH_CIPHER) ||
	 * defined(_CFG_CRYPTO_WITH_MAC) || defined(_CFG_CRYPTO_WITH_AUTHENC)
	 */

#if defined(CFG_CRYPTO_HMAC)
/*
 * Get mbedtls hash info given a TEE Algorithm "algo"
 * Return
 * - mbedtls_md_info_t * in case of success,
 * - NULL in case of error
 */
static const mbedtls_md_info_t *tee_algo_to_mbedtls_hash_info(uint32_t algo)
{
	switch (algo) {
#if defined(CFG_CRYPTO_SHA1)
	case TEE_ALG_RSASSA_PKCS1_V1_5_SHA1:
	case TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA1:
	case TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1:
	case TEE_ALG_SHA1:
	case TEE_ALG_DSA_SHA1:
	case TEE_ALG_HMAC_SHA1:
		return mbedtls_md_info_from_string("SHA1");
#endif
#if defined(CFG_CRYPTO_MD5)
	case TEE_ALG_RSASSA_PKCS1_V1_5_MD5:
	case TEE_ALG_MD5:
	case TEE_ALG_HMAC_MD5:
		return mbedtls_md_info_from_string("MD5");
#endif
#if defined(CFG_CRYPTO_SHA224)
	case TEE_ALG_RSASSA_PKCS1_V1_5_SHA224:
	case TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA224:
	case TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA224:
	case TEE_ALG_SHA224:
	case TEE_ALG_DSA_SHA224:
	case TEE_ALG_HMAC_SHA224:
		return mbedtls_md_info_from_string("SHA224");
#endif
#if defined(CFG_CRYPTO_SHA256)
	case TEE_ALG_RSASSA_PKCS1_V1_5_SHA256:
	case TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256:
	case TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA256:
	case TEE_ALG_SHA256:
	case TEE_ALG_DSA_SHA256:
	case TEE_ALG_HMAC_SHA256:
		return mbedtls_md_info_from_string("SHA256");
#endif
#if defined(CFG_CRYPTO_SHA384)
	case TEE_ALG_RSASSA_PKCS1_V1_5_SHA384:
	case TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA384:
	case TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA384:
	case TEE_ALG_SHA384:
	case TEE_ALG_HMAC_SHA384:
		return mbedtls_md_info_from_string("SHA384");
#endif
#if defined(CFG_CRYPTO_SHA512)
	case TEE_ALG_RSASSA_PKCS1_V1_5_SHA512:
	case TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA512:
	case TEE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA512:
	case TEE_ALG_SHA512:
	case TEE_ALG_HMAC_SHA512:
		return mbedtls_md_info_from_string("SHA512");
#endif
	case TEE_ALG_RSAES_PKCS1_V1_5:
		/* invalid one. but it should not be used anyway */
		return NULL;
	default:
		return NULL;
	}
}
#endif /*  defined(CFG_CRYPTO_HMAC) */

/******************************************************************************
 * Message digest functions
 ******************************************************************************/
#if defined(_CFG_CRYPTO_WITH_HASH)
static TEE_Result hash_get_ctx_size(uint32_t algo, size_t *size)
{
	switch (algo) {
#if defined(CFG_CRYPTO_MD5)
	case TEE_ALG_MD5:
		*size = sizeof(mbedtls_md5_context);
		break;
#endif
#if defined(CFG_CRYPTO_SHA1)
	case TEE_ALG_SHA1:
		*size = sizeof(mbedtls_sha1_context);
		break;
#endif
#if defined(CFG_CRYPTO_SHA224)
	case TEE_ALG_SHA224:
		*size = sizeof(mbedtls_sha256_context);
		break;
#endif
#if defined(CFG_CRYPTO_SHA256)
	case TEE_ALG_SHA256:
		*size = sizeof(mbedtls_sha256_context);
		break;
#endif
#if defined(CFG_CRYPTO_SHA384)
	case TEE_ALG_SHA384:
		*size = sizeof(mbedtls_sha512_context);
		break;
#endif
#if defined(CFG_CRYPTO_SHA512)
	case TEE_ALG_SHA512:
		*size = sizeof(mbedtls_sha512_context);
		break;
#endif
	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}
	return TEE_SUCCESS;
}

TEE_Result crypto_hash_alloc_ctx(void **ctx_ret, uint32_t algo)
{
	TEE_Result res;
	size_t ctx_size;
	void *ctx;

	res = hash_get_ctx_size(algo, &ctx_size);
	if (res)
		return res;

	ctx = calloc(1, ctx_size);
	if (!ctx)
		return TEE_ERROR_OUT_OF_MEMORY;

	*ctx_ret = ctx;
	return TEE_SUCCESS;
}

void crypto_hash_free_ctx(void *ctx, uint32_t algo __unused)
{
	size_t ctx_size __maybe_unused;

	/*
	 * Check that it's a supported algo, or crypto_hash_alloc_ctx()
	 * could never have succeded above.
	 */
	assert(!hash_get_ctx_size(algo, &ctx_size));
	free(ctx);
}

void crypto_hash_copy_state(void *dst_ctx, void *src_ctx, uint32_t algo)
{
	TEE_Result res __maybe_unused;
	size_t ctx_size = 0;

	res = hash_get_ctx_size(algo, &ctx_size);
	assert(!res);
	memcpy(dst_ctx, src_ctx, ctx_size);
}

TEE_Result crypto_hash_init(void *ctx, uint32_t algo)
{
	if (ctx == NULL)
		return TEE_ERROR_BAD_PARAMETERS;

	switch (algo) {
#if defined(CFG_CRYPTO_SHA1)
	case TEE_ALG_SHA1:
		mbedtls_sha1_init(ctx);
		mbedtls_sha1_starts(ctx);
		break;
#endif
#if defined(CFG_CRYPTO_MD5)
	case TEE_ALG_MD5:
		mbedtls_md5_init(ctx);
		mbedtls_md5_starts(ctx);
		break;
#endif
#if defined(CFG_CRYPTO_SHA224)
	case TEE_ALG_SHA224:
		mbedtls_sha256_init(ctx);
		mbedtls_sha256_starts(ctx, 1);
		break;
#endif
#if defined(CFG_CRYPTO_SHA256)
	case TEE_ALG_SHA256:
		mbedtls_sha256_init(ctx);
		mbedtls_sha256_starts(ctx, 0);
		break;
#endif
#if defined(CFG_CRYPTO_SHA384)
	case TEE_ALG_SHA384:
		mbedtls_sha512_init(ctx);
		mbedtls_sha512_starts(ctx, 1);
		break;
#endif
#if defined(CFG_CRYPTO_SHA512)
	case TEE_ALG_SHA512:
		mbedtls_sha512_init(ctx);
		mbedtls_sha512_starts(ctx, 0);
		break;
#endif
	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}
	return TEE_SUCCESS;
}

TEE_Result crypto_hash_update(void *ctx, uint32_t algo,
				      const uint8_t *data, size_t len)
{
	if (ctx == NULL)
		return TEE_ERROR_BAD_PARAMETERS;

	switch (algo) {
#if defined(CFG_CRYPTO_SHA1)
	case TEE_ALG_SHA1:
		mbedtls_sha1_update(ctx, data, len);
		break;
#endif
#if defined(CFG_CRYPTO_MD5)
	case TEE_ALG_MD5:
		mbedtls_md5_update(ctx, data, len);
		break;
#endif
#if defined(CFG_CRYPTO_SHA224)
	case TEE_ALG_SHA224:
		mbedtls_sha256_update(ctx, data, len);
		break;
#endif
#if defined(CFG_CRYPTO_SHA256)
	case TEE_ALG_SHA256:
		mbedtls_sha256_update(ctx, data, len);
		break;
#endif
#if defined(CFG_CRYPTO_SHA384)
	case TEE_ALG_SHA384:
		mbedtls_sha512_update(ctx, data, len);
		break;
#endif
#if defined(CFG_CRYPTO_SHA512)
	case TEE_ALG_SHA512:
		mbedtls_sha512_update(ctx, data, len);
		break;
#endif
	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}

	return TEE_SUCCESS;
}

TEE_Result crypto_hash_final(void *ctx, uint32_t algo, uint8_t *digest,
			     size_t len)
{
	TEE_Result res = TEE_SUCCESS;
	size_t hash_size;
	uint8_t block_digest[TEE_MAX_HASH_SIZE];
	uint8_t *tmp_digest;

	if (ctx == NULL)
		return TEE_ERROR_BAD_PARAMETERS;

	res = tee_hash_get_digest_size(algo, &hash_size);
	if (res != TEE_SUCCESS)
		return res;

	if (hash_size > len) {
		if (hash_size > sizeof(block_digest))
			return TEE_ERROR_BAD_STATE;
		tmp_digest = block_digest; /* use a tempory buffer */
	} else {
		tmp_digest = digest;
	}

	switch (algo) {
#if defined(CFG_CRYPTO_SHA1)
	case TEE_ALG_SHA1:
		mbedtls_sha1_finish(ctx, tmp_digest);
		break;
#endif
#if defined(CFG_CRYPTO_MD5)
	case TEE_ALG_MD5:
		mbedtls_md5_finish(ctx, tmp_digest);
		break;
#endif
#if defined(CFG_CRYPTO_SHA224)
	case TEE_ALG_SHA224:
		mbedtls_sha256_finish(ctx, tmp_digest);
		break;
#endif
#if defined(CFG_CRYPTO_SHA256)
	case TEE_ALG_SHA256:
		mbedtls_sha256_finish(ctx, tmp_digest);
		break;
#endif
#if defined(CFG_CRYPTO_SHA384)
	case TEE_ALG_SHA384:
		mbedtls_sha512_finish(ctx, tmp_digest);
		break;
#endif
#if defined(CFG_CRYPTO_SHA512)
	case TEE_ALG_SHA512:
		mbedtls_sha512_finish(ctx, tmp_digest);
		break;
#endif
	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}
	if (hash_size > len)
		memcpy(digest, tmp_digest, len);

	return res;
}
#endif /* _CFG_CRYPTO_WITH_HASH */

/******************************************************************************
 * Asymmetric algorithms
 ******************************************************************************/

#if defined(_CFG_CRYPTO_WITH_ACIPHER)
struct bignum *crypto_bignum_allocate(size_t size_bits __unused)
{
	return NULL;
}

TEE_Result crypto_bignum_bin2bn(const uint8_t *from __unused,
				size_t fromsize __unused,
				struct bignum *to __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

size_t crypto_bignum_num_bytes(struct bignum *a __unused)
{
	return 0;
}

size_t crypto_bignum_num_bits(struct bignum *a __unused)
{
	return 0;
}

/*
 * crypto_bignum_allocate() and crypto_bignum_bin2bn() failing should be
 * enough to guarantee that the functions calling this function aren't
 * called, but just in case add a panic() here to avoid unexpected
 * behavoir.
 */
static void bignum_cant_happen(void)
{
	volatile bool b = true;

	/* Avoid warning about function does not return */
	if (b)
		panic();
}

void crypto_bignum_bn2bin(const struct bignum *from __unused,
			  uint8_t *to __unused)
{
	bignum_cant_happen();
}

void crypto_bignum_copy(struct bignum *to __unused,
			const struct bignum *from __unused)
{
	bignum_cant_happen();
}

void crypto_bignum_free(struct bignum *a)
{
	if (a)
		panic();
}

void crypto_bignum_clear(struct bignum *a __unused)
{
	bignum_cant_happen();
}

/* return -1 if a<b, 0 if a==b, +1 if a>b */
int32_t crypto_bignum_compare(struct bignum *a __unused,
			      struct bignum *b __unused)
{
	bignum_cant_happen();
	return -1;
}


#if defined(CFG_CRYPTO_RSA)
TEE_Result crypto_acipher_alloc_rsa_keypair(struct rsa_keypair *s __unused,
					    size_t key_size_bits __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result
crypto_acipher_alloc_rsa_public_key(struct rsa_public_key *s __unused,
				    size_t key_size_bits __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

void crypto_acipher_free_rsa_public_key(struct rsa_public_key *s __unused)
{
}

TEE_Result crypto_acipher_gen_rsa_key(struct rsa_keypair *key __unused,
				      size_t key_size __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_acipher_rsanopad_decrypt(struct rsa_keypair *key __unused,
					   const uint8_t *src __unused,
					   size_t src_len __unused,
					   uint8_t *dst __unused,
					   size_t *dst_len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_acipher_rsanopad_encrypt(struct rsa_public_key *key __unused,
					   const uint8_t *src __unused,
					   size_t src_len __unused,
					   uint8_t *dst __unused,
					   size_t *dst_len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_acipher_rsaes_decrypt(uint32_t algo __unused,
					struct rsa_keypair *key __unused,
					const uint8_t *label __unused,
					size_t label_len __unused,
					const uint8_t *src __unused,
					size_t src_len __unused,
					uint8_t *dst __unused,
					size_t *dst_len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_acipher_rsaes_encrypt(uint32_t algo __unused,
					struct rsa_public_key *key __unused,
					const uint8_t *label __unused,
					size_t label_len __unused,
					const uint8_t *src __unused,
					size_t src_len __unused,
					uint8_t *dst __unused,
					size_t *dst_len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_acipher_rsassa_sign(uint32_t algo __unused,
				      struct rsa_keypair *key __unused,
				      int salt_len __unused,
				      const uint8_t *msg __unused,
				      size_t msg_len __unused,
				      uint8_t *sig __unused,
				      size_t *sig_len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_acipher_rsassa_verify(uint32_t algo __unused,
					struct rsa_public_key *key __unused,
					int salt_len __unused,
					const uint8_t *msg __unused,
					size_t msg_len __unused,
					const uint8_t *sig __unused,
					size_t sig_len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}
#endif /* CFG_CRYPTO_RSA */

#if defined(CFG_CRYPTO_DSA)
TEE_Result crypto_acipher_alloc_dsa_keypair(struct dsa_keypair *s __unused,
					    size_t key_size_bits __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result
crypto_acipher_alloc_dsa_public_key(struct dsa_public_key *s __unused,
				    size_t key_size_bits __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_acipher_gen_dsa_key(struct dsa_keypair *key __unused,
				      size_t key_size __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_acipher_dsa_sign(uint32_t algo __unused,
				   struct dsa_keypair *key __unused,
				   const uint8_t *msg __unused,
				   size_t msg_len __unused,
				   uint8_t *sig __unused,
				   size_t *sig_len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_acipher_dsa_verify(uint32_t algo __unused,
				     struct dsa_public_key *key __unused,
				     const uint8_t *msg __unused,
				     size_t msg_len __unused,
				     const uint8_t *sig __unused,
				     size_t sig_len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}
#endif /* CFG_CRYPTO_DSA */

#if defined(CFG_CRYPTO_DH)
TEE_Result crypto_acipher_alloc_dh_keypair(struct dh_keypair *s __unused,
					   size_t key_size_bits __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_acipher_gen_dh_key(struct dh_keypair *key __unused,
				     struct bignum *q __unused,
				     size_t xbits __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result
crypto_acipher_dh_shared_secret(struct dh_keypair *private_key __unused,
				struct bignum *public_key __unused,
				struct bignum *secret __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}
#endif /* CFG_CRYPTO_DH */

#if defined(CFG_CRYPTO_ECC)
TEE_Result
crypto_acipher_alloc_ecc_public_key(struct ecc_public_key *s __unused,
				    size_t key_size_bits __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_acipher_alloc_ecc_keypair(struct ecc_keypair *s __unused,
					    size_t key_size_bits __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

void crypto_acipher_free_ecc_public_key(struct ecc_public_key *s __unused)
{
}

TEE_Result crypto_acipher_gen_ecc_key(struct ecc_keypair *key __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_acipher_ecc_sign(uint32_t algo __unused,
				   struct ecc_keypair *key __unused,
				   const uint8_t *msg __unused,
				   size_t msg_len __unused,
				   uint8_t *sig __unused,
				   size_t *sig_len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_acipher_ecc_verify(uint32_t algo __unused,
				     struct ecc_public_key *key __unused,
				     const uint8_t *msg __unused,
				     size_t msg_len __unused,
				     const uint8_t *sig __unused,
				     size_t sig_len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result
crypto_acipher_ecc_shared_secret(struct ecc_keypair *private_key __unused,
				 struct ecc_public_key *public_key __unused,
				 void *secret __unused,
				 unsigned long *secret_len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

#endif /* CFG_CRYPTO_ECC */

#endif /* _CFG_CRYPTO_WITH_ACIPHER */

/******************************************************************************
 * Symmetric ciphers
 ******************************************************************************/

#if defined(_CFG_CRYPTO_WITH_CIPHER)

static TEE_Result cipher_get_ctx_size(uint32_t algo, size_t *size)
{
	switch (algo) {
#if defined(CFG_CRYPTO_ECB)
	case TEE_ALG_AES_ECB_NOPAD:
		*size = sizeof(mbedtls_cipher_context_t);
		break;
#if defined(CFG_CRYPTO_CBC)
#endif
	case TEE_ALG_AES_CBC_NOPAD:
		*size = sizeof(mbedtls_cipher_context_t);
		break;
#endif
#if defined(CFG_CRYPTO_CTR)
	case TEE_ALG_AES_CTR:
		*size = sizeof(mbedtls_cipher_context_t);
		break;
#endif
#if defined(CFG_CRYPTO_XTS)
	case TEE_ALG_AES_XTS:
		return TEE_ERROR_NOT_SUPPORTED;
#endif
#if defined(CFG_CRYPTO_CTS)
	case TEE_ALG_AES_CTS:
		return TEE_ERROR_NOT_SUPPORTED;
#endif
#if defined(CFG_CRYPTO_ECB)
	case TEE_ALG_DES_ECB_NOPAD:
	case TEE_ALG_DES3_ECB_NOPAD:
		*size = sizeof(mbedtls_cipher_context_t);
		break;
#endif
#if defined(CFG_CRYPTO_CBC)
	case TEE_ALG_DES_CBC_NOPAD:
	case TEE_ALG_DES3_CBC_NOPAD:
		*size = sizeof(mbedtls_cipher_context_t);
		break;
#endif
	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}

	return TEE_SUCCESS;
}

TEE_Result crypto_cipher_alloc_ctx(void **ctx_ret, uint32_t algo)
{
	const mbedtls_cipher_info_t *cipher_info = NULL;
	int lmd_res;
	TEE_Result res;
	size_t ctx_size;
	void *ctx;

	switch (algo) {
#if defined(CFG_CRYPTO_ECB)
	case TEE_ALG_AES_ECB_NOPAD:
		cipher_info = tee_algo_to_mbedtls_cipher_info(algo, 128);
		break;
#endif
#if defined(CFG_CRYPTO_CBC)
	case TEE_ALG_AES_CBC_NOPAD:
		cipher_info = tee_algo_to_mbedtls_cipher_info(algo, 128);
		break;
#endif
#if defined(CFG_CRYPTO_CTR)
	case TEE_ALG_AES_CTR:
		cipher_info = tee_algo_to_mbedtls_cipher_info(algo, 128);
		break;
#endif
#if defined(CFG_CRYPTO_XTS)
	case TEE_ALG_AES_XTS:
		return TEE_ERROR_NOT_SUPPORTED;
#endif
#if defined(CFG_CRYPTO_CTS)
	case TEE_ALG_AES_CTS:
		return TEE_ERROR_NOT_SUPPORTED;
#endif
#if defined(CFG_CRYPTO_ECB)
	case TEE_ALG_DES_ECB_NOPAD:
		cipher_info = tee_algo_to_mbedtls_cipher_info(algo,
					MBEDTLS_KEY_LENGTH_DES);
		break;
	case TEE_ALG_DES3_ECB_NOPAD:
		cipher_info = tee_algo_to_mbedtls_cipher_info(algo,
					MBEDTLS_KEY_LENGTH_DES_EDE);
		break;
#endif
#if defined(CFG_CRYPTO_CBC)
	case TEE_ALG_DES_CBC_NOPAD:
		cipher_info = tee_algo_to_mbedtls_cipher_info(algo,
					MBEDTLS_KEY_LENGTH_DES);
		break;
	case TEE_ALG_DES3_CBC_NOPAD:
		cipher_info = tee_algo_to_mbedtls_cipher_info(algo,
					MBEDTLS_KEY_LENGTH_DES_EDE);
		break;
#endif
	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}

	if (!cipher_info)
		return TEE_ERROR_NOT_SUPPORTED;

	res = cipher_get_ctx_size(algo, &ctx_size);
	if (res)
		return res;

	ctx = calloc(1, ctx_size);
	if (!ctx)
		return TEE_ERROR_OUT_OF_MEMORY;

	mbedtls_cipher_init(ctx);

	lmd_res = mbedtls_cipher_setup(ctx, cipher_info);
	if (lmd_res != 0) {
		crypto_cipher_free_ctx(ctx, algo);
		EMSG("mbedtls_cipher_setup failed, res is 0x%x\n", -lmd_res);
		return TEE_ERROR_BAD_STATE;
	}

	*ctx_ret = ctx;
	return TEE_SUCCESS;
}

void crypto_cipher_free_ctx(void *ctx, uint32_t algo __maybe_unused)
{
	size_t ctx_size __maybe_unused;

	/*
	 * Check that it's a supported algo, or crypto_cipher_alloc_ctx()
	 * could never have succeded above.
	 */
	assert(!cipher_get_ctx_size(algo, &ctx_size));
	mbedtls_cipher_free(ctx);
	free(ctx);
}

void crypto_cipher_copy_state(void *dst_ctx, void *src_ctx,
				uint32_t algo __maybe_unused)
{
	mbedtls_cipher_clone(dst_ctx, src_ctx);
}

TEE_Result crypto_cipher_init(void *ctx, uint32_t algo,
			      TEE_OperationMode mode,
			      const uint8_t *key1, size_t key1_len,
			      const uint8_t *key2 __maybe_unused,
			      size_t key2_len __maybe_unused,
			      const uint8_t *iv __maybe_unused,
			      size_t iv_len __maybe_unused)
{
	const mbedtls_cipher_info_t *cipher_info = NULL;
	int lmd_res;

	if (!ctx)
		return TEE_ERROR_BAD_PARAMETERS;

	cipher_info = tee_algo_to_mbedtls_cipher_info(algo, key1_len * 8);
	if (cipher_info == NULL)
		return TEE_ERROR_NOT_SUPPORTED;

	lmd_res = mbedtls_cipher_setup_info(ctx, cipher_info);
	if (lmd_res != 0) {
		EMSG("setup info failed, res is 0x%x\n", -lmd_res);
		return TEE_ERROR_BAD_STATE;
	}

	lmd_res = mbedtls_cipher_setkey(ctx, key1, key1_len * 8,
				mode == TEE_MODE_ENCRYPT ?
				MBEDTLS_ENCRYPT : MBEDTLS_DECRYPT);
	if (lmd_res != 0) {
		EMSG("setkey failed, res is 0x%x\n", -lmd_res);
		return TEE_ERROR_BAD_STATE;
	}

	if (iv != NULL) {
		lmd_res = mbedtls_cipher_set_iv(ctx, iv, iv_len);
		if (lmd_res != 0) {
			EMSG("set iv failed, res is 0x%x\n", -lmd_res);
			return TEE_ERROR_BAD_STATE;
		}
	}

	lmd_res = mbedtls_cipher_reset(ctx);
	if (lmd_res != 0) {
		EMSG("mbedtls_cipher_reset failed, res is 0x%x\n", -lmd_res);
		return TEE_ERROR_BAD_STATE;
	}

	return TEE_SUCCESS;
}

TEE_Result crypto_cipher_update(void *ctx, uint32_t algo,
				TEE_OperationMode mode __maybe_unused,
				bool last_block __maybe_unused,
				const uint8_t *data, size_t len, uint8_t *dst)
{
	int lmd_res;
	size_t olen;
	size_t finish_olen;
#if defined(CFG_CRYPTO_ECB)
	size_t blk_size;
#endif
	if (!ctx)
		return TEE_ERROR_BAD_PARAMETERS;

	switch (algo) {
#if defined(CFG_CRYPTO_ECB)
	case TEE_ALG_AES_ECB_NOPAD:
	case TEE_ALG_DES_ECB_NOPAD:
	case TEE_ALG_DES3_ECB_NOPAD:
		blk_size = mbedtls_cipher_get_block_size(ctx);
		if (len % blk_size != 0)
			return TEE_ERROR_BAD_PARAMETERS;
		while (len) {
			lmd_res = mbedtls_cipher_update(ctx, data,
					blk_size, dst, &olen);
			if (lmd_res != 0) {
				EMSG("update failed, res is 0x%x\n", -lmd_res);
				return TEE_ERROR_BAD_STATE;
			}
			data += olen;
			dst += olen;
			len -= olen;
		}
		break;
#endif
#if defined(CFG_CRYPTO_CBC)
	case TEE_ALG_AES_CBC_NOPAD:
	case TEE_ALG_DES_CBC_NOPAD:
	case TEE_ALG_DES3_CBC_NOPAD:
		lmd_res = mbedtls_cipher_reset(ctx);
		if (lmd_res != 0) {
			EMSG("mbedtls_cipher_reset failed, res is 0x%x\n",
			     -lmd_res);
			return TEE_ERROR_BAD_STATE;
		}

		lmd_res = mbedtls_cipher_update(ctx, data, len, dst, &olen);
		if (lmd_res != 0) {
			EMSG("update failed, res is 0x%x\n", -lmd_res);
			return TEE_ERROR_BAD_STATE;
		}
		lmd_res = mbedtls_cipher_finish(ctx, dst + olen, &finish_olen);
		break;
#endif
#if defined(CFG_CRYPTO_CTR)
	case TEE_ALG_AES_CTR:
		lmd_res = mbedtls_cipher_update(ctx, data, len, dst, &olen);
		if (lmd_res != 0) {
			EMSG("update failed, res is 0x%x\n", -lmd_res);
			return TEE_ERROR_BAD_STATE;
		}

		lmd_res = mbedtls_cipher_finish(ctx, dst + olen, &finish_olen);
		break;
#endif
#if defined(CFG_CRYPTO_XTS)
	case TEE_ALG_AES_XTS:
		return TEE_ERROR_NOT_SUPPORTED;
#endif
#if defined(CFG_CRYPTO_CTS)
	case TEE_ALG_AES_CTS:
		return TEE_ERROR_NOT_SUPPORTED;
#endif
	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}

	if (lmd_res != 0) {
		EMSG("mbedtls_cipher_update failed, res is 0x%x\n",
				-lmd_res);
		return TEE_ERROR_BAD_STATE;
	}

	return TEE_SUCCESS;
}

void crypto_cipher_final(void *ctx __unused, uint32_t algo __unused)
{
}
#endif /* _CFG_CRYPTO_WITH_CIPHER */

/*****************************************************************************
 * Message Authentication Code functions
 *****************************************************************************/

#if defined(_CFG_CRYPTO_WITH_MAC)
static TEE_Result mac_get_ctx_size(uint32_t algo, size_t *size)
{
	switch (algo) {
#if defined(CFG_CRYPTO_HMAC)
	case TEE_ALG_HMAC_MD5:
	case TEE_ALG_HMAC_SHA224:
	case TEE_ALG_HMAC_SHA1:
	case TEE_ALG_HMAC_SHA256:
	case TEE_ALG_HMAC_SHA384:
	case TEE_ALG_HMAC_SHA512:
		*size = sizeof(mbedtls_md_context_t);
		break;
#endif
#if defined(CFG_CRYPTO_CBC_MAC)
	case TEE_ALG_AES_CBC_MAC_NOPAD:
	case TEE_ALG_AES_CBC_MAC_PKCS5:
	case TEE_ALG_DES_CBC_MAC_NOPAD:
	case TEE_ALG_DES_CBC_MAC_PKCS5:
	case TEE_ALG_DES3_CBC_MAC_NOPAD:
	case TEE_ALG_DES3_CBC_MAC_PKCS5:
		return TEE_ERROR_NOT_SUPPORTED;
#endif
#if defined(CFG_CRYPTO_CMAC)
	case TEE_ALG_AES_CMAC:
		*size = sizeof(mbedtls_cipher_context_t);
		break;
#endif
	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}

	return TEE_SUCCESS;
}

TEE_Result crypto_mac_alloc_ctx(void **ctx_ret, uint32_t algo)
{
#if defined(CFG_CRYPTO_HMAC)
	const mbedtls_md_info_t *md_info = NULL;
#endif
#if defined(CFG_CRYPTO_CMAC)
	const mbedtls_cipher_info_t *cipher_info = NULL;
#endif
#if defined(CFG_CRYPTO_HMAC) || defined(CFG_CRYPTO_CMAC)
	int lmd_res;
#endif
	TEE_Result res = TEE_SUCCESS;
	size_t ctx_size;
	void *ctx;

	res = mac_get_ctx_size(algo, &ctx_size);
	if (res)
		return res;

	ctx = calloc(1, ctx_size);
	if (!ctx)
		return TEE_ERROR_OUT_OF_MEMORY;

	switch (algo) {
#if defined(CFG_CRYPTO_HMAC)
	case TEE_ALG_HMAC_MD5:
	case TEE_ALG_HMAC_SHA224:
	case TEE_ALG_HMAC_SHA1:
	case TEE_ALG_HMAC_SHA256:
	case TEE_ALG_HMAC_SHA384:
	case TEE_ALG_HMAC_SHA512:
		md_info = tee_algo_to_mbedtls_hash_info(algo);
		if (!md_info) {
			res =  TEE_ERROR_NOT_SUPPORTED;
			break;
		}

		mbedtls_md_init(ctx);
		lmd_res = mbedtls_md_setup(ctx, md_info, 1);
		if (lmd_res != 0) {
			EMSG("md setup failed, res is 0x%x\n", -lmd_res);
			res = TEE_ERROR_GENERIC;
		}
		break;
#endif
#if defined(CFG_CRYPTO_CBC_MAC)
	case TEE_ALG_AES_CBC_MAC_NOPAD:
	case TEE_ALG_AES_CBC_MAC_PKCS5:
	case TEE_ALG_DES_CBC_MAC_NOPAD:
	case TEE_ALG_DES_CBC_MAC_PKCS5:
	case TEE_ALG_DES3_CBC_MAC_NOPAD:
	case TEE_ALG_DES3_CBC_MAC_PKCS5:
		res = TEE_ERROR_NOT_SUPPORTED;
		break;
#endif
#if defined(CFG_CRYPTO_CMAC)
	case TEE_ALG_AES_CMAC:
		cipher_info =
			tee_algo_to_mbedtls_cipher_info(TEE_ALG_AES_ECB_NOPAD,
							128);
		if (!cipher_info) {
			res =  TEE_ERROR_NOT_SUPPORTED;
			break;
		}
		mbedtls_cipher_init(ctx);
		lmd_res = mbedtls_cipher_setup(ctx, cipher_info);
		if (lmd_res != 0) {
			EMSG("cipher setup failed, res is 0x%x\n", -lmd_res);
			res = TEE_ERROR_GENERIC;
		}
		lmd_res = mbedtls_cipher_cmac_setup(ctx);
		if (lmd_res != 0) {
			EMSG("cmac setup failed, res is 0x%x\n", -lmd_res);
			res = TEE_ERROR_GENERIC;
		}
		break;
#endif
	default:
		res = TEE_ERROR_NOT_SUPPORTED;
	}

	if (res == TEE_SUCCESS)
		*ctx_ret = ctx;
	else
		crypto_mac_free_ctx(ctx, algo);
	return res;
}

void crypto_mac_free_ctx(void *ctx, uint32_t algo __maybe_unused)
{
	switch (algo) {
#if defined(CFG_CRYPTO_HMAC)
	case TEE_ALG_HMAC_MD5:
	case TEE_ALG_HMAC_SHA224:
	case TEE_ALG_HMAC_SHA1:
	case TEE_ALG_HMAC_SHA256:
	case TEE_ALG_HMAC_SHA384:
	case TEE_ALG_HMAC_SHA512:
		mbedtls_md_free(ctx);
		break;
#endif
#if defined(CFG_CRYPTO_CBC_MAC)
	case TEE_ALG_AES_CBC_MAC_NOPAD:
	case TEE_ALG_AES_CBC_MAC_PKCS5:
	case TEE_ALG_DES_CBC_MAC_NOPAD:
	case TEE_ALG_DES_CBC_MAC_PKCS5:
	case TEE_ALG_DES3_CBC_MAC_NOPAD:
	case TEE_ALG_DES3_CBC_MAC_PKCS5:
		break;
#endif
#if defined(CFG_CRYPTO_CMAC)
	case TEE_ALG_AES_CMAC:
		mbedtls_cipher_free(ctx);
		break;
#endif
	default:
		break;
	}
	free(ctx);
}

void crypto_mac_copy_state(void *dst_ctx, void *src_ctx, uint32_t algo)
{
#if defined(CFG_CRYPTO_HMAC) || defined(CFG_CRYPTO_CMAC)
	int lmd_res;
#endif

	switch (algo) {
#if defined(CFG_CRYPTO_HMAC)
	case TEE_ALG_HMAC_MD5:
	case TEE_ALG_HMAC_SHA224:
	case TEE_ALG_HMAC_SHA1:
	case TEE_ALG_HMAC_SHA256:
	case TEE_ALG_HMAC_SHA384:
	case TEE_ALG_HMAC_SHA512:
		lmd_res = mbedtls_md_clone(dst_ctx, src_ctx);
		if (lmd_res != 0)
			EMSG("hmac clone failed, res is 0x%x\n", -lmd_res);
		break;
#endif
#if defined(CFG_CRYPTO_CBC_MAC)
	case TEE_ALG_AES_CBC_MAC_NOPAD:
	case TEE_ALG_AES_CBC_MAC_PKCS5:
	case TEE_ALG_DES_CBC_MAC_NOPAD:
	case TEE_ALG_DES_CBC_MAC_PKCS5:
	case TEE_ALG_DES3_CBC_MAC_NOPAD:
	case TEE_ALG_DES3_CBC_MAC_PKCS5:
		break;
#endif
#if defined(CFG_CRYPTO_CMAC)
	case TEE_ALG_AES_CMAC:
		lmd_res = mbedtls_cipher_clone(dst_ctx, src_ctx);
		if (lmd_res != 0)
			EMSG("cmac clone failed, res is 0x%x\n", -lmd_res);
		break;
#endif
	default:
		break;
	}
}

TEE_Result crypto_mac_init(void *ctx, uint32_t algo, const uint8_t *key,
			   size_t len)
{
#if defined(CFG_CRYPTO_HMAC) || defined(CFG_CRYPTO_CMAC)
	int lmd_res;
#endif
#if defined(CFG_CRYPTO_CMAC)
	const mbedtls_cipher_info_t *cipher_info = NULL;
#endif

	if (!ctx)
		return TEE_ERROR_BAD_PARAMETERS;

	switch (algo) {
#if defined(CFG_CRYPTO_HMAC)
	case TEE_ALG_HMAC_MD5:
	case TEE_ALG_HMAC_SHA224:
	case TEE_ALG_HMAC_SHA1:
	case TEE_ALG_HMAC_SHA256:
	case TEE_ALG_HMAC_SHA384:
	case TEE_ALG_HMAC_SHA512:
		mbedtls_md_hmac_reset(ctx);
		lmd_res = mbedtls_md_hmac_starts(ctx, key, len);
		if (lmd_res != 0) {
			EMSG("hmac starts failed, res is 0x%x\n", -lmd_res);
			return TEE_ERROR_GENERIC;
		}
		break;
#endif
#if defined(CFG_CRYPTO_CBC_MAC)
	case TEE_ALG_AES_CBC_MAC_NOPAD:
	case TEE_ALG_AES_CBC_MAC_PKCS5:
	case TEE_ALG_DES_CBC_MAC_NOPAD:
	case TEE_ALG_DES_CBC_MAC_PKCS5:
	case TEE_ALG_DES3_CBC_MAC_NOPAD:
	case TEE_ALG_DES3_CBC_MAC_PKCS5:
		return TEE_ERROR_NOT_SUPPORTED;
#endif
#if defined(CFG_CRYPTO_CMAC)
	case TEE_ALG_AES_CMAC:
		cipher_info =
			tee_algo_to_mbedtls_cipher_info(TEE_ALG_AES_ECB_NOPAD,
							len * 8);
		if (!cipher_info)
			return TEE_ERROR_NOT_SUPPORTED;

		lmd_res = mbedtls_cipher_setup_info(ctx, cipher_info);
		if (lmd_res != 0) {
			EMSG("setup info failed, res is 0x%x\n", -lmd_res);
			return TEE_ERROR_BAD_STATE;
		}

		lmd_res = mbedtls_cipher_cmac_reset(ctx);
		if (lmd_res != 0) {
			EMSG("cmac reset failed, res is 0x%x\n", -lmd_res);
			return TEE_ERROR_GENERIC;
		}

		lmd_res = mbedtls_cipher_cmac_starts(ctx, key, len * 8);
		if (lmd_res != 0) {
			EMSG("cmac starts failed, res is 0x%x\n", -lmd_res);
			return TEE_ERROR_GENERIC;
		}
		break;
#endif
	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}
	return TEE_SUCCESS;
}

TEE_Result crypto_mac_update(void *ctx, uint32_t algo, const uint8_t *data,
			     size_t len)
{
#if defined(CFG_CRYPTO_HMAC) || defined(CFG_CRYPTO_CMAC)
	int lmd_res;
#endif

	if (!data || !len)
		return TEE_SUCCESS;

	switch (algo) {
#if defined(CFG_CRYPTO_HMAC)
	case TEE_ALG_HMAC_MD5:
	case TEE_ALG_HMAC_SHA224:
	case TEE_ALG_HMAC_SHA1:
	case TEE_ALG_HMAC_SHA256:
	case TEE_ALG_HMAC_SHA384:
	case TEE_ALG_HMAC_SHA512:
		lmd_res = mbedtls_md_hmac_update(ctx, data, len);
		if (lmd_res != 0) {
			EMSG("hmac update failed, res is 0x%x\n", -lmd_res);
			return TEE_ERROR_GENERIC;
		}
		break;
#endif
#if defined(CFG_CRYPTO_CBC_MAC)
	case TEE_ALG_AES_CBC_MAC_NOPAD:
	case TEE_ALG_AES_CBC_MAC_PKCS5:
	case TEE_ALG_DES_CBC_MAC_NOPAD:
	case TEE_ALG_DES_CBC_MAC_PKCS5:
	case TEE_ALG_DES3_CBC_MAC_NOPAD:
	case TEE_ALG_DES3_CBC_MAC_PKCS5:
		return TEE_ERROR_NOT_SUPPORTED;
#endif
#if defined(CFG_CRYPTO_CMAC)
	case TEE_ALG_AES_CMAC:
		lmd_res = mbedtls_cipher_cmac_update(ctx, data, len);
		if (lmd_res != 0) {
			EMSG("cmac update failed, res is 0x%x\n", -lmd_res);
			return TEE_ERROR_GENERIC;
		}
		break;
#endif
	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}

	return TEE_SUCCESS;
}

TEE_Result crypto_mac_final(void *ctx, uint32_t algo, uint8_t *digest,
			    size_t digest_len)
{
#if defined(CFG_CRYPTO_HMAC) || defined(CFG_CRYPTO_CMAC)
	int lmd_res;
	size_t block_size;
#endif

	switch (algo) {
#if defined(CFG_CRYPTO_HMAC)
	case TEE_ALG_HMAC_MD5:
	case TEE_ALG_HMAC_SHA224:
	case TEE_ALG_HMAC_SHA1:
	case TEE_ALG_HMAC_SHA256:
	case TEE_ALG_HMAC_SHA384:
	case TEE_ALG_HMAC_SHA512:
		block_size = mbedtls_md_get_size(((mbedtls_md_context_t *)ctx)
						->md_info);
		if (block_size > digest_len)
			return TEE_ERROR_SHORT_BUFFER;
		lmd_res = mbedtls_md_hmac_finish(ctx, digest);
		if (lmd_res != 0) {
			EMSG("hmac finish failed, res is 0x%x\n", -lmd_res);
			return TEE_ERROR_GENERIC;
		}
		break;
#endif
#if defined(CFG_CRYPTO_CBC_MAC)
	case TEE_ALG_AES_CBC_MAC_NOPAD:
	case TEE_ALG_AES_CBC_MAC_PKCS5:
	case TEE_ALG_DES_CBC_MAC_NOPAD:
	case TEE_ALG_DES_CBC_MAC_PKCS5:
	case TEE_ALG_DES3_CBC_MAC_NOPAD:
	case TEE_ALG_DES3_CBC_MAC_PKCS5:
		return TEE_ERROR_NOT_SUPPORTED;
#endif
#if defined(CFG_CRYPTO_CMAC)
	case TEE_ALG_AES_CMAC:
		block_size = mbedtls_cipher_get_block_size(ctx);
		if (block_size > digest_len)
			return TEE_ERROR_SHORT_BUFFER;

		lmd_res = mbedtls_cipher_cmac_finish(ctx, digest);
		if (lmd_res != 0) {
			EMSG("cmac finish failed, res is 0x%x\n", -lmd_res);
			return TEE_ERROR_GENERIC;
		}
		break;
#endif
	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}
	return TEE_SUCCESS;
}
#endif /* _CFG_CRYPTO_WITH_MAC */

/******************************************************************************
 * Authenticated encryption
 ******************************************************************************/

#define TEE_CCM_KEY_MAX_LENGTH		32
#define TEE_CCM_NONCE_MAX_LENGTH	13
#define TEE_CCM_TAG_MAX_LENGTH		16
#define TEE_GCM_TAG_MAX_LENGTH		16

#if defined(CFG_CRYPTO_CCM)
TEE_Result crypto_aes_ccm_alloc_ctx(void **ctx_ret __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

void crypto_aes_ccm_free_ctx(void *ctx __unused)
{
	if (ctx)
		assert(0);
}

void crypto_aes_ccm_copy_state(void *dst_ctx __unused, void *src_ctx __unused)
{
}

TEE_Result crypto_aes_ccm_init(void *ctx __unused,
			       TEE_OperationMode mode __unused,
			       const uint8_t *key __unused,
			       size_t key_len __unused,
			       const uint8_t *nonce __unused,
			       size_t nonce_len __unused,
			       size_t tag_len __unused,
			       size_t aad_len __unused,
			       size_t payload_len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_aes_ccm_update_aad(void *ctx __unused,
			const uint8_t *data __unused, size_t len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_aes_ccm_update_payload(void *ctx __unused,
					 TEE_OperationMode mode __unused,
					 const uint8_t *src_data __unused,
					 size_t len __unused,
					 uint8_t *dst_data __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_aes_ccm_enc_final(void *ctx __unused,
				    const uint8_t *src_data __unused,
				    size_t len __unused,
				    uint8_t *dst_data __unused,
				    uint8_t *dst_tag __unused,
				    size_t *dst_tag_len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_aes_ccm_dec_final(void *ctx __unused,
				    const uint8_t *src_data __unused,
				    size_t len __unused,
				    uint8_t *dst_data __unused,
				    const uint8_t *tag __unused,
				    size_t tag_len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

void crypto_aes_ccm_final(void *ctx __unused)
{
}
#endif /*CFG_CRYPTO_CCM*/

#if defined(CFG_CRYPTO_AES_GCM_FROM_CRYPTOLIB)
TEE_Result crypto_aes_gcm_alloc_ctx(void **ctx_ret __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

void crypto_aes_gcm_free_ctx(void *ctx __unused)
{
	if (ctx)
		assert(0);
}

void crypto_aes_gcm_copy_state(void *dst_ctx __unused, void *src_ctx __unused)
{
	assert(0);
}

TEE_Result crypto_aes_gcm_init(void *ctx __unused,
			       TEE_OperationMode mode __unused,
			       const uint8_t *key __unused,
			       size_t key_len __unused,
			       const uint8_t *nonce __unused,
			       size_t nonce_len __unused,
			       size_t tag_len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_aes_gcm_update_aad(void *ctx __unused,
				const uint8_t *data __unused,
				size_t len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_aes_gcm_update_payload(void *ctx __unused,
					 TEE_OperationMode mode __unused,
					 const uint8_t *src_data __unused,
					 size_t len __unused,
					 uint8_t *dst_data __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_aes_gcm_enc_final(void *ctx __unused,
				    const uint8_t *src_data __unused,
				    size_t len __unused,
				    uint8_t *dst_data __unused,
				    uint8_t *dst_tag __unused,
				    size_t *dst_tag_len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

TEE_Result crypto_aes_gcm_dec_final(void *ctx __unused,
				    const uint8_t *src_data __unused,
				    size_t len __unused,
				    uint8_t *dst_data __unused,
				    const uint8_t *tag __unused,
				    size_t tag_len __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

void crypto_aes_gcm_final(void *ctx __unused)
{
	if (ctx)
		assert(0);
}
#endif /*CFG_CRYPTO_AES_GCM_FROM_CRYPTOLIB*/

/******************************************************************************
 * Pseudo Random Number Generator
 ******************************************************************************/
#if defined(CFG_CRYPTO_CTR_DRBG)
static TEE_Result ctr_drbg_read(void *buf, size_t blen)
{
	TEE_Result res = TEE_SUCCESS;
	int err;
	mbedtls_ctr_drbg_context ctr_drbg;

	mbedtls_ctr_drbg_init(&ctr_drbg);

	err = mbedtls_ctr_drbg_seed(&ctr_drbg, mbd_rand, NULL, NULL, 0);
	if (err != 0) {
		EMSG("failed\n ! mbedtls_ctr_drbg_seed returned 0x%x\n", -err);
		res = TEE_ERROR_SECURITY;
		goto exit;
	}

	err = mbedtls_ctr_drbg_random(&ctr_drbg, buf, blen);
	if (err != 0) {
		EMSG("failed!\n");
		res = TEE_ERROR_BAD_STATE;
		goto exit;
	}

exit:
	mbedtls_ctr_drbg_free(&ctr_drbg);
	return res;
}
#endif

#if defined(CFG_CRYPTO_HMAC_DRBG)
static TEE_Result hmac_drbg_read(void *buf, size_t blen)
{
	TEE_Result res = TEE_SUCCESS;
	int err;
	mbedtls_hmac_drbg_context hmac_drbg;
	const mbedtls_md_info_t *md_info;

	mbedtls_hmac_drbg_init(&hmac_drbg);

#if defined(MBEDTLS_SHA1_C)
	md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
#endif
#if defined(MBEDTLS_SHA256_C)
	md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
#endif

	if (md_info == NULL) {
		EMSG("failed\n ! mbedtls_md_info_from_type return NULL!\n");
		res = TEE_ERROR_BAD_STATE;
		goto exit;
	}
	err = mbedtls_hmac_drbg_seed(&hmac_drbg, md_info,
			mbd_rand, NULL, NULL, 0);
	if (err != 0) {
		EMSG("failed\n ! mbedtls_hmac_drbg_seed returned 0x%x\n", -err);
		res = TEE_ERROR_SECURITY;
		goto exit;
	}

	err = mbedtls_hmac_drbg_random(&hmac_drbg, buf, blen);
	if (err != 0) {
		EMSG("failed!\n");
		res = TEE_ERROR_BAD_STATE;
		goto exit;
	}

exit:
	mbedtls_hmac_drbg_free(&hmac_drbg);
	return res;
}
#endif

TEE_Result crypto_rng_read(void *buf, size_t blen)
{
#if defined(CFG_CRYPTO_CTR_DRBG)
	return ctr_drbg_read(buf, blen);
#elif defined(CFG_CRYPTO_HMAC_DRBG)
	return hmac_drbg_read(buf, blen);
#endif
}

TEE_Result crypto_rng_add_entropy(const uint8_t *inbuf, size_t len)
{
	TEE_Result res = TEE_SUCCESS;
	int err;
	mbedtls_entropy_context entropy;

	mbedtls_entropy_init(&entropy);

	err = mbedtls_entropy_update_manual(&entropy, inbuf, len);
	if (err != 0) {
		EMSG("entropy updatemanual faile, returned 0x%x\n", -err);
		res = TEE_ERROR_SECURITY;
		goto out;
	}
out:
	mbedtls_entropy_free(&entropy);
	return res;
}

TEE_Result crypto_init(void)
{
	return TEE_SUCCESS;
}

#if defined(CFG_CRYPTO_SHA256)
TEE_Result hash_sha256_check(const uint8_t *hash,
		const uint8_t *data, size_t data_size)
{
	mbedtls_sha256_context hs;
	uint8_t digest[TEE_SHA256_HASH_SIZE];

	mbedtls_sha256_init(&hs);
	mbedtls_sha256_starts(&hs, 0);
	mbedtls_sha256_update(&hs, data, data_size);
	mbedtls_sha256_finish(&hs, digest);
	mbedtls_sha256_free(&hs);

	if (buf_compare_ct(digest, hash, sizeof(digest)) != 0)
		return TEE_ERROR_SECURITY;
	return TEE_SUCCESS;
}
#endif

TEE_Result rng_generate(void *buffer, size_t len)
{
#if defined(CFG_WITH_SOFTWARE_PRNG)
#if defined(CFG_CRYPTO_CTR_DRBG)
	return ctr_drbg_read(buffer, len);
#elif defined(CFG_CRYPTO_HMAC_DRBG)
	return hmac_drbg_read(buffer, len);
#endif
#else
	return get_rng_array(buffer, len);
#endif
	return TEE_SUCCESS;
}

TEE_Result crypto_aes_expand_enc_key(const void *key, size_t key_len,
				     void *enc_key, size_t enc_keylen,
				     unsigned int *rounds)
{
	mbedtls_aes_context ctx;

	mbedtls_aes_init(&ctx);
	if (mbedtls_aes_setkey_enc(&ctx, key, key_len * 8) != 0)
		return TEE_ERROR_BAD_PARAMETERS;

	if (enc_keylen > sizeof(ctx.buf))
		return TEE_ERROR_BAD_PARAMETERS;
	memcpy(enc_key, ctx.buf, enc_keylen);
	*rounds = ctx.nr;
	mbedtls_aes_free(&ctx);
	return TEE_SUCCESS;
}

void crypto_aes_enc_block(const void *enc_key, size_t enc_keylen,
			  unsigned int rounds, const void *src, void *dst)
{
	mbedtls_aes_context ctx;

	mbedtls_aes_init(&ctx);
	if (enc_keylen > sizeof(ctx.buf))
		panic();
	memcpy(ctx.buf, enc_key, enc_keylen);
	ctx.rk = ctx.buf;
	ctx.nr = rounds;
	mbedtls_aes_encrypt(&ctx, src, dst);
	mbedtls_aes_free(&ctx);
}
