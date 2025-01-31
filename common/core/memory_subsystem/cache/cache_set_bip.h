#ifndef CACHE_SET_BIP_H
#define CACHE_SET_BIP_H

#include "cache_set.h"
#include "cache_set_lru.h"
#include <random>

class CacheSetBIP : public CacheSet
{
   public:
      CacheSetBIP(CacheBase::cache_t cache_type,
            UInt32 associativity, UInt32 blocksize, UInt32 inv_throttle, 
            CacheSetInfoLRU* set_info, UInt8 num_attempts);
      virtual ~CacheSetBIP();

      virtual UInt32 getReplacementIndex(CacheCntlr *cntlr);
      void updateReplacementIndex(UInt32 accessed_index);

   protected:
      const UInt8 m_num_attempts;
      UInt8* m_lru_bits;
      CacheSetInfoLRU* m_set_info;
      UInt32 m_inv_throttle;
      std::random_device m_rd;
      std::mt19937 m_gen;
      std::uniform_int_distribution<UInt32> m_distribution;
      void moveToMRU(UInt32 accessed_index);
      void moveToLRU(UInt32 accessed_index);
      void moveToBimodal(UInt32 accessed_index);
};

#endif /* CACHE_SET_BIP_H */
