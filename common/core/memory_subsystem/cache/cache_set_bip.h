#ifndef CACHE_SET_BIP_H
#define CACHE_SET_BIP_H

#include "cache_set.h"
#include "cache_set_lru.h"


class CacheSetBIP : public CacheSet
{
   public:
      CacheSetBIP(CacheBase::cache_t cache_type,
            UInt32 associativity, UInt32 blocksize, CacheSetInfoLRU* set_info, UInt8 num_attempts);
      virtual ~CacheSetBIP();

      virtual UInt32 getReplacementIndex(CacheCntlr *cntlr);
      void updateReplacementIndex(UInt32 accessed_index);

   protected:
      const UInt8 m_num_attempts;
      UInt8* m_lru_bits;
      CacheSetInfoLRU* m_set_info;
      void moveToMRU(UInt32 accessed_index);
};

#endif /* CACHE_SET_BIP_H */
