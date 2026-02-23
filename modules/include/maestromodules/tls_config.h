#ifndef TLS_CONFIG_H
#define TLS_CONFIG_H

/* --- High-Level Mbed TLS Features --- */
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_PROTO_TLS1_3
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_NET_C

/* --- PSA Crypto Core --- */
/* Just enable the core; the build system will deduce the WANT_ALG macros */
#define MBEDTLS_PSA_CRYPTO_C
#define MBEDTLS_PSA_CRYPTO_CONFIG_PKEY
#define MBEDTLS_PSA_CRYPTO_CLIENT

/* --- Platform / Memory Mapping --- */
#define MBEDTLS_PSA_CRYPTO_PLATFORM_MEMORY

#endif /* TLS_CONFIG_H */
