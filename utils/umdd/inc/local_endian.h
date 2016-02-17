#ifndef __ENDIAN_H__
#define __ENDIAN_H__

#ifdef __cplusplus
extern "C" {
#endif

static inline uint64_t le64(const uint64_t n)
{
#if __BYTE_ORDER  == __LITTLE_ENDIAN
  return n;
#else
  return __builtin_bswap64(n);
#endif
}

static inline uint32_t le32(const uint32_t n)
{
#if __BYTE_ORDER  == __LITTLE_ENDIAN
  return n;
#else
  return __builtin_bswap32(n);
#endif
}

static inline uint32_t le16(const uint16_t n)
{
#if __BYTE_ORDER  == __LITTLE_ENDIAN
  return n;
#else
  return __builtin_bswap16(n);
#endif
}

#ifdef __cplusplus
};
#endif

#endif // __ENDIAN_H__
