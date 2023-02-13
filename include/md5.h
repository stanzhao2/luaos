

#ifndef __INC_MD5_H_
#define __INC_MD5_H_

////////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

////////////////////////////////////////////////////////////////////////////////
namespace md5{ //namespace md5 begin.
////////////////////////////////////////////////////////////////////////////////
namespace basic{ //namespace basic begin.
////////////////////////////////////////////////////////////////////////////////
// MD5基本位操作函数
#define D_F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define D_G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define D_H(x, y, z) ((x) ^ (y) ^ (z))
#define D_I(x, y, z) ((y) ^ ((x) | (~z)))
// 位循环左移位操作
#define ROL(x, n)   (((x) << (n)) | ((x) >> (32-(n))))

// ---------变换操作----------
// 第一轮变换基本操作
#define FF(a, b, c, d, x, s, t) { \
    (a) += D_F((b), (c), (d)) + (x) + (t); \
    (a) = ROL((a), (s)); \
    (a) += (b); \
}
// 第二轮变换基本操作
#define GG(a, b, c, d, x, s, t) { \
    (a) += D_G((b), (c), (d)) + (x) + (t); \
    (a) = ROL((a), (s)); \
    (a) += (b); \
}
// 第三轮变换基本操作
#define HH(a, b, c, d, x, s, t) { \
    (a) += D_H((b), (c), (d)) + (x) + (t); \
    (a) = ROL((a), (s)); \
    (a) += (b); \
}
// 第四轮变换基本操作
#define II(a, b, c, d, x, s, t) { \
    (a) += D_I((b), (c), (d)) + (x) + (t); \
    (a) = ROL((a), (s)); \
    (a) += (b); \
}
typedef struct{unsigned int p[4], q[4][16];} md5_data;

/* ------------------------------------------------
        MD5关键参数，修改即可形成不同的变体
   ------------------------------------------------ */
static const md5_data md5_params = {
    // 初始状态
    { 0x67452301,0xEFCDAB89,0x98BADCFE,0x10325476},
    {{0xD76AA478,0xE8C7B756,0x242070DB,0xC1BDCEEE,
      0xF57C0FAF,0x4787C62A,0xA8304613,0xFD469501,
      0x698098D8,0x8B44F7AF,0xFFFF5BB1,0x895CD7BE,
      0x6B901122,0xFD987193,0xA679438E,0x49B40821},
     {0xF61E2562,0xC040B340,0x265E5A51,0xE9B6C7AA,
      0xD62F105D,0x02441453,0xD8A1E681,0xE7D3FBC8,
      0x21E1CDE6,0xC33707D6,0xF4D50D87,0x455A14ED,
      0xA9E3E905,0xFCEFA3F8,0x676F02D9,0x8D2A4C8A},
     {0xFFFA3942,0x8771F681,0x6D9D6122,0xFDE5380C,
      0xA4BEEA44,0x4BDECFA9,0xF6BB4B60,0xBEBFBC70,
      0x289B7EC6,0xEAA127FA,0xD4EF3085,0x04881D05,
      0xD9D4D039,0xE6DB99E5,0x1FA27CF8,0xC4AC5665},
     {0xF4292244,0x432AFF97,0xAB9423A7,0xFC93A039,
      0x655B59C3,0x8F0CCC92,0xFFEFF47D,0x85845DD1,
      0x6FA87E4F,0xFE2CE6E0,0xA3014314,0x4E0811A1,
      0xF7537E82,0xBD3AF235,0x2AD7D2BB,0xEB86D391}}
};
// 变换操作移位表
static const unsigned char shift_table[][4] = {
    {7,12,17,22},{5,9,14,20},{4,11,16,23},{6,10,15,21}
};
// 填充数据
static const unsigned char md_padding[] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
typedef struct _md5_ctx{
    unsigned int  status[4];	// 记录数据的变化状态
    unsigned int  count[2];		// 记录数据的原始长度(以bit为单位)
    unsigned char buffer[64];	// 原始数据
} md5_ctx;
////////////////////////////////////////////////////////////////////////////////
static void md5_init(md5_ctx* p_context)
{
    const unsigned int *origin_state;
    origin_state = md5_params.p;
    p_context->status[0] = origin_state[0];
    p_context->status[1] = origin_state[1];
    p_context->status[2] = origin_state[2];
    p_context->status[3] = origin_state[3];
    p_context->count[0] = p_context->count[1] = 0;
}
// MD5基本变换操作
static void md5_transform(unsigned int* p_state, unsigned int* px)
{
    const unsigned int (*off_table)[16];
    unsigned int a,b,c,d;

    off_table = md5_params.q;
    a = p_state[0], b = p_state[1], c = p_state[2], d = p_state[3];

    // 第一轮变换
    FF(a, b, c, d, px[ 0], shift_table[0][0], off_table[0][ 0]);
    FF(d, a, b, c, px[ 1], shift_table[0][1], off_table[0][ 1]);
    FF(c, d, a, b, px[ 2], shift_table[0][2], off_table[0][ 2]);
    FF(b, c, d, a, px[ 3], shift_table[0][3], off_table[0][ 3]);
    FF(a, b, c, d, px[ 4], shift_table[0][0], off_table[0][ 4]);
    FF(d, a, b, c, px[ 5], shift_table[0][1], off_table[0][ 5]);
    FF(c, d, a, b, px[ 6], shift_table[0][2], off_table[0][ 6]);
    FF(b, c, d, a, px[ 7], shift_table[0][3], off_table[0][ 7]);
    FF(a, b, c, d, px[ 8], shift_table[0][0], off_table[0][ 8]);
    FF(d, a, b, c, px[ 9], shift_table[0][1], off_table[0][ 9]);
    FF(c, d, a, b, px[10], shift_table[0][2], off_table[0][10]);
    FF(b, c, d, a, px[11], shift_table[0][3], off_table[0][11]);
    FF(a, b, c, d, px[12], shift_table[0][0], off_table[0][12]);
    FF(d, a, b, c, px[13], shift_table[0][1], off_table[0][13]);
    FF(c, d, a, b, px[14], shift_table[0][2], off_table[0][14]);
    FF(b, c, d, a, px[15], shift_table[0][3], off_table[0][15]);

    // 第二轮变换
    GG(a, b, c, d, px[ 1], shift_table[1][0], off_table[1][ 0]);
    GG(d, a, b, c, px[ 6], shift_table[1][1], off_table[1][ 1]);
    GG(c, d, a, b, px[11], shift_table[1][2], off_table[1][ 2]);
    GG(b, c, d, a, px[ 0], shift_table[1][3], off_table[1][ 3]);
    GG(a, b, c, d, px[ 5], shift_table[1][0], off_table[1][ 4]);
    GG(d, a, b, c, px[10], shift_table[1][1], off_table[1][ 5]);
    GG(c, d, a, b, px[15], shift_table[1][2], off_table[1][ 6]);
    GG(b, c, d, a, px[ 4], shift_table[1][3], off_table[1][ 7]);
    GG(a, b, c, d, px[ 9], shift_table[1][0], off_table[1][ 8]);
    GG(d, a, b, c, px[14], shift_table[1][1], off_table[1][ 9]);
    GG(c, d, a, b, px[ 3], shift_table[1][2], off_table[1][10]);
    GG(b, c, d, a, px[ 8], shift_table[1][3], off_table[1][11]);
    GG(a, b, c, d, px[13], shift_table[1][0], off_table[1][12]);
    GG(d, a, b, c, px[ 2], shift_table[1][1], off_table[1][13]);
    GG(c, d, a, b, px[ 7], shift_table[1][2], off_table[1][14]);
    GG(b, c, d, a, px[12], shift_table[1][3], off_table[1][15]);

    // 第三轮变换
    HH(a, b, c, d, px[ 5], shift_table[2][0], off_table[2][ 0]);
    HH(d, a, b, c, px[ 8], shift_table[2][1], off_table[2][ 1]);
    HH(c, d, a, b, px[11], shift_table[2][2], off_table[2][ 2]);
    HH(b, c, d, a, px[14], shift_table[2][3], off_table[2][ 3]);
    HH(a, b, c, d, px[ 1], shift_table[2][0], off_table[2][ 4]);
    HH(d, a, b, c, px[ 4], shift_table[2][1], off_table[2][ 5]);
    HH(c, d, a, b, px[ 7], shift_table[2][2], off_table[2][ 6]);
    HH(b, c, d, a, px[10], shift_table[2][3], off_table[2][ 7]);
    HH(a, b, c, d, px[13], shift_table[2][0], off_table[2][ 8]);
    HH(d, a, b, c, px[ 0], shift_table[2][1], off_table[2][ 9]);
    HH(c, d, a, b, px[ 3], shift_table[2][2], off_table[2][10]);
    HH(b, c, d, a, px[ 6], shift_table[2][3], off_table[2][11]);
    HH(a, b, c, d, px[ 9], shift_table[2][0], off_table[2][12]);
    HH(d, a, b, c, px[12], shift_table[2][1], off_table[2][13]);
    HH(c, d, a, b, px[15], shift_table[2][2], off_table[2][14]);
    HH(b, c, d, a, px[ 2], shift_table[2][3], off_table[2][15]);

    // 第四轮变换
    II(a, b, c, d, px[ 0], shift_table[3][0], off_table[3][ 0]);
    II(d, a, b, c, px[ 7], shift_table[3][1], off_table[3][ 1]);
    II(c, d, a, b, px[14], shift_table[3][2], off_table[3][ 2]);
    II(b, c, d, a, px[ 5], shift_table[3][3], off_table[3][ 3]);
    II(a, b, c, d, px[12], shift_table[3][0], off_table[3][ 4]);
    II(d, a, b, c, px[ 3], shift_table[3][1], off_table[3][ 5]);
    II(c, d, a, b, px[10], shift_table[3][2], off_table[3][ 6]);
    II(b, c, d, a, px[ 1], shift_table[3][3], off_table[3][ 7]);
    II(a, b, c, d, px[ 8], shift_table[3][0], off_table[3][ 8]);
    II(d, a, b, c, px[15], shift_table[3][1], off_table[3][ 9]);
    II(c, d, a, b, px[ 6], shift_table[3][2], off_table[3][10]);
    II(b, c, d, a, px[13], shift_table[3][3], off_table[3][11]);
    II(a, b, c, d, px[ 4], shift_table[3][0], off_table[3][12]);
    II(d, a, b, c, px[11], shift_table[3][1], off_table[3][13]);
    II(c, d, a, b, px[ 2], shift_table[3][2], off_table[3][14]);
    II(b, c, d, a, px[ 9], shift_table[3][3], off_table[3][15]);

    p_state[0] += a;
    p_state[1] += b;
    p_state[2] += c;
    p_state[3] += d;
}
// MD5块更新操作
static void md5_update(md5_ctx* p_context, const unsigned char* pInput, unsigned int dwInputLen)
{
    unsigned int i, dwIndex, dwPartLen, dwBitsNum;
    // 计算 mod 64 的字节数
    dwIndex = (p_context->count[0] >> 3) & 0x3F;

    // 更新数据位数
    dwBitsNum = dwInputLen << 3;
    p_context->count[0] += dwBitsNum;

    if (p_context->count[0] < dwBitsNum)
        p_context->count[1]++;
    p_context->count[1] += dwInputLen >> 29;

    dwPartLen = 64 - dwIndex;
    if(dwInputLen >= dwPartLen)
    {
        memcpy( p_context->buffer+dwIndex, pInput, dwPartLen );
        md5_transform( p_context->status, (unsigned int*)p_context->buffer );

        for (i = dwPartLen; i + 63 < dwInputLen; i += 64)
            md5_transform( p_context->status, (unsigned int*)(pInput + i) );
        dwIndex = 0;
    }
    else
        i = 0;
    memcpy(p_context->buffer + dwIndex, pInput + i, dwInputLen - i);
}
// 处理最后的数据块
static void md5_final(md5_ctx* p_context)
{
    unsigned int dwIndex, dwPadLen;
    unsigned char pBits[8];
    memcpy(pBits, p_context->count, 8);

    // 计算 mod 64 的字节数
    dwIndex = (p_context->count[0] >> 3) & 0x3F;

    // 使长度满足K*64+56个字节
    dwPadLen = (dwIndex < 56) ? (56-dwIndex) : (120-dwIndex);
    md5_update(p_context, md_padding, dwPadLen);
    md5_update(p_context, pBits, 8);
}
////////////////////////////////////////////////////////////////////////////////
} //namespace basic end.
////////////////////////////////////////////////////////////////////////////////
inline int hash(const void *in, size_t bytes, void *out)
{
    unsigned int _bytes = (unsigned int)bytes;
    const unsigned char *_in = (const unsigned char*)in;
    unsigned char *_out = (unsigned char*)out;

    basic::md5_ctx struContext;
    if (_in == 0)
        _bytes = 0;

    // 进行MD5变换
    basic::md5_init(&struContext); // 初始化
    basic::md5_update(&struContext, _in, _bytes); // MD5数据块更新操作
    basic::md5_final(&struContext); // 获得最终结果

    // 获取哈希值
    if (_out != 0)
        memcpy(_out, struContext.status, 16);
    return 16;
}
inline int hmac_hash(const void *in, size_t bytes, const void *key, size_t key_size, void *out)
{
    unsigned int _bytes = (unsigned int)bytes;
    const unsigned char *_in = (const unsigned char*)in;

    unsigned int _key_size = (unsigned int)key_size;
    const unsigned char *_key = (const unsigned char*)key;
    unsigned char *_out = (unsigned char*)out;

    unsigned char hmac_key[64] = {0};
    unsigned char k_ipad[64];
    unsigned char k_opad[64];
    basic::md5_ctx struContext;

    if (_in == 0)
        _bytes = 0;

    if (_key == 0)
        _key_size = 0;

    // 保证密钥长度不超过64字节
    if (_key_size > 64)
        hash(_key, _key_size, hmac_key);
    else
        memcpy(hmac_key, _key, _key_size);

    for (unsigned int i = 0; i < 64; i++)
    {
        k_ipad[i] = hmac_key[i] ^ 0x36;
        k_opad[i] = hmac_key[i] ^ 0x5C;
    }
    // 内圈MD5运算
    basic::md5_init(&struContext);      // 初始化
    basic::md5_update(&struContext, k_ipad, 64);    // MD5数据块更新操作
    basic::md5_update(&struContext, _in, _bytes);   // MD5数据块更新操作
    basic::md5_final(&struContext); // 获得最终结果
    memcpy(hmac_key, struContext.status, 16);

    // 外圈MD5运算
    basic::md5_init(&struContext);      // 初始化
    basic::md5_update(&struContext, k_opad, 64);    // MD5数据块更新操作
    basic::md5_update(&struContext, hmac_key, 16);  // MD5数据块更新操作
    basic::md5_final(&struContext); // 获得最终结果

    // 获取哈希值
    if (_out != 0)
        memcpy(_out, struContext.status, 16);
    return 16;
}

////////////////////////////////////////////////////////////////////////////////
} //namespace md5 end.
////////////////////////////////////////////////////////////////////////////////

#endif //__INC_MD5_H_
