#ifndef LZBENCH_H
#define LZBENCH_H

#define _CRT_SECURE_NO_WARNINGS
#define _FILE_OFFSET_BITS 64  // turn off_t into a 64-bit type for ftello() and fseeko()

#include <stdint.h>
#include <string>
#include <vector>

#include "compressors.h"
#include "lizard/lizard_compress.h"    // LIZARD_MAX_CLEVEL
#include "query_common.h"

#define PROGNAME "lzbench"
#define PROGVERSION "1.7.1"
#define PAD_SIZE (16*1024)
#define ALIGN_BYTES 32  // input and output buffs aligned to this many bytes
#define MIN_PAGE_SIZE 4096  // smallest page size we expect, if it's wrong the first algorithm might be a bit slower
// #define DEFAULT_LOOP_TIME (100*1000000)  // 1/10 of a second
#define DEFAULT_LOOP_TIME 0  // 1/10 of a second
#define GET_COMPRESS_BOUND(insize) (insize + 6*insize/15 + PAD_SIZE)
#define LZBENCH_PRINT(level, fmt, ...) if (params->verbose >= level) printf(fmt, __VA_ARGS__)

#define MAX(a,b) ((a)>(b))?(a):(b)
#ifndef MIN
	#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(WIN64) || defined(_WIN64)
	#define WINDOWS
#endif

/* **************************************
*  Compiler Options
****************************************/
#if defined(_MSC_VER)
#  define _CRT_SECURE_NO_WARNINGS    /* Disable some Visual warning messages for fopen, strncpy */
#  define _CRT_SECURE_NO_DEPRECATE   /* VS2005 */
#if _MSC_VER <= 1800                 /* (1800 = Visual Studio 2013) */
#define snprintf sprintf_s       /* snprintf unsupported by Visual <= 2013 */
#endif
#endif

#ifdef WINDOWS
	#include <windows.h>
	typedef LARGE_INTEGER bench_rate_t;
	typedef LARGE_INTEGER bench_timer_t;
	#define InitTimer(rate) if (!QueryPerformanceFrequency(&rate)) { printf("QueryPerformance not present"); };
	#define GetTime(now) QueryPerformanceCounter(&now);
	#define GetDiffTime(rate, start_ticks, end_ticks) (1000000000ULL*(end_ticks.QuadPart - start_ticks.QuadPart)/rate.QuadPart)
	static inline void uni_sleep(UINT milisec) { Sleep(milisec); };
    #ifndef fseeko
		#ifdef _fseeki64
            #define fseeko _fseeki64
            #define ftello _ftelli64
		#else
            #define fseeko fseek
            #define ftello ftell
        #endif
	#endif
	#define PROGOS "Windows"
#else
    #include <stdarg.h> // va_args
	#include <time.h>
	#include <unistd.h>
	#include <sys/resource.h>
	static inline void uni_sleep(uint32_t milisec) { usleep(milisec * 1000); };
#if defined(__APPLE__) || defined(__MACH__)
    #include <mach/mach_time.h>
	typedef mach_timebase_info_data_t bench_rate_t;
    typedef uint64_t bench_timer_t;
	#define InitTimer(rate) mach_timebase_info(&rate);
	#define GetTime(now) now = mach_absolute_time();
	#define GetDiffTime(rate, start_ticks, end_ticks) ((end_ticks - start_ticks) * (uint64_t)rate.numer) / ((uint64_t)rate.denom)
	#define PROGOS "MacOS"
#else
	typedef struct timespec bench_rate_t;
    typedef struct timespec bench_timer_t;
	#define InitTimer(rate)
	#define GetTime(now) if (clock_gettime(CLOCK_MONOTONIC, &now) == -1 ){ printf("clock_gettime error"); };
	#define GetDiffTime(rate, start_ticks, end_ticks) (1000000000ULL*( end_ticks.tv_sec - start_ticks.tv_sec ) + ( end_ticks.tv_nsec - start_ticks.tv_nsec ))
	#define PROGOS "Linux"
#endif
#endif


typedef struct string_table
{
    std::string col1_algname;
    uint64_t col2_ctime, col3_dtime, col4_comprsize, col5_origsize;
    std::string col6_filename;
    string_table(std::string c1, uint64_t c2, uint64_t c3, uint64_t c4, uint64_t c5, std::string filename) : col1_algname(c1), col2_ctime(c2), col3_dtime(c3), col4_comprsize(c4), col5_origsize(c5), col6_filename(filename) {}
} string_table_t;

enum textformat_e { MARKDOWN=1, TEXT, TEXT_FULL, CSV, TURBOBENCH, MARKDOWN2 };
enum timetype_e { FASTEST=1, AVERAGE, MEDIAN };
enum preprocessor_e { DELTA = 1, DELTA2 = 2, DELTA3 = 3, DELTA4 = 4};


class lzbench_params_t {
public:
    int show_speed, compress_only;
    timetype_e timetype;
    textformat_e textformat;
    size_t chunk_size;
    uint32_t c_iters, d_iters, cspeed, verbose, cmintime, dmintime, cloop_time, dloop_time;
    size_t mem_limit;
    int random_read;
    std::vector<string_table_t> results;
    std::vector<int64_t> preprocessors;
    const char* in_filename;
    // int element_sz;
    QueryParams query_params;
    DataInfo data_info;
    int nthreads;
    bool unverified;

    lzbench_params_t(const lzbench_params_t &) = default;
    lzbench_params_t() = default;
};

struct less_using_1st_column { inline bool operator() (const string_table_t& struct1, const string_table_t& struct2) {  return (struct1.col1_algname < struct2.col1_algname); } };
struct less_using_2nd_column { inline bool operator() (const string_table_t& struct1, const string_table_t& struct2) {  return (struct1.col2_ctime > struct2.col2_ctime); } };
struct less_using_3rd_column { inline bool operator() (const string_table_t& struct1, const string_table_t& struct2) {  return (struct1.col3_dtime > struct2.col3_dtime); } };
struct less_using_4th_column { inline bool operator() (const string_table_t& struct1, const string_table_t& struct2) {  return (struct1.col4_comprsize < struct2.col4_comprsize); } };
struct less_using_5th_column { inline bool operator() (const string_table_t& struct1, const string_table_t& struct2) {  return (struct1.col5_origsize < struct2.col5_origsize); } };

typedef int64_t (*compress_func)(char *in, size_t insize, char *out, size_t outsize, size_t, size_t, char*);
typedef char* (*init_func)(size_t insize, size_t, size_t);
typedef void (*deinit_func)(char* workmem);

typedef struct
{
    const char* name;
    const char* version;
    int first_level;
    int last_level;
    int additional_param;
    int max_block_size;
    compress_func compress;
    compress_func decompress;
    init_func init;
    deinit_func deinit;
} compressor_desc_t;


typedef struct
{
    const char* name;
    const char* params;
} alias_desc_t;

#define FASTPFOR_ENTRY(NAME, FUNCNAME) \
    {NAME, "2017-9", 0, 0, 0, 0, lzbench_ ## FUNCNAME ## _compress, lzbench_ ## FUNCNAME ## _decompress, NULL, NULL}


#define LZBENCH_COMPRESSOR_COUNT 116

static const compressor_desc_t comp_desc[LZBENCH_COMPRESSOR_COUNT] =
{
    // { "memcpy",     "",            0,   0,    0,       0, lzbench_return_0,            lzbench_memcpy,                NULL,                    NULL },
    { "memcpy",     "",            0,   0,    0,       0, lzbench_memcpy,            lzbench_memcpy,                NULL,                    NULL },
    { "materialized","",           0,   0,    0,       0, lzbench_memcpy,            lzbench_memcpy,                NULL,                    NULL },
    // General-purpose compressors
    { "brieflz",    "1.1.0",       0,   0,    0,       0, lzbench_brieflz_compress,    lzbench_brieflz_decompress,    lzbench_brieflz_init,    lzbench_brieflz_deinit },
    { "brotli",     "2017-03-10",  0,  11,    0,       0, lzbench_brotli_compress,     lzbench_brotli_decompress,     NULL,                    NULL },
    { "brotli22",   "2017-03-10",  0,  11,   22,       0, lzbench_brotli_compress,     lzbench_brotli_decompress,     NULL,                    NULL },
    { "brotli24",   "2017-03-10",  0,  11,   24,       0, lzbench_brotli_compress,     lzbench_brotli_decompress,     NULL,                    NULL },
    { "crush",      "1.0",         0,   2,    0,       0, lzbench_crush_compress,      lzbench_crush_decompress,      NULL,                    NULL },
    { "csc",        "2016-10-13",  1,   5,    0,       0, lzbench_csc_compress,        lzbench_csc_decompress,        NULL,                    NULL },
    { "density",    "0.12.5 beta", 1,   3,    0,       0, lzbench_density_compress,    lzbench_density_decompress,    NULL,                    NULL }, // decompression error (shortened output)
    { "fastlz",     "0.1",         1,   2,    0,       0, lzbench_fastlz_compress,     lzbench_fastlz_decompress,     NULL,                    NULL },
    { "fse",        "0.9.0",       0,   0,    0,       0, lzbench_fse_compress,        lzbench_fse_decompress,        NULL,                    NULL },
    { "gipfeli",    "2016-07-13",  0,   0,    0,       0, lzbench_gipfeli_compress,    lzbench_gipfeli_decompress,    NULL,                    NULL },
    { "glza",       "0.8",         0,   0,    0,       0, lzbench_glza_compress,       lzbench_glza_decompress,       NULL,                    NULL },
    { "huff0",      "0.9.0",       0,   0,    0, 127<<10, lzbench_huff0_compress,      lzbench_huff0_decompress,      NULL,                    NULL },
    { "libdeflate", "0.7",         1,  12,    0,       0, lzbench_libdeflate_compress, lzbench_libdeflate_decompress, NULL,                    NULL },
    { "lz4",        "1.7.5",       0,   0,    0,       0, lzbench_lz4_compress,        lzbench_lz4_decompress,        NULL,                    NULL },
    { "lz4fast",    "1.7.5",       1,  99,    0,       0, lzbench_lz4fast_compress,    lzbench_lz4_decompress,        NULL,                    NULL },
    { "lz4hc",      "1.7.5",       1,  12,    0,       0, lzbench_lz4hc_compress,      lzbench_lz4_decompress,        NULL,                    NULL },
    { "lizard",     "1.0",  LIZARD_MIN_CLEVEL, LIZARD_MAX_CLEVEL, 0, 0, lzbench_lizard_compress,      lzbench_lizard_decompress,        NULL,                    NULL },
    { "lzf",        "3.6",         0,   1,    0,       0, lzbench_lzf_compress,        lzbench_lzf_decompress,        NULL,                    NULL },
    { "lzfse",      "2017-03-08",  0,   0,    0,       0, lzbench_lzfse_compress,      lzbench_lzfse_decompress,      lzbench_lzfse_init,      lzbench_lzfse_deinit },
    { "lzg",        "1.0.8",       1,   9,    0,       0, lzbench_lzg_compress,        lzbench_lzg_decompress,        NULL,                    NULL },
    { "lzham",      "1.0 -d26",    0,   4,    0,       0, lzbench_lzham_compress,      lzbench_lzham_decompress,      NULL,                    NULL },
    { "lzham22",    "1.0",         0,   4,   22,       0, lzbench_lzham_compress,      lzbench_lzham_decompress,      NULL,                    NULL },
    { "lzham24",    "1.0",         0,   4,   24,       0, lzbench_lzham_compress,      lzbench_lzham_decompress,      NULL,                    NULL },
    { "lzjb",       "2010",        0,   0,    0,       0, lzbench_lzjb_compress,       lzbench_lzjb_decompress,       NULL,                    NULL },
    { "lzlib",      "1.8",         0,   9,    0,       0, lzbench_lzlib_compress,      lzbench_lzlib_decompress,      NULL,                    NULL },
    { "lzma",       "16.04",       0,   9,    0,       0, lzbench_lzma_compress,       lzbench_lzma_decompress,       NULL,                    NULL },
    { "lzmat",      "1.01",        0,   0,    0,       0, lzbench_lzmat_compress,      lzbench_lzmat_decompress,      NULL,                    NULL }, // decompression error (returns 0) and SEGFAULT (?)
    { "lzo1",       "2.09",        1,   1,    0,       0, lzbench_lzo1_compress,       lzbench_lzo1_decompress,       lzbench_lzo_init,        lzbench_lzo_deinit },
    { "lzo1a",      "2.09",        1,   1,    0,       0, lzbench_lzo1a_compress,      lzbench_lzo1a_decompress,      lzbench_lzo_init,        lzbench_lzo_deinit },
    { "lzo1b",      "2.09",        1,   1,    0,       0, lzbench_lzo1b_compress,      lzbench_lzo1b_decompress,      lzbench_lzo_init,        lzbench_lzo_deinit },
    { "lzo1c",      "2.09",        1,   1,    0,       0, lzbench_lzo1c_compress,      lzbench_lzo1c_decompress,      lzbench_lzo_init,        lzbench_lzo_deinit },
    { "lzo1f",      "2.09",        1,   1,    0,       0, lzbench_lzo1f_compress,      lzbench_lzo1f_decompress,      lzbench_lzo_init,        lzbench_lzo_deinit },
    { "lzo1x",      "2.09",        1,   1,    0,       0, lzbench_lzo1x_compress,      lzbench_lzo1x_decompress,      lzbench_lzo_init,        lzbench_lzo_deinit },
    { "lzo1y",      "2.09",        1,   1,    0,       0, lzbench_lzo1y_compress,      lzbench_lzo1y_decompress,      lzbench_lzo_init,        lzbench_lzo_deinit },
    { "lzo1z",      "2.09",      999, 999,    0,       0, lzbench_lzo1z_compress,      lzbench_lzo1z_decompress,      lzbench_lzo_init,        lzbench_lzo_deinit },
    { "lzo2a",      "2.09",      999, 999,    0,       0, lzbench_lzo2a_compress,      lzbench_lzo2a_decompress,      lzbench_lzo_init,        lzbench_lzo_deinit },
    { "lzrw",       "15-Jul-1991", 1,   5,    0,       0, lzbench_lzrw_compress,       lzbench_lzrw_decompress,       lzbench_lzrw_init,       lzbench_lzrw_deinit },
    { "lzsse2",     "2016-05-14",  0,  17,    0,       0, lzbench_lzsse2_compress,     lzbench_lzsse2_decompress,     lzbench_lzsse2_init,     lzbench_lzsse2_deinit },
    { "lzsse4",     "2016-05-14",  0,  17,    0,       0, lzbench_lzsse4_compress,     lzbench_lzsse4_decompress,     lzbench_lzsse4_init,     lzbench_lzsse4_deinit },
    { "lzsse4fast", "2016-05-14",  0,   0,    0,       0, lzbench_lzsse4fast_compress, lzbench_lzsse4_decompress,     lzbench_lzsse4fast_init, lzbench_lzsse4fast_deinit },
    { "lzsse8",     "2016-05-14",  0,  17,    0,       0, lzbench_lzsse8_compress,     lzbench_lzsse8_decompress,     lzbench_lzsse8_init,     lzbench_lzsse8_deinit },
    { "lzsse8fast", "2016-05-14",  0,   0,    0,       0, lzbench_lzsse8fast_compress, lzbench_lzsse8_decompress,     lzbench_lzsse8fast_init, lzbench_lzsse8fast_deinit },
    { "lzvn",       "2017-03-08",  0,   0,    0,       0, lzbench_lzvn_compress,       lzbench_lzvn_decompress,       lzbench_lzvn_init,       lzbench_lzvn_deinit },
    { "pithy",      "2011-12-24",  0,   9,    0,       0, lzbench_pithy_compress,      lzbench_pithy_decompress,      NULL,                    NULL }, // decompression error (returns 0)
    { "quicklz",    "1.5.0",       1,   3,    0,       0, lzbench_quicklz_compress,    lzbench_quicklz_decompress,    NULL,                    NULL },
    { "shrinker",   "0.1",         0,   0,    0, 128<<20, lzbench_shrinker_compress,   lzbench_shrinker_decompress,   NULL,                    NULL },
    { "slz_deflate","1.0.0",       1,   3,    2,       0, lzbench_slz_compress,        lzbench_slz_decompress,        NULL,                    NULL },
    { "slz_gzip",   "1.0.0",       1,   3,    1,       0, lzbench_slz_compress,        lzbench_slz_decompress,        NULL,                    NULL },
    { "slz_zlib",   "1.0.0",       1,   3,    0,       0, lzbench_slz_compress,        lzbench_slz_decompress,        NULL,                    NULL },
    { "snappy",     "1.1.4",       0,   0,    0,       0, lzbench_snappy_compress,     lzbench_snappy_decompress,     NULL,                    NULL },
    { "tornado",    "0.6a",        1,  16,    0,       0, lzbench_tornado_compress,    lzbench_tornado_decompress,    NULL,                    NULL },
    { "ucl_nrv2b",  "1.03",        1,   9,    0,       0, lzbench_ucl_nrv2b_compress,  lzbench_ucl_nrv2b_decompress,  NULL,                    NULL },
    { "ucl_nrv2d",  "1.03",        1,   9,    0,       0, lzbench_ucl_nrv2d_compress,  lzbench_ucl_nrv2d_decompress,  NULL,                    NULL },
    { "ucl_nrv2e",  "1.03",        1,   9,    0,       0, lzbench_ucl_nrv2e_compress,  lzbench_ucl_nrv2e_decompress,  NULL,                    NULL },
    { "wflz",       "2015-09-16",  0,   0,    0,       0, lzbench_wflz_compress,       lzbench_wflz_decompress,       lzbench_wflz_init,       lzbench_wflz_deinit }, // SEGFAULT on decompressiom with gcc 4.9+ -O3 on Ubuntu
    { "xpack",      "2016-06-02",  1,   9,    0,   1<<19, lzbench_xpack_compress,      lzbench_xpack_decompress,      lzbench_xpack_init,      lzbench_xpack_deinit },
    { "xz",         "5.2.3",       0,   9,    0,       0, lzbench_xz_compress,         lzbench_xz_decompress,         NULL,                    NULL },
    { "yalz77",     "2015-09-19",  1,  12,    0,       0, lzbench_yalz77_compress,     lzbench_yalz77_decompress,     NULL,                    NULL },
    { "yappy",      "2014-03-22",  0,  99,    0,       0, lzbench_yappy_compress,      lzbench_yappy_decompress,      lzbench_yappy_init,      NULL },
    { "zlib",       "1.2.11",      1,   9,    0,       0, lzbench_zlib_compress,       lzbench_zlib_decompress,       NULL,                    NULL },
    { "zling",      "2016-01-10",  0,   4,    0,       0, lzbench_zling_compress,      lzbench_zling_decompress,      NULL,                    NULL },
    { "zstd",       "1.1.4",       1,  22,    0,       0, lzbench_zstd_compress,       lzbench_zstd_decompress,       lzbench_zstd_init,       lzbench_zstd_deinit },
    { "zstd22",     "1.1.4",       1,  22,   22,       0, lzbench_zstd_compress,       lzbench_zstd_decompress,       lzbench_zstd_init,       lzbench_zstd_deinit },
    { "zstd24",     "1.1.4",       1,  22,   24,       0, lzbench_zstd_compress,       lzbench_zstd_decompress,       lzbench_zstd_init,       lzbench_zstd_deinit },
    { "nakamichi",  "okamigan",    0,   0,    0,       0, lzbench_nakamichi_compress,  lzbench_nakamichi_decompress,  NULL,                    NULL },
    { "example",    "0.0",         0,   0,    0,       0, lzbench_example_compress,    lzbench_example_decompress,    NULL,                    NULL },
    // Integer compressors
    FASTPFOR_ENTRY("fastpfor", fastpfor),
    FASTPFOR_ENTRY("binarypacking", binarypacking),
    FASTPFOR_ENTRY("optpfor", optpfor),
    FASTPFOR_ENTRY("varintg8iu", varintg8iu),
    FASTPFOR_ENTRY("simple8b", simple8b),
    FASTPFOR_ENTRY("simdgroupsimple", simdgroupsimple),
    { "blosclz",        "1.12.1",     1, 9,   0,       0, lzbench_blosclz_compress,        lzbench_blosclz_decompress,              NULL,       NULL },
    { "blosc_bitshuf8b",  "1.12.1",   1, 9,   1,       0, lzbench_blosc_bitshuf_compress,  lzbench_blosc_bitshuf_decompress,        NULL,       NULL },
    { "blosc_byteshuf8b", "1.12.1",   1, 9,   1,       0, lzbench_blosc_byteshuf_compress, lzbench_blosc_byteshuf_decompress,       NULL,       NULL },
    { "blosc_bitshuf16b",  "1.12.1",  1, 9,   2,       0, lzbench_blosc_bitshuf_compress,  lzbench_blosc_bitshuf_decompress,        NULL,       NULL },
    { "blosc_byteshuf16b", "1.12.1",  1, 9,   2,       0, lzbench_blosc_byteshuf_compress, lzbench_blosc_byteshuf_decompress,       NULL,       NULL },
    { "bbp",       "2017-9-13", 0, 0,   0,       0, lzbench_bbp_compress,            lzbench_bbp_decompress,  lzbench_bbp_init, lzbench_bbp_deinit },
    { "sprintzDelta1d",    "0.0", 0, 0,   0,       0, lzbench_sprintz_delta_1d_compress,        lzbench_sprintz_delta_1d_decompress,        NULL,       NULL },
    { "sprintzDblDelta1d", "0.0", 0, 0,   0,       0, lzbench_sprintz_dbldelta_1d_compress,     lzbench_sprintz_dbldelta_1d_decompress,     NULL,       NULL },
    { "sprintzDynDelta1d", "0.0", 0, 0,   0,       0, lzbench_sprintz_dyndelta_1d_compress,     lzbench_sprintz_dyndelta_1d_decompress,     NULL,       NULL },
    { "sprDelta2",       "0.0", 0, 0,   0,       0, lzbench_sprintz_delta2_compress,       lzbench_sprintz_delta2_decompress,       NULL,       NULL },
    { "sprDeltaRLE",     "0.0", 0, 0,   0,       0, lzbench_sprintz_delta_rle_compress,    lzbench_sprintz_delta_rle_decompress,    NULL,       NULL },
    { "sprDeltaRLE2",    "0.0", 0, 0,   0,       0, lzbench_sprintz_delta_rle2_compress,   lzbench_sprintz_delta_rle2_decompress,   NULL,       NULL },
    { "sprDeltaRLE_FSE", "0.0", 0, 0,   0,       0, lzbench_sprintz_delta_rle_fse_compress,lzbench_sprintz_delta_rle_fse_decompress,NULL, NULL },
    { "sprDeltaRLE_HUF", "0.0", 0, 0,   0,  64<<10, lzbench_sprintz_delta_rle_huf_compress,lzbench_sprintz_delta_rle_huf_decompress,NULL, NULL },
    { "sprDeltaRLE_Zstd","0.0", 1, 22,  0,       0, lzbench_sprintz_delta_rle_zstd_compress,lzbench_sprintz_delta_rle_zstd_decompress, lzbench_zstd_init, lzbench_zstd_deinit },
    { "sprRowMajor",     "0.0", 1, 128, 0,       0, lzbench_sprintz_row_compress,          lzbench_sprintz_row_decompress,          NULL,       NULL },
    { "sprRowDelta",     "0.0", 1, 128, 0,       0, lzbench_sprintz_row_delta_compress,    lzbench_sprintz_row_delta_decompress,    NULL,       NULL },
    { "sprRowDelta_HUF", "0.0", 1, 128, 0,  64<<10, lzbench_sprintz_row_delta_huf_compress,lzbench_sprintz_row_delta_huf_decompress,NULL,       NULL },
    { "sprRowDelta_FSE", "0.0", 1, 128, 0,       0, lzbench_sprintz_row_delta_fse_compress,lzbench_sprintz_row_delta_fse_decompress,NULL,       NULL },
    { "sprRowDeltaRLE",  "0.0", 1, 128, 0,       0, lzbench_sprintz_row_delta_rle_compress,lzbench_sprintz_row_delta_rle_decompress,NULL,       NULL },
    { "sprRowDeltaRLE_lowdim","0.0",1,4,0,       0, lzbench_sprintz_row_delta_rle_lowdim_compress,lzbench_sprintz_row_delta_rle_lowdim_decompress, NULL,       NULL },
    { "sprRowDeltaRLE_HUF","0.0",1,128, 0,  64<<10, lzbench_sprintz_row_delta_rle_huf_compress,lzbench_sprintz_row_delta_rle_huf_decompress,NULL,       NULL },
    // transforms/preproc
    { "sprJustDelta",    "0.0", 1, 128, 0,       0, lzbench_sprintz_delta_encode,          lzbench_sprintz_delta_decode,            NULL,       NULL },
    { "sprJustDblDelta", "0.0", 1, 128, 0,       0, lzbench_sprintz_doubledelta_encode,    lzbench_sprintz_doubledelta_decode,      NULL,       NULL },
    { "sprJustXff",      "0.0", 1, 128, 0,       0, lzbench_sprintz_xff_encode,            lzbench_sprintz_xff_decode,              NULL,       NULL },
    { "sprJustDelta_16b",    "0.0", 1, 128, 0,   0, lzbench_sprintz_delta_encode_16b,      lzbench_sprintz_delta_decode_16b,        NULL,       NULL },
    { "sprJustDblDelta_16b", "0.0", 1, 128, 0,   0, lzbench_sprintz_doubledelta_encode_16b,lzbench_sprintz_doubledelta_decode_16b,  NULL,       NULL },
    { "sprJustXff_16b",      "0.0", 1, 128, 0,   0, lzbench_sprintz_xff_encode_16b,        lzbench_sprintz_xff_decode_16b,          NULL,       NULL },
    // xff funcs
    { "sprXffRLE",       "0.0", 1, 128, 0,       0, lzbench_sprintz_row_xff_rle_compress,  lzbench_sprintz_row_xff_rle_decompress,  NULL,       NULL },
    { "sprXffRLE_lowdim","0.0", 1, 4,   0,       0, lzbench_sprintz_row_xff_rle_lowdim_compress,  lzbench_sprintz_row_xff_rle_lowdim_decompress,  NULL,       NULL },
    // sprintz top-level functions
    { "sprintzDelta",    "0.0", 1, 128, 0,       0, lzbench_sprintz_delta_compress,  lzbench_sprintz_delta_decompress,              NULL,       NULL },
    { "sprintzXff",      "0.0", 1, 128, 0,       0, lzbench_sprintz_xff_compress,  lzbench_sprintz_xff_decompress,                  NULL,       NULL },
    { "sprintzDelta_HUF","0.0", 1, 128, 0,  64<<10, lzbench_sprintz_delta_huf_compress,  lzbench_sprintz_delta_huf_decompress,      NULL,       NULL },
    { "sprintzXff_HUF",  "0.0", 1, 128, 0,  64<<10, lzbench_sprintz_xff_huf_compress,  lzbench_sprintz_xff_huf_decompress,          NULL,       NULL },
    { "sprintzDelta_16b","0.0", 1, 128, 0,       0, lzbench_sprintz_delta_compress_16b,  lzbench_sprintz_delta_decompress_16b,         NULL,    NULL },
    { "sprintzXff_16b",  "0.0", 1, 128, 0,       0, lzbench_sprintz_xff_compress_16b,  lzbench_sprintz_xff_decompress_16b,             NULL,    NULL },
    { "sprintzDelta_HUF_16b","0.0", 1,128,0,80<<10, lzbench_sprintz_delta_huf_compress_16b,  lzbench_sprintz_delta_huf_decompress_16b, NULL,    NULL },
    { "sprintzXff_HUF_16b",  "0.0", 1,128,0,80<<10, lzbench_sprintz_xff_huf_compress_16b,  lzbench_sprintz_xff_huf_decompress_16b,     NULL,    NULL },
    // pushed-down query functions; must be run with -U since they don't write out decompressed data
    { "sprintzDeltaQuery0_8b", "0.0", 1,128,0,80<<10, lzbench_sprintz_delta_compress,  lzbench_sprintz_delta_query0_8b,      NULL,       NULL },
    { "sprintzXffQuery0_16b",  "0.0", 1,128,0,80<<10, lzbench_sprintz_xff_compress_16b,  lzbench_sprintz_xff_query1_16b,    NULL,       NULL },
    // NOTE: the following 2 codecs are unsafe and should only be used for speed profiling
    { "sprFixedBitpack", "0.0", 1, 8,   0,       0, lzbench_fixed_bitpack_compress,  lzbench_fixed_bitpack_decompress,              NULL,       NULL }, // input bytes must all be <= 1
    { "sprJustBitpack",  "0.0", 0, 0,   0,       0, lzbench_just_bitpack_compress,   lzbench_just_bitpack_decompress,               NULL,       NULL }, // input bytes must all be <= 15
};

#undef FASTPFOR_ENTRY


#define LZBENCH_ALIASES_COUNT 12

static const alias_desc_t alias_desc[LZBENCH_ALIASES_COUNT] =
{
    { "fast", "density/fastlz/lizard,10,11,12,13,14/lz4/lz4fast,3,17/lzf/lzfse/lzjb/lzo1b,1/lzo1c,1/lzo1f,1/lzo1x,1/lzo1y,1/" \
              "lzrw,1,3,4,5/lzsse4fast/lzsse8fast/lzvn/pithy,0,3,6,9/quicklz,1,2/shrinker/snappy/tornado,1,2,3/zstd,1,2,3,4,5" }, // default alias
#if !defined(__arm__) && !defined(__aarch64__)
    { "all",  "blosclz,1,3,6,9/brieflz/brotli,0,2,5,8,11/" \
              "crush,0,1,2/csc,1,3,5/density,1,2,3/fastlz,1,2/gipfeli/libdeflate,1,3,6,9,12/lizard,10,12,15,19,20,22,25,29,30,32,35,39,40,42,45,49/lz4/lz4fast,3,17/lz4hc,1,4,9,12/" \
              "lzf,0,1/lzfse/lzg,1,4,6,8/lzham,0,1/lzjb/lzlib,0,3,6,9/lzma,0,2,4,5,9/lzo1/lzo1a/lzo1b,1,3,6,9,99,999/lzo1c,1,3,6,9,99,999/lzo1f/lzo1x/lzo1y/lzo1z/lzo2a/" \
              "lzrw,1,3,4,5/lzsse2,1,6,12,16/lzsse4,1,6,12,16/lzsse8,1,6,12,16/lzvn/pithy,0,3,6,9/quicklz,1,2,3/slz_zlib/snappy/tornado,1,2,3,4,5,6,7,10,13,16/" \
              "ucl_nrv2b,1,6,9/ucl_nrv2d,1,6,9/ucl_nrv2e,1,6,9/xpack,1,6,9/xz,0,3,6,9/yalz77,1,4,8,12/yappy,1,10,100/zlib,1,6,9/zling,0,1,2,3,4/zstd,1,2,5,8,11,15,18,22/" \
              "shrinker/wflz/lzmat" }, // these can SEGFAULT
#else
    { "all",  "blosclz,1,3,6,9/brieflz/brotli,0,2,5,8/" \
              "crush,0,1,2/csc,1,3,5/density,1,2,3/fastlz,1,2/gipfeli/libdeflate,1,3,6,9,12/lizard,10,12,15,20,22,25,30,32,35,40,42,45/lz4/lz4fast,3,17/lz4hc,1,4,9/" \
              "lzf,0,1/lzfse/lzg,1,4,6,8/lzham,0,1/lzjb/lzlib,0,3,6,9/lzma,0,2,4,5/lzo1/lzo1a/lzo1b,1,3,6,9,99,999/lzo1c,1,3,6,9,99,999/lzo1f/lzo1x/lzo1y/lzo1z/lzo2a/" \
              "lzrw,1,3,4,5/lzsse2,1,6,12,16/lzsse4,1,6,12,16/lzsse8,1,6,12,16/lzvn/pithy,0,3,6,9/quicklz,1,2,3/slz_zlib/snappy/tornado,1,2,3,4,5,6,7,10,13,16/" \
              "ucl_nrv2b,1,6,9/ucl_nrv2d,1,6,9/ucl_nrv2e,1,6,9/xpack,1,6,9/xz,0,3,6,9/yalz77,1,4,8,12/yappy,1,10,100/zlib,1,6,9/zling,0,1,2,3,4/zstd,1,2,5,8,11,15,18,22/" \
              "shrinker/wflz/lzmat" }, // these can SEGFAULT
#endif
    { "opt",  "brotli,6,7,8,9,10,11/csc,1,2,3,4,5/lzham,0,1,2,3,4/lzlib,0,1,2,3,4,5,6,7,8,9/lzma,0,1,2,3,4,5,6,7,8,9/" \
              "tornado,5,6,7,8,9,10,11,12,13,14,15,16/xz,1,2,3,4,5,6,7,8,9/zstd,18,19,20,21,22" },
    { "lzo1",  "lzo1,1,99" },
    { "lzo1a", "lzo1a,1,99" },
    { "lzo1b", "lzo1b,1,2,3,4,5,6,7,8,9,99,999" },
    { "lzo1c", "lzo1c,1,2,3,4,5,6,7,8,9,99,999" },
    { "lzo1f", "lzo1f,1,999" },
    { "lzo1x", "lzo1x,1,11,12,15,999" },
    { "lzo1y", "lzo1y,1,999" },
    { "lzo",   "lzo1/lzo1a/lzo1b/lzo1c/lzo1f/lzo1x/lzo1y/lzo1z/lzo2a" },
    { "ucl",   "ucl_nrv2b/ucl_nrv2d/ucl_nrv2e" },
};

// functions used by main.cpp
int lzbench_main(lzbench_params_t* params, const char** inFileNames,
    unsigned ifnIdx, char* encoder_list);
int lzbench_join(lzbench_params_t* params, const char** inFileNames,
    unsigned ifnIdx, char* encoder_list);


#endif
