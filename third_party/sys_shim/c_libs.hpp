#pragma once
// Portable C library interface: uses system -dev headers when installed,
// and falls back to exact ABI-compatible declarations when building against
// runtime shared libraries directly.

#include <cstddef>
#include <cstdint>

// ============================================================================
// 1. SQLite3 C ABI
// ============================================================================
#if __has_include(<sqlite3.h>)
#include <sqlite3.h>
#else
extern "C" {
struct sqlite3;
struct sqlite3_stmt;
typedef int64_t sqlite3_int64;
typedef void (*sqlite3_destructor_type)(void*);

inline constexpr int SQLITE_OK         = 0;
inline constexpr int SQLITE_ERROR      = 1;
inline constexpr int SQLITE_BUSY       = 5;
inline constexpr int SQLITE_LOCKED     = 6;
inline constexpr int SQLITE_CONSTRAINT = 19;
inline constexpr int SQLITE_ROW        = 100;
inline constexpr int SQLITE_DONE       = 101;

inline constexpr int SQLITE_OPEN_READONLY  = 0x00000001;
inline constexpr int SQLITE_OPEN_READWRITE = 0x00000002;
inline constexpr int SQLITE_OPEN_CREATE    = 0x00000004;
inline constexpr int SQLITE_OPEN_NOMUTEX   = 0x00008000;
inline constexpr int SQLITE_OPEN_FULLMUTEX = 0x00010000;

inline constexpr int SQLITE_INTEGER = 1;
inline constexpr int SQLITE_FLOAT   = 2;
inline constexpr int SQLITE_TEXT    = 3;
inline constexpr int SQLITE_BLOB    = 4;
inline constexpr int SQLITE_NULL    = 5;

inline sqlite3_destructor_type const SQLITE_STATIC    = reinterpret_cast<sqlite3_destructor_type>(0);
inline sqlite3_destructor_type const SQLITE_TRANSIENT = reinterpret_cast<sqlite3_destructor_type>(-1);

const char* sqlite3_libversion(void);
int sqlite3_open_v2(const char* filename, sqlite3** ppDb, int flags, const char* zVfs);
int sqlite3_close_v2(sqlite3* db);
int sqlite3_busy_timeout(sqlite3* db, int ms);
int sqlite3_exec(sqlite3* db, const char* sql, int (*callback)(void*, int, char**, char**), void* arg, char** errmsg);
void sqlite3_free(void* ptr);
const char* sqlite3_errmsg(sqlite3* db);
int sqlite3_extended_errcode(sqlite3* db);
sqlite3_int64 sqlite3_last_insert_rowid(sqlite3* db);
int sqlite3_changes(sqlite3* db);

int sqlite3_prepare_v2(sqlite3* db, const char* zSql, int nByte, sqlite3_stmt** ppStmt, const char** pzTail);
int sqlite3_step(sqlite3_stmt* pStmt);
int sqlite3_reset(sqlite3_stmt* pStmt);
int sqlite3_clear_bindings(sqlite3_stmt* pStmt);
int sqlite3_finalize(sqlite3_stmt* pStmt);

int sqlite3_bind_int(sqlite3_stmt* pStmt, int idx, int val);
int sqlite3_bind_int64(sqlite3_stmt* pStmt, int idx, sqlite3_int64 val);
int sqlite3_bind_double(sqlite3_stmt* pStmt, int idx, double val);
int sqlite3_bind_null(sqlite3_stmt* pStmt, int idx);
int sqlite3_bind_text(sqlite3_stmt* pStmt, int idx, const char* val, int n, sqlite3_destructor_type dtor);
int sqlite3_bind_blob(sqlite3_stmt* pStmt, int idx, const void* val, int n, sqlite3_destructor_type dtor);

int sqlite3_column_count(sqlite3_stmt* pStmt);
int sqlite3_column_type(sqlite3_stmt* pStmt, int iCol);
int sqlite3_column_int(sqlite3_stmt* pStmt, int iCol);
sqlite3_int64 sqlite3_column_int64(sqlite3_stmt* pStmt, int iCol);
double sqlite3_column_double(sqlite3_stmt* pStmt, int iCol);
const unsigned char* sqlite3_column_text(sqlite3_stmt* pStmt, int iCol);
const void* sqlite3_column_blob(sqlite3_stmt* pStmt, int iCol);
int sqlite3_column_bytes(sqlite3_stmt* pStmt, int iCol);
}
#endif

// ============================================================================
// 2. OpenSSL 3.0 EVP / HMAC / RAND / CRYPTO C ABI
// ============================================================================
#if __has_include(<openssl/evp.h>)
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#else
extern "C" {
struct evp_cipher_ctx_st;
struct evp_cipher_st;
struct evp_md_ctx_st;
struct evp_md_st;
struct engine_st;

typedef evp_cipher_ctx_st EVP_CIPHER_CTX;
typedef evp_cipher_st EVP_CIPHER;
typedef evp_md_ctx_st EVP_MD_CTX;
typedef evp_md_st EVP_MD;
typedef engine_st ENGINE;

inline constexpr int EVP_CTRL_AEAD_SET_IVLEN = 0x9;
inline constexpr int EVP_CTRL_AEAD_GET_TAG   = 0x10;
inline constexpr int EVP_CTRL_AEAD_SET_TAG   = 0x11;
inline constexpr int EVP_CTRL_GCM_SET_IVLEN  = EVP_CTRL_AEAD_SET_IVLEN;
inline constexpr int EVP_CTRL_GCM_GET_TAG    = EVP_CTRL_AEAD_GET_TAG;
inline constexpr int EVP_CTRL_GCM_SET_TAG    = EVP_CTRL_AEAD_SET_TAG;

const char* OpenSSL_version(int type);
int RAND_bytes(unsigned char* buf, int num);
int CRYPTO_memcmp(const void* a, const void* b, size_t len);
void OPENSSL_cleanse(void* ptr, size_t len);

const EVP_MD* EVP_sha256(void);
const EVP_MD* EVP_sha1(void);
const EVP_MD* EVP_md5(void);
const EVP_CIPHER* EVP_aes_256_gcm(void);

EVP_MD_CTX* EVP_MD_CTX_new(void);
void EVP_MD_CTX_free(EVP_MD_CTX* ctx);
int EVP_DigestInit_ex(EVP_MD_CTX* ctx, const EVP_MD* type, ENGINE* impl);
int EVP_DigestUpdate(EVP_MD_CTX* ctx, const void* d, size_t cnt);
int EVP_DigestFinal_ex(EVP_MD_CTX* ctx, unsigned char* md, unsigned int* s);

unsigned char* HMAC(const EVP_MD* evp_md, const void* key, int key_len,
                    const unsigned char* d, size_t n, unsigned char* md, unsigned int* md_len);

int PKCS5_PBKDF2_HMAC(const char* pass, int passlen,
                      const unsigned char* salt, int saltlen, int iter,
                      const EVP_MD* digest, int keylen, unsigned char* out);

EVP_CIPHER_CTX* EVP_CIPHER_CTX_new(void);
void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX* c);
int EVP_EncryptInit_ex(EVP_CIPHER_CTX* ctx, const EVP_CIPHER* cipher, ENGINE* impl,
                       const unsigned char* key, const unsigned char* iv);
int EVP_EncryptUpdate(EVP_CIPHER_CTX* ctx, unsigned char* out, int* outl,
                      const unsigned char* in, int inl);
int EVP_EncryptFinal_ex(EVP_CIPHER_CTX* ctx, unsigned char* out, int* outl);

int EVP_DecryptInit_ex(EVP_CIPHER_CTX* ctx, const EVP_CIPHER* cipher, ENGINE* impl,
                       const unsigned char* key, const unsigned char* iv);
int EVP_DecryptUpdate(EVP_CIPHER_CTX* ctx, unsigned char* out, int* outl,
                      const unsigned char* in, int inl);
int EVP_DecryptFinal_ex(EVP_CIPHER_CTX* ctx, unsigned char* outm, int* outl);

int EVP_CIPHER_CTX_ctrl(EVP_CIPHER_CTX* ctx, int type, int arg, void* ptr);
}
#endif

// ============================================================================
// 3. libcurl C ABI (Verified against curl 7.88.x / 8.x stable ABI)
// ============================================================================
#if __has_include(<curl/curl.h>)
#include <curl/curl.h>
#else
extern "C" {
typedef void CURL;
typedef int64_t curl_off_t;

struct curl_slist {
    char* data;
    curl_slist* next;
};

typedef int CURLcode;
inline constexpr CURLcode CURLE_OK = 0;
inline constexpr long CURL_GLOBAL_DEFAULT = 3;

typedef int CURLoption;
inline constexpr int CURLOPTTYPE_LONG          = 0;
inline constexpr int CURLOPTTYPE_OBJECTPOINT   = 10000;
inline constexpr int CURLOPTTYPE_FUNCTIONPOINT = 20000;
inline constexpr int CURLOPTTYPE_OFF_T         = 30000;
inline constexpr int CURLOPTTYPE_CBPOINT        = CURLOPTTYPE_OBJECTPOINT;

inline constexpr CURLoption CURLOPT_WRITEDATA          = CURLOPTTYPE_OBJECTPOINT + 1;
inline constexpr CURLoption CURLOPT_URL                = CURLOPTTYPE_OBJECTPOINT + 2;
inline constexpr CURLoption CURLOPT_USERPWD            = CURLOPTTYPE_OBJECTPOINT + 5;
inline constexpr CURLoption CURLOPT_RANGE              = CURLOPTTYPE_OBJECTPOINT + 7;
inline constexpr CURLoption CURLOPT_READDATA           = CURLOPTTYPE_OBJECTPOINT + 9;
inline constexpr CURLoption CURLOPT_ERRORBUFFER        = CURLOPTTYPE_OBJECTPOINT + 10;
inline constexpr CURLoption CURLOPT_WRITEFUNCTION      = CURLOPTTYPE_FUNCTIONPOINT + 11;
inline constexpr CURLoption CURLOPT_READFUNCTION       = CURLOPTTYPE_FUNCTIONPOINT + 12;
inline constexpr CURLoption CURLOPT_POSTFIELDS         = CURLOPTTYPE_OBJECTPOINT + 15;
inline constexpr CURLoption CURLOPT_USERAGENT          = CURLOPTTYPE_OBJECTPOINT + 18;
inline constexpr CURLoption CURLOPT_HTTPHEADER         = CURLOPTTYPE_OBJECTPOINT + 23;
inline constexpr CURLoption CURLOPT_QUOTE              = CURLOPTTYPE_OBJECTPOINT + 28;
inline constexpr CURLoption CURLOPT_HEADERDATA         = CURLOPTTYPE_OBJECTPOINT + 29;
inline constexpr CURLoption CURLOPT_CUSTOMREQUEST      = CURLOPTTYPE_OBJECTPOINT + 36;
inline constexpr CURLoption CURLOPT_NOBODY             = CURLOPTTYPE_LONG + 44;
inline constexpr CURLoption CURLOPT_UPLOAD             = CURLOPTTYPE_LONG + 46;
inline constexpr CURLoption CURLOPT_POST               = CURLOPTTYPE_LONG + 47;
inline constexpr CURLoption CURLOPT_DIRLISTONLY        = CURLOPTTYPE_LONG + 48;
inline constexpr CURLoption CURLOPT_FOLLOWLOCATION     = CURLOPTTYPE_LONG + 52;
inline constexpr CURLoption CURLOPT_SSL_VERIFYPEER     = CURLOPTTYPE_LONG + 64;
inline constexpr CURLoption CURLOPT_MAXREDIRS          = CURLOPTTYPE_LONG + 68;
inline constexpr CURLoption CURLOPT_HEADERFUNCTION     = CURLOPTTYPE_FUNCTIONPOINT + 79;
inline constexpr CURLoption CURLOPT_HTTPGET            = CURLOPTTYPE_LONG + 80;
inline constexpr CURLoption CURLOPT_SSL_VERIFYHOST     = CURLOPTTYPE_LONG + 81;
inline constexpr CURLoption CURLOPT_BUFFERSIZE         = CURLOPTTYPE_LONG + 98;
inline constexpr CURLoption CURLOPT_NOPROGRESS         = CURLOPTTYPE_LONG + 43;
inline constexpr CURLoption CURLOPT_NOSIGNAL           = CURLOPTTYPE_LONG + 99;
inline constexpr CURLoption CURLOPT_XFERINFODATA       = CURLOPTTYPE_CBPOINT + 57;
inline constexpr CURLoption CURLOPT_XFERINFOFUNCTION   = CURLOPTTYPE_FUNCTIONPOINT + 219;
inline constexpr CURLoption CURLOPT_ACCEPT_ENCODING    = CURLOPTTYPE_OBJECTPOINT + 102;
inline constexpr CURLoption CURLOPT_FTP_CREATE_MISSING_DIRS = CURLOPTTYPE_LONG + 110;
inline constexpr CURLoption CURLOPT_INFILESIZE_LARGE   = CURLOPTTYPE_OFF_T + 115;
inline constexpr CURLoption CURLOPT_USE_SSL            = CURLOPTTYPE_LONG + 119;
inline constexpr CURLoption CURLOPT_POSTFIELDSIZE_LARGE = CURLOPTTYPE_OFF_T + 120;
inline constexpr CURLoption CURLOPT_TCP_NODELAY        = CURLOPTTYPE_LONG + 121;
inline constexpr CURLoption CURLOPT_MAX_SEND_SPEED_LARGE = CURLOPTTYPE_OFF_T + 145;
inline constexpr CURLoption CURLOPT_MAX_RECV_SPEED_LARGE = CURLOPTTYPE_OFF_T + 146;
inline constexpr CURLoption CURLOPT_SSH_PRIVATE_KEYFILE = CURLOPTTYPE_OBJECTPOINT + 153;
inline constexpr CURLoption CURLOPT_TIMEOUT_MS         = CURLOPTTYPE_LONG + 155;
inline constexpr CURLoption CURLOPT_CONNECTTIMEOUT_MS  = CURLOPTTYPE_LONG + 156;
inline constexpr CURLoption CURLOPT_USERNAME           = CURLOPTTYPE_OBJECTPOINT + 173;
inline constexpr CURLoption CURLOPT_PASSWORD           = CURLOPTTYPE_OBJECTPOINT + 174;
inline constexpr CURLoption CURLOPT_RESOLVE            = CURLOPTTYPE_OBJECTPOINT + 203;
inline constexpr CURLoption CURLOPT_PROTOCOLS_STR      = CURLOPTTYPE_OBJECTPOINT + 318;
inline constexpr CURLoption CURLOPT_REDIR_PROTOCOLS_STR = CURLOPTTYPE_OBJECTPOINT + 319;

inline constexpr long CURLUSESSL_ALL = 3L;

typedef int CURLINFO;
inline constexpr int CURLINFO_STRING = 0x100000;
inline constexpr int CURLINFO_LONG   = 0x200000;
inline constexpr int CURLINFO_OFF_T  = 0x600000;
inline constexpr CURLINFO CURLINFO_RESPONSE_CODE = CURLINFO_LONG + 2;
inline constexpr CURLINFO CURLINFO_CONTENT_TYPE  = CURLINFO_STRING + 18;
inline constexpr CURLINFO CURLINFO_CONTENT_LENGTH_DOWNLOAD_T = CURLINFO_OFF_T + 15;

CURLcode curl_global_init(long flags);
void curl_global_cleanup(void);
const char* curl_version(void);
CURL* curl_easy_init(void);
void curl_easy_cleanup(CURL* curl);
void curl_easy_reset(CURL* curl);
CURLcode curl_easy_setopt(CURL* curl, CURLoption option, ...);
CURLcode curl_easy_perform(CURL* curl);
CURLcode curl_easy_getinfo(CURL* curl, CURLINFO info, ...);
const char* curl_easy_strerror(CURLcode code);
curl_slist* curl_slist_append(curl_slist* list, const char* data);
void curl_slist_free_all(curl_slist* list);
}
#endif

// ============================================================================
// 4. zlib C ABI (zlib 1.2.x / 1.3.x)
// ============================================================================
#if __has_include(<zlib.h>)
#include <zlib.h>
#define alist_deflateInit2(s, l, m, w, ml, st) deflateInit2((s), (l), (m), (w), (ml), (st))
#define alist_inflateInit2(s, w) inflateInit2((s), (w))
#else
extern "C" {
typedef unsigned char Bytef;
typedef unsigned int uInt;
typedef unsigned long uLong;
typedef void* voidpf;
typedef voidpf (*alloc_func)(voidpf opaque, uInt items, uInt size);
typedef void (*free_func)(voidpf opaque, voidpf address);

struct internal_state;
struct z_stream_s {
    const Bytef* next_in;
    uInt avail_in;
    uLong total_in;
    Bytef* next_out;
    uInt avail_out;
    uLong total_out;
    const char* msg;
    internal_state* state;
    alloc_func zalloc;
    free_func zfree;
    voidpf opaque;
    int data_type;
    uLong adler;
    uLong reserved;
};
typedef z_stream_s z_stream;

inline constexpr int Z_NO_FLUSH      = 0;
inline constexpr int Z_FINISH        = 4;
inline constexpr int Z_OK            = 0;
inline constexpr int Z_STREAM_END    = 1;
inline constexpr int Z_BUF_ERROR     = -5;
inline constexpr int Z_DEFLATED      = 8;
inline constexpr int Z_DEFAULT_STRATEGY = 0;
inline constexpr int MAX_WBITS       = 15;

const char* zlibVersion(void);
uLong crc32(uLong crc, const Bytef* buf, uInt len);
int deflateInit2_(z_stream* strm, int level, int method, int windowBits,
                  int memLevel, int strategy, const char* version, int stream_size);
int deflate(z_stream* strm, int flush);
int deflateEnd(z_stream* strm);
int inflateInit2_(z_stream* strm, int windowBits, const char* version, int stream_size);
int inflate(z_stream* strm, int flush);
int inflateEnd(z_stream* strm);

inline int alist_deflateInit2(z_stream* s, int level, int method, int windowBits, int memLevel, int strategy) {
    return deflateInit2_(s, level, method, windowBits, memLevel, strategy, zlibVersion(), static_cast<int>(sizeof(z_stream)));
}
inline int alist_inflateInit2(z_stream* s, int windowBits) {
    return inflateInit2_(s, windowBits, zlibVersion(), static_cast<int>(sizeof(z_stream)));
}
}
#endif
