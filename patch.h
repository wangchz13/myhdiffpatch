#include <string.h>

typedef unsigned char 		TByte;
typedef unsigned long long 	hpatch_size_t;
typedef int					hpatch_BOOL;
typedef unsigned int		hpatch_uint;
typedef hpatch_size_t		hpatch_StreamPos_t;
#define     hpatch_FALSE    0
#define     hpatch_TRUE     1
#define     uint08      TByte

static hpatch_BOOL _bytesRle_load(TByte* out_data, TByte* out_dataEnd,
    const TByte* rle_code, const TByte* rle_code_end);

inline static void addData(TByte* dst, const TByte* src, hpatch_size_t length);

uint08 patch( /*const*/ TByte* oldData, /*const*/ TByte* oldData_end, const TByte* serializedDiff, const TByte* serializedDiff_end, hpatch_size_t newDataSize);

hpatch_BOOL hpatch_unpackUIntWithTag(const TByte** src_code, /*const*/ /*TByte* src_code_end,*/
    hpatch_StreamPos_t* result, const hpatch_uint kTagBit);