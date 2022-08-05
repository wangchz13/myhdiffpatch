#include <stdio.h>
#include <stdlib.h>
#include "patch.h"
#include "quicklz.h"

int move_vec[200];
int covers[200][3];
int covers_crc[200] = { 0 };

static const hpatch_uint kSignTagBit=1;
static const hpatch_uint kByteRleType_bit=2;

//byte rle type , ctrl code: high 2bit + packedLen(6bit+...)
typedef enum TByteRleType{
    kByteRleType_rle0  = 0,    //00 rle 0  , data code:0 byte
    kByteRleType_rle255= 1,    //01 rle 255, data code:0 byte
    kByteRleType_rle   = 2,    //10 rle x(1--254), data code:1 byte (save x)
    kByteRleType_unrle = 3     //11 n byte data, data code:n byte(save no rle data)
} TByteRleType;



#define unpackUIntWithTagTo(puint,src_code,/*src_code_end,*/kTagBit) \
        hpatch_unpackUIntWithTag(src_code,/*src_code_end,*/puint,kTagBit)
#define unpackUIntTo(puint,src_code/*,src_code_end*/) \
        unpackUIntWithTagTo(puint,src_code,/*src_code_end,*/0)

void save_data_to_file(unsigned char* buff, int length)
{
    FILE* fp = fopen("dump", "wb");
    fwrite(buff, length, 1, fp);
    fclose(fp);
}
void save_data_to_file2(unsigned char* buff, int length,const char*path)
{
    FILE* fp = fopen(path, "wb");
    fwrite(buff, length, 1, fp);
    fclose(fp);
}

unsigned int CRC_CCITT(unsigned char* arr_buff, unsigned int len)
{
    unsigned int crc = 0x0000;
    unsigned int i, j;
    for (i = 0; i < len; i++) {
        crc = ((crc & 0xFF00) | (crc & 0x00FF) ^ (arr_buff[i] & 0xFF));
        for (j = 0; j < 8; j++) {
            if ((crc & 0x0001) > 0) {
                crc = crc >> 1;
                crc = crc ^ 0x8408;
            }
            else
                crc = crc >> 1;
        }
    }
    return crc;
}

hpatch_BOOL hpatch_unpackUIntWithTag(const TByte** src_code,/*const TByte* src_code_end,*/
                                     hpatch_StreamPos_t* result,const hpatch_uint kTagBit)
{
    hpatch_StreamPos_t  value;
    TByte               code;
    const TByte*        pcode=*src_code;

    code=*pcode; ++pcode;
    value=code&((1<<(7-kTagBit))-1);
        if ((code&(1<<(7-kTagBit)))!=0){
        do {
            code=*pcode; ++pcode;
            value=(value<<7) | (code&((1<<7)-1));
        } while ((code&(1<<7))!=0);
    }
    (*src_code)=pcode;
    *result=value;
    return hpatch_TRUE;
}


//hpatch_size_t getNewDataSize(const TByte** serializedDiff, const TByte* serializedDiff_end)
//{
//    const TByte* temp = *serializedDiff;
//    const TByte* temp_end = serializedDiff_end;
//    hpatch_size_t newSize;
//    unpackUIntTo(&newSize, &temp, temp_end);
//    *serializedDiff = temp;
//    return newSize;
//}

uint08 patch(/*const*/ TByte* oldData,/*const*/ TByte* oldData_end, const TByte* serializedDiff, const TByte* serializedDiff_end, hpatch_size_t newDataSize)
{
    const TByte* code_lengths, * code_lengths_end,
        * code_inc_newPos, * code_inc_newPos_end,
        * code_inc_oldPos, * code_inc_oldPos_end,
        * code_newDataDiff, * code_newDataDiff_end;
    hpatch_size_t coverCount;
    hpatch_size_t move_flag;
    // assert_param(out_newData<=out_newData_end);
    // assert_param(oldData<=oldData_end);
    // assert_param(serializedDiff<=serializedDiff_end);

    unpackUIntTo(&coverCount, &serializedDiff);
    unpackUIntTo(&move_flag, &serializedDiff);
    if (move_flag == 1)
    {
        //for (int i = 0; i < coverCount; i++)
        //{
        //    unpackUIntTo(&temp, &serializedDiff, serializedDiff_end);
        //    move_vec[i] = temp;
        //}
        for (int i = 0; i < coverCount; i++)
        {
            unpackUIntTo(&covers[i][0], &serializedDiff);
            //covers[i][0] = temp;
            unpackUIntTo(&covers[i][1], &serializedDiff);
            //covers[i][1] = temp;
            unpackUIntTo(&covers[i][2], &serializedDiff);
            //covers[i][2] = temp;
            covers_crc[i] = CRC_CCITT(oldData + covers[i][0], covers[i][2]);
        }
    }
    else
    {
        return hpatch_FALSE;
    }

    {
        hpatch_size_t lengthSize, inc_newPosSize, inc_oldPosSize, newDataDiffSize;
        unpackUIntTo(&lengthSize, &serializedDiff);
        unpackUIntTo(&inc_newPosSize, &serializedDiff);
        unpackUIntTo(&inc_oldPosSize, &serializedDiff);
        unpackUIntTo(&newDataDiffSize, &serializedDiff);

        code_lengths = serializedDiff;     serializedDiff += lengthSize;
        code_lengths_end = serializedDiff;

        code_inc_newPos = serializedDiff; serializedDiff += inc_newPosSize;
        code_inc_newPos_end = serializedDiff;

        code_inc_oldPos = serializedDiff; serializedDiff += inc_oldPosSize;
        code_inc_oldPos_end = serializedDiff;

        code_newDataDiff = serializedDiff; serializedDiff += newDataDiffSize;
        code_newDataDiff_end = serializedDiff;
    }
    //hpatch_BOOL bytesRleload = _bytesRle_load(out_newData, out_newData_end, serializedDiff, serializedDiff_end);
    {   //patch
        //const hpatch_size_t newDataSize = (hpatch_size_t)(out_newData_end - out_newData);
        hpatch_size_t oldPosBack = 0;
        hpatch_size_t newPosBack = 0;
        hpatch_size_t i;
        for (i = 0; i < coverCount; ++i)
        {
            memmove(oldData + covers[i][1], oldData + covers[i][0], covers[i][2]);
            
        }
        for (i = 0; i < coverCount; i++)
        {
            int crc = CRC_CCITT(oldData + covers[i][1], covers[i][2]);
            if (crc != covers_crc[i])
            {
                printf("copy covers %d error\n", (int)i);
                return hpatch_FALSE;
            }
        }
        save_data_to_file2(oldData, newDataSize, "movedump");
        for (i = 0; i < coverCount; ++i) {
            hpatch_size_t copyLength, addLength, oldPos, inc_oldPos, inc_oldPos_sign;
            unpackUIntTo(&copyLength, &code_inc_newPos);
            unpackUIntTo(&addLength, &code_lengths);

            inc_oldPos_sign = (*code_inc_oldPos) >> (8 - kSignTagBit);
            unpackUIntWithTagTo(&inc_oldPos, &code_inc_oldPos, kSignTagBit);
            if (inc_oldPos_sign == 0)
                oldPos = oldPosBack + inc_oldPos;
            else
                oldPos = oldPosBack - inc_oldPos;

            if (copyLength > 0) {
                if (move_flag) {
                    memcpy(oldData + newPosBack, code_newDataDiff, copyLength);
                }
                else
                {
                    //memcpy(out_newData + newPosBack, code_newDataDiff, copyLength);
                }

                code_newDataDiff += copyLength;
                newPosBack += copyLength;
            }

            //move
            if (move_flag)
            {
                
                //addData(oldData + newPosBack, out_newData + newPosBack, addLength);
                oldPosBack = oldPos;
                newPosBack += addLength;
            }
            else
            {
                //addData(out_newData + newPosBack, oldData + oldPos, addLength);
                oldPosBack = oldPos;
                newPosBack += addLength;
            }
            //move
        }

        if (newPosBack < newDataSize) {
            hpatch_size_t copyLength = newDataSize - newPosBack;
            if (move_flag)
            {
                memcpy(oldData + newPosBack, code_newDataDiff, copyLength);
            }
            else
            {
                //memcpy(out_newData + newPosBack, code_newDataDiff, copyLength);

            }
            code_newDataDiff += copyLength;
            newPosBack = newDataSize;
        }
        bytesRle(oldData, oldData + newDataSize, serializedDiff, serializedDiff_end);
    }

    if ((code_lengths == code_lengths_end)
        && (code_inc_newPos == code_inc_newPos_end)
        && (code_inc_oldPos == code_inc_oldPos_end)
        && (code_newDataDiff == code_newDataDiff_end))
        return hpatch_TRUE;
    else
        return hpatch_FALSE;
}

//
//uint08 patch0(TByte* out_newData,TByte* out_newData_end,/*const*/ TByte* oldData,/*const*/ TByte* oldData_end,const TByte* serializedDiff,const TByte* serializedDiff_end)
//{
//    const TByte *code_lengths, *code_lengths_end,
//                *code_inc_newPos, *code_inc_newPos_end,
//                *code_inc_oldPos, *code_inc_oldPos_end,
//                *code_newDataDiff, *code_newDataDiff_end;
//    hpatch_size_t coverCount;
//    hpatch_size_t move_flag,temp;
//    // assert_param(out_newData<=out_newData_end);
//    // assert_param(oldData<=oldData_end);
//    // assert_param(serializedDiff<=serializedDiff_end);
//    
//    unpackUIntTo(&coverCount,&serializedDiff,serializedDiff_end);
//    unpackUIntTo(&move_flag, &serializedDiff, serializedDiff_end);
//    if (move_flag == 1)
//    {
//        for (int i = 0; i < coverCount; i++)
//        {
//            unpackUIntTo(&temp, &serializedDiff, serializedDiff_end);
//            move_vec[i] = temp;
//        }
//    }
//
//    {
//        hpatch_size_t lengthSize,inc_newPosSize,inc_oldPosSize,newDataDiffSize;
//        unpackUIntTo(&lengthSize,&serializedDiff, serializedDiff_end);
//        unpackUIntTo(&inc_newPosSize,&serializedDiff, serializedDiff_end);
//        unpackUIntTo(&inc_oldPosSize,&serializedDiff, serializedDiff_end);
//        unpackUIntTo(&newDataDiffSize,&serializedDiff, serializedDiff_end);
//
//        code_lengths=serializedDiff;     serializedDiff+=lengthSize;
//        code_lengths_end=serializedDiff;
//
//        code_inc_newPos=serializedDiff; serializedDiff+=inc_newPosSize;
//        code_inc_newPos_end=serializedDiff;
//
//        code_inc_oldPos=serializedDiff; serializedDiff+=inc_oldPosSize;
//        code_inc_oldPos_end=serializedDiff;
//
//        code_newDataDiff=serializedDiff; serializedDiff+=newDataDiffSize;
//        code_newDataDiff_end=serializedDiff;
//    }
//    hpatch_BOOL bytesRleload =  _bytesRle_load(out_newData, out_newData_end, serializedDiff, serializedDiff_end);
//    {   //patch
//        const hpatch_size_t newDataSize=(hpatch_size_t)(out_newData_end-out_newData);
//        hpatch_size_t oldPosBack=0;
//        hpatch_size_t newPosBack=0;
//        hpatch_size_t i;
//        for (i=0; i<coverCount; ++i){
//            hpatch_size_t copyLength,addLength, oldPos,inc_oldPos,inc_oldPos_sign;
//            unpackUIntTo(&copyLength,&code_inc_newPos, code_inc_newPos_end);
//            unpackUIntTo(&addLength,&code_lengths, code_lengths_end);
//
//            inc_oldPos_sign=(*code_inc_oldPos)>>(8-kSignTagBit);
//            unpackUIntWithTagTo(&inc_oldPos,&code_inc_oldPos, code_inc_oldPos_end, kSignTagBit);
//            if (inc_oldPos_sign==0)
//                oldPos=oldPosBack+inc_oldPos;
//            else
//                oldPos=oldPosBack-inc_oldPos;
//
//            if (copyLength>0){
//                if (move_flag) {
//                    memcpy(oldData + newPosBack, code_newDataDiff, copyLength);
//                }
//                else
//                {
//                    memcpy(out_newData + newPosBack, code_newDataDiff, copyLength);
//                }
//                
//                code_newDataDiff+=copyLength;
//                newPosBack+=copyLength;
//            }
//
//            //move
//            if (move_flag)
//            {
//                memmove(oldData + newPosBack, oldData + oldPos, addLength);
//                addData(oldData + oldPos, out_newData + newPosBack, addLength);
//                oldPosBack = oldPos;
//                newPosBack += addLength;
//            }
//            else
//            {
//                addData(out_newData + newPosBack, oldData + oldPos, addLength);
//                oldPosBack = oldPos;
//                newPosBack += addLength;
//            }
//            //move
//        }
//
//        if (newPosBack<newDataSize){
//            hpatch_size_t copyLength=newDataSize-newPosBack;
//
//            memcpy(out_newData+newPosBack,code_newDataDiff,copyLength);
//            code_newDataDiff+=copyLength;
//            newPosBack=newDataSize;
//        }
//    }
//
//    if (  (code_lengths==code_lengths_end)
//        &&(code_inc_newPos==code_inc_newPos_end)
//        &&(code_inc_oldPos==code_inc_oldPos_end)
//        &&(code_newDataDiff==code_newDataDiff_end))
//        return hpatch_TRUE;
//    else
//        return hpatch_FALSE;
//}

inline static void addData(TByte* dst,const TByte* src,hpatch_size_t length){
    while (length--) {
        *dst++ += *src++;
    }
}

static hpatch_BOOL bytesRle(TByte* out_data, TByte* out_dataEnd,
    const TByte* rle_code, const TByte* rle_code_end)
{
    const TByte* ctrlBuf, * ctrlBuf_end;
    hpatch_size_t ctrlSize;
    unpackUIntTo(&ctrlSize, &rle_code);

    ctrlBuf = rle_code;
    rle_code += ctrlSize;
    ctrlBuf_end = rle_code;
    while (ctrlBuf_end - ctrlBuf > 0) {
        enum TByteRleType type = (enum TByteRleType)((*ctrlBuf) >> (8 - kByteRleType_bit));
        hpatch_size_t length;
        unpackUIntWithTagTo(&length, &ctrlBuf, kByteRleType_bit);

        ++length;
        switch (type) {
        case kByteRleType_rle0: {
            //memset(out_data, 0, length);
            out_data += length;
        }break;
        case kByteRleType_rle255: {
            //memset(out_data, 255, length);
            for (int i = 0; i < length; i++)
                out_data[i] += 255;
            out_data += length;
        }break;
        case kByteRleType_rle: {

            //memset(out_data, *rle_code, length);
            for (int i = 0; i < length; i++)
                out_data[i] += *rle_code;
            ++rle_code;
            out_data += length;
        }break;
        case kByteRleType_unrle: {

            //memcpy(out_data, rle_code, length);
            for (int i = 0; i < length; i++)
                out_data[i] += rle_code[i];
            rle_code += length;
            out_data += length;
        }break;
        }
    }

    if ((ctrlBuf == ctrlBuf_end)
        && (rle_code == rle_code_end)
        && (out_data == out_dataEnd))
        return hpatch_TRUE;
    else
        return hpatch_FALSE;
}


static hpatch_BOOL _bytesRle_load(TByte* out_data,TByte* out_dataEnd,
                                  const TByte* rle_code,const TByte* rle_code_end)
{
    const TByte*    ctrlBuf,*ctrlBuf_end;
    hpatch_size_t ctrlSize;
    unpackUIntTo(&ctrlSize,&rle_code);

    ctrlBuf=rle_code;
    rle_code+=ctrlSize;
    ctrlBuf_end=rle_code;
    while (ctrlBuf_end-ctrlBuf>0){
        enum TByteRleType type=(enum TByteRleType)((*ctrlBuf)>>(8-kByteRleType_bit));
        hpatch_size_t length;
        unpackUIntWithTagTo(&length,&ctrlBuf,kByteRleType_bit);

        ++length;
        switch (type){
            case kByteRleType_rle0:{
                memset(out_data,0,length);
                out_data+=length;
            }break;
            case kByteRleType_rle255:{
                memset(out_data,255,length);
                out_data+=length;
            }break;
            case kByteRleType_rle:{

                memset(out_data,*rle_code,length);
                ++rle_code;
                out_data+=length;
            }break;
            case kByteRleType_unrle:{

                memcpy(out_data,rle_code,length);
                rle_code+=length;
                out_data+=length;
            }break;
        }
    }
    
    if (  (ctrlBuf==ctrlBuf_end)
        &&(rle_code==rle_code_end)
        &&(out_data==out_dataEnd))
        return hpatch_TRUE;
    else
        return hpatch_FALSE;
}


static size_t getFileSize(FILE* file) {
    fseek(file, 0, SEEK_END);
    size_t read_len = ftell(file);
    fseek(file, 0, SEEK_SET);
    return read_len;
}
 


int main(int argc, char const* argv[])
{

    const char* oldname = argv[1];//led1.hex
    const char* patchname = argv[2];//patch
    const char* newname = argv[3];//myled2.hex

    TByte* oldData = 0;
    TByte* patchData = 0;
    TByte* raw_patch;
    TByte* patchData_temp;

    FILE* oldfile = fopen(oldname, "rb");
    if (oldfile == NULL)
    {
        printf("failed to open old file.\n");
        return 1;
    }
    FILE* patchfile = fopen(patchname, "rb");
    if (patchfile == NULL) 
    { 
        printf("failed to open patch file.\n");
        return 2;
    }

   
    


    FILE* newfile = fopen(newname, "wb");

    size_t newsize,oldsize;
    size_t oldfilesize = getFileSize(oldfile);

    unsigned int newcrc, oldcrc,crc;
    size_t patchsize = getFileSize(patchfile);
    

    

    
    patchData = (TByte*)malloc(50*1024);
    

    { //½âÑ¹patch

        char patch_crc[2];
        fread(patch_crc, 1, 1, patchfile);
        fread(patch_crc+1, 1, 1, patchfile);
        char* file_data, * decompressed;
        TByte* temp = patchData;
        size_t d, c;
        qlz_state_decompress* state_decompress = (qlz_state_decompress*)malloc(sizeof(qlz_state_decompress));
        file_data = (char*)malloc(4096);

        // allocate decompression buffer
        decompressed = (char*)malloc(3696);
        memset(state_decompress, 0, sizeof(qlz_state_decompress));
        while ((c = fread(file_data, 1, 9, patchfile)) != 0)
        {
            c = qlz_size_compressed(file_data);
            fread(file_data + 9, 1, c - 9, patchfile);
            d = qlz_decompress(file_data, decompressed, state_decompress);
            printf("%u bytes decompressed into %u.\n", (unsigned int)c, (unsigned int)d);
            memcpy(temp, decompressed, d);
            temp += d;
        }


    }
    
    //fread(patchData, 1, patchsize, patchfile);

    patchData_temp = patchData;
    unpackUIntTo(&newsize, &patchData_temp);
    //newsize = getNewDataSize(&patchData_temp, patchData + patchsize);

    oldData = (TByte*)malloc(oldfilesize>newsize? oldfilesize:newsize);
    fread(oldData, 1, oldfilesize, oldfile);
    // oldsize = getNewDataSize(&patchData_temp, patchData + patchsize);
    unpackUIntTo(&oldsize, &patchData_temp);
    //newcrc = getNewDataSize(&patchData_temp, patchData + patchsize);
    unpackUIntTo(&newcrc, &patchData_temp);
    //oldcrc = getNewDataSize(&patchData_temp, patchData + patchsize);
    unpackUIntTo(&oldcrc, &patchData_temp);
    if (oldfilesize != oldsize)
    {
        printf("old file size error!\n");
        return 3;
    }
    crc = CRC_CCITT(oldData, oldfilesize);
    if (crc != oldcrc)
    {
        printf("old file crc error!\n");
        return 4;
    }

    printf("new file size:%d\n", (int)newsize);
    printf("old file size:%d\n", (int)oldsize);
    printf("new file crc:%04X\n", newcrc);
    printf("old file crc:%04X\n", oldcrc);
    //TByte* newData = (TByte*)malloc(newsize);

    uint08 ret = patch( oldData, oldData + oldsize, patchData_temp, 0, newsize);

    save_data_to_file(oldData, newsize);
    //crc = CRC_CCITT(newData, newsize);
    oldcrc = CRC_CCITT(oldData, newsize);
    if (crc != newcrc)
    {
        printf("new file crc error!");
        //return 5;
    }

    int crcs[200];
    if (newcrc == oldcrc)
    {
        for (int i = 0; i < 200; i++)
        {
            crcs[i] = CRC_CCITT(oldData + i * 512, 512);
        }

        printf("³É¹¦£¡£¡£¡!\n");
    }
    else
    {
        printf("Ê§°Ü£¡£¡£¡!\n");
    }
   // getchar();
    //fwrite(oldData, 1, newsize, newfile);
    fclose(oldfile);
    fclose(patchfile);
    fclose(newfile);
    return 0;
}
