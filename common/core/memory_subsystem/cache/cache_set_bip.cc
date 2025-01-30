#include "cache_set_bip.h"
#include "log.h"
#include "stats.h"

// Implements LRU replacement, optionally augmented with Query-Based Selection [Jaleel et al., MICRO'10]

CacheSetBIP::CacheSetBIP(
      CacheBase::cache_t cache_type,
      UInt32 associativity, UInt32 blocksize, UInt32 inv_throttle, CacheSetInfoLRU* set_info, UInt8 num_attempts)
   : CacheSet(cache_type, associativity, blocksize)
   , m_num_attempts(num_attempts)
   , m_inv_throttle(inv_throttle)
   , m_set_info(set_info)
{
   m_lru_bits = new UInt8[m_associativity];
   for (UInt32 i = 0; i < m_associativity; i++)
      m_lru_bits[i] = i;
}

CacheSetBIP::~CacheSetBIP()
{
   delete [] m_lru_bits;
}

UInt32
CacheSetBIP::getReplacementIndex(CacheCntlr *cntlr)
{
   // First try to find an invalid block
   for (UInt32 i = 0; i < m_associativity; i++)
   {
      if (!m_cache_block_info_array[i]->isValid())
      {
         // Mark our newly-inserted line as most-recently used
         moveToMRU(i);
         return i;
      }
   }

   // Make m_num_attemps attempts at evicting the block at LRU position
   for(UInt8 attempt = 0; attempt < m_num_attempts; ++attempt)
   {
      UInt32 index = 0;
      UInt8 max_bits = 0;
      for (UInt32 i = 0; i < m_associativity; i++)
      {
         if (m_lru_bits[i] > max_bits && isValidReplacement(i))
         {
            index = i;
            max_bits = m_lru_bits[i];
         }
      }
      LOG_ASSERT_ERROR(index < m_associativity, "Error Finding LRU bits");

      bool qbs_reject = false;
      if (attempt < m_num_attempts - 1)
      {
         LOG_ASSERT_ERROR(cntlr != NULL, "CacheCntlr == NULL, QBS can only be used when cntlr is passed in");
         qbs_reject = cntlr->isInLowerLevelCache(m_cache_block_info_array[index]);
      }

      if (qbs_reject)
      {
         // Block is contained in lower-level cache, and we have more tries remaining.
         // Move this block to MRU and try again
         moveToMRU(index);
         cntlr->incrementQBSLookupCost();
         continue;
      }
      else
      {
         // Mark our newly-inserted line as most-recently used
         moveToMRU(index);
         m_set_info->incrementAttempt(attempt);
         return index;
      }
   }

   LOG_PRINT_ERROR("Should not reach here");
}

void
CacheSetBIP::updateReplacementIndex(UInt32 accessed_index)
{
   m_set_info->increment(m_lru_bits[accessed_index]);
   moveToMRU(accessed_index);
}

void
CacheSetBIP::moveToMRU(UInt32 accessed_index)
{
   for (UInt32 i = 0; i < m_associativity; i++)
   {
      if (m_lru_bits[i] < m_lru_bits[accessed_index])
         m_lru_bits[i] ++;
   }
   m_lru_bits[accessed_index] = 0;
}

void
CacheSetBIP::moveToLRU(UInt32 accessed_index)
{
   for (UInt32 i = 0; i < m_associativity; i++)
   {
      if (m_lru_bits[i] > m_lru_bits[accessed_index])
         m_lru_bits[i] --;
   }
   m_lru_bits[accessed_index] = m_associativity - 1;
}

void
CacheSetBIP::moveToBimodal(UInt32 accessed_index)
{
   static UInt32 insertions = 0;
   if (insertions % m_inv_throttle == 0++) {
      moveToMRU(accessed_index);
   } else {
      moveToLRU(accessed_index);
   }
}
