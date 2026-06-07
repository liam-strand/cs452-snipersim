/*
 * This file is covered under the Interval Academic License, see LICENCE.academic
 */

#include "interval_contention_nehalem.h"
#include "core.h"
#include "stats.h"
#include "simulator.h"
#include "config.hpp"

IntervalContentionNehalem::IntervalContentionNehalem(const Core *core, const CoreModel *core_model)
   : m_core_model(core_model)
   , m_count_int(0)
   , m_count_fp(0)
   , m_cpContrInt(0)
   , m_cpContrFp(0)
{
   for(unsigned int i = 0; i < DynamicMicroOpNehalem::UOP_PORT_SIZE; ++i)
   {
      m_cpContrByPort[i] = 0;
      String name = String("cpContr_") + DynamicMicroOpNehalem::PortTypeString((DynamicMicroOpNehalem::uop_port_t)i);
      registerStatsMetric("interval_timer", core->getId(), name, &(m_cpContrByPort[i]));
   }

   // Pooled functional-unit capacities (uops issued per cycle per class)
   m_num_int_alu = Sim()->getCfg()->getIntArray("perf_model/core/interval_timer/num_int_alu", core->getId());
   m_num_fp_fu = Sim()->getCfg()->getIntArray("perf_model/core/interval_timer/num_fp_fu", core->getId());
   LOG_ASSERT_ERROR(m_num_int_alu > 0, "num_int_alu must be > 0");
   LOG_ASSERT_ERROR(m_num_fp_fu > 0, "num_fp_fu must be > 0");
   registerStatsMetric("interval_timer", core->getId(), "cpContr_int_alu", &m_cpContrInt);
   registerStatsMetric("interval_timer", core->getId(), "cpContr_fp_fu", &m_cpContrFp);
}

// Classify a uop into the pooled-FU classes: generic int → int ALU pool,
// FP add/sub/mul/div → FP FU pool.  Loads, stores and branches are still
// governed by their issue ports (PORT2 / PORT34 / PORT5).
static bool isIntAluUop(const DynamicMicroOp *uop)
{
   return uop->getMicroOp()->getSubtype() == MicroOp::UOP_SUBTYPE_GENERIC;
}

static bool isFpUop(const DynamicMicroOp *uop)
{
   MicroOp::uop_subtype_t subtype = uop->getMicroOp()->getSubtype();
   return subtype == MicroOp::UOP_SUBTYPE_FP_ADDSUB || subtype == MicroOp::UOP_SUBTYPE_FP_MULDIV;
}

void IntervalContentionNehalem::clearFunctionalUnitStats()
{
   for(unsigned int i = 0; i < (unsigned int)DynamicMicroOpNehalem::UOP_PORT_SIZE; ++i)
   {
      m_count_byport[i] = 0;
   }
   m_count_int = 0;
   m_count_fp = 0;
}

void IntervalContentionNehalem::addFunctionalUnitStats(const DynamicMicroOp *uop)
{
   m_count_byport[uop->getCoreSpecificInfo<DynamicMicroOpNehalem>()->getPort()]++;
   if (isIntAluUop(uop))
      m_count_int++;
   else if (isFpUop(uop))
      m_count_fp++;
}

void IntervalContentionNehalem::removeFunctionalUnitStats(const DynamicMicroOp *uop)
{
   m_count_byport[uop->getCoreSpecificInfo<DynamicMicroOpNehalem>()->getPort()]--;
   if (isIntAluUop(uop))
      m_count_int--;
   else if (isFpUop(uop))
      m_count_fp--;
}

uint64_t IntervalContentionNehalem::getEffectiveCriticalPathLength(uint64_t critical_path_length, bool update_reason)
{
   // Contention-reason bookkeeping: a port, or one of the pooled FU classes
   enum { REASON_NONE, REASON_PORT, REASON_INT_ALU, REASON_FP_FU } reason_class = REASON_NONE;
   DynamicMicroOpNehalem::uop_port_t reason = DynamicMicroOpNehalem::UOP_PORT_SIZE;
   uint64_t effective_cp_length = critical_path_length;

   // For the standard ports, check if we have exceeded our execution limit.
   // PORT0 and PORT1 uops (FP and int multiplies) are governed by the pooled
   // functional-unit checks below, as are the shared ports.
   for (unsigned int i = 0 ; i < DynamicMicroOpNehalem::UOP_PORT_SIZE ; i++)
   {
      // Skip shared ports and the pooled execution ports
      if (i != DynamicMicroOpNehalem::UOP_PORT015
         && i != DynamicMicroOpNehalem::UOP_PORT05
         && i != DynamicMicroOpNehalem::UOP_PORT0
         && i != DynamicMicroOpNehalem::UOP_PORT1
         && effective_cp_length < m_count_byport[i]
      )
      {
         effective_cp_length = m_count_byport[i];
         reason_class = REASON_PORT;
         reason = (DynamicMicroOpNehalem::uop_port_t)i;
      }
   }
   // Pooled functional units: m_num_int_alu generic-int uops and
   // m_num_fp_fu FP uops can issue per cycle (rounded up to the next cycle)
   if (m_count_int > (m_num_int_alu * effective_cp_length))
   {
      effective_cp_length = (m_count_int + m_num_int_alu - 1) / m_num_int_alu;
      reason_class = REASON_INT_ALU;
   }
   if (m_count_fp > (m_num_fp_fu * effective_cp_length))
   {
      effective_cp_length = (m_count_fp + m_num_fp_fu - 1) / m_num_fp_fu;
      reason_class = REASON_FP_FU;
   }

   if (update_reason && effective_cp_length > critical_path_length)
   {
      LOG_ASSERT_ERROR(reason_class != REASON_NONE, "There should be a reason for the cp extention, but there isn't");
      uint64_t contribution = 1000000 * (effective_cp_length - critical_path_length) / effective_cp_length;
      switch(reason_class)
      {
         case REASON_PORT:
            m_cpContrByPort[reason] += contribution;
            break;
         case REASON_INT_ALU:
            m_cpContrInt += contribution;
            break;
         case REASON_FP_FU:
            m_cpContrFp += contribution;
            break;
         default:
            break;
      }
   }

   return effective_cp_length;
}
