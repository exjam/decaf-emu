#pragma optimize("", off)
#include <disassemble.h>
#include <libgfd/gfd.h>
#include <libgpu/latte/latte_instructions.h>
#include <gsl.h>
#include <SpvBuilder.h>
#include <stack>

using namespace latte;

namespace latte
{

inline bool
isTranscendentalOnly(SQ_ALU_FLAGS flags)
{
   if (flags & SQ_ALU_FLAG_VECTOR) {
      return false;
   }

   if (flags & SQ_ALU_FLAG_TRANSCENDENTAL) {
      return true;
   }

   return false;
}

inline bool
isVectorOnly(SQ_ALU_FLAGS flags)
{
   if (flags & SQ_ALU_FLAG_TRANSCENDENTAL) {
      return false;
   }

   if (flags & SQ_ALU_FLAG_VECTOR) {
      return true;
   }

   return false;
}

struct AluGroup
{
   AluGroup(const latte::AluInst *group)
   {
      auto instructionCount = 0u;
      auto literalCount = 0u;

      for (instructionCount = 1u; instructionCount <= 5u; ++instructionCount) {
         auto &inst = group[instructionCount - 1];
         auto srcCount = 0u;

         if (inst.word1.ENCODING() == SQ_ALU_ENCODING::OP2) {
            srcCount = getInstructionNumSrcs(inst.op2.ALU_INST());
         } else {
            srcCount = getInstructionNumSrcs(inst.op3.ALU_INST());
         }

         if (srcCount > 0 && inst.word0.SRC0_SEL() == SQ_ALU_SRC::LITERAL) {
            literalCount = std::max<unsigned>(literalCount, 1u + inst.word0.SRC0_CHAN());
         }

         if (srcCount > 1 && inst.word0.SRC1_SEL() == SQ_ALU_SRC::LITERAL) {
            literalCount = std::max<unsigned>(literalCount, 1u + inst.word0.SRC1_CHAN());
         }

         if (srcCount > 2 && inst.op3.SRC2_SEL() == SQ_ALU_SRC::LITERAL) {
            literalCount = std::max<unsigned>(literalCount, 1u + inst.op3.SRC2_CHAN());
         }

         if (inst.word0.LAST()) {
            break;
         }
      }

      instructions = gsl::make_span(group, instructionCount);
      literals = gsl::make_span(reinterpret_cast<const uint32_t *>(group + instructionCount), literalCount);
   }

   size_t getNextSlot(size_t slot)
   {
      slot += instructions.size();
      slot += (literals.size() + 1) / 2;
      return slot;
   }

   gsl::span<const latte::AluInst> instructions;
   gsl::span<const uint32_t> literals;
};

struct AluGroupUnits
{
   SQ_CHAN addInstructionUnit(const latte::AluInst &inst)
   {
      SQ_ALU_FLAGS flags;
      SQ_CHAN unit = inst.word1.DST_CHAN();

      if (inst.word1.ENCODING() == SQ_ALU_ENCODING::OP2) {
         flags = getInstructionFlags(inst.op2.ALU_INST());
      } else {
         flags = getInstructionFlags(inst.op3.ALU_INST());
      }

      if (isTranscendentalOnly(flags)) {
         unit = SQ_CHAN::T;
      } else if (isVectorOnly(flags)) {
         unit = unit;
      } else if (units[unit]) {
         unit = SQ_CHAN::T;
      }

      decaf_assert(!units[unit], fmt::format("Clause instruction unit collision for unit {}", unit));
      units[unit] = true;
      return unit;
   }

   bool units[5] = { false, false, false, false, false };
};

} // namespace latte


struct TranslateState
{
   int cfPC;
   std::stack<spv::Id> cfStack;
   std::vector<spv::Block *> blocks;
   const std::vector<uint8_t> *binary;

   spv::Id boolType;
   spv::Id floatType;
   spv::Id predicate;

   auto push()
   {
      cfStack.push(predicate);
   }

   auto pop_n(int n)
   {
      auto value = cfStack.top();

      for (auto i = 0; i < n; ++i) {
         value = cfStack.top();
         cfStack.pop();
      }

      predicate = value;
      return predicate;
   }
};



static void
translateNormal(spv::Builder &b, TranslateState &s, ControlFlowInst cf)
{
   auto result = true;
   auto id = cf.word1.CF_INST();
   auto name = getInstructionName(id);

   switch (id) {
   case SQ_CF_INST::SQ_CF_INST_POP_PUSH:
      s.pop_n(cf.word1.POP_COUNT());
      // fallthrough:
   case SQ_CF_INST::SQ_CF_INST_PUSH:
      s.push();
      b.createConditionalBranch(s.predicate,
                                s.blocks[s.cfPC + 1],
                                s.blocks[cf.word0.ADDR()]);
      break;
   case SQ_CF_INST::SQ_CF_INST_POP_PUSH_ELSE:
      s.pop_n(cf.word1.POP_COUNT());
      // fallthrough:
   case SQ_CF_INST::SQ_CF_INST_PUSH_ELSE:
      s.push();
      s.predicate = b.createUnaryOp(spv::Op::OpLogicalNot,
                                    s.boolType,
                                    s.predicate);
      b.createConditionalBranch(s.predicate,
                                s.blocks[s.cfPC + 1],
                                s.blocks[cf.word0.ADDR()]);
      break;
   case SQ_CF_INST::SQ_CF_INST_POP:
      s.pop_n(cf.word1.POP_COUNT());
      b.createConditionalBranch(s.predicate,
                                s.blocks[s.cfPC + 1],
                                s.blocks[cf.word0.ADDR()]);
      break;
   case SQ_CF_INST::SQ_CF_INST_POP_JUMP:
      s.pop_n(cf.word1.POP_COUNT());
      // fallthrough:
   case SQ_CF_INST::SQ_CF_INST_JUMP:
      b.createConditionalBranch(s.predicate,
                                s.blocks[s.cfPC + 1],
                                s.blocks[cf.word0.ADDR()]);
      break;
   case SQ_CF_INST::SQ_CF_INST_ELSE:
      s.predicate = b.createUnaryOp(spv::Op::OpLogicalNot,
                                    s.boolType,
                                    s.predicate);
      b.createConditionalBranch(s.predicate,
                                s.blocks[s.cfPC + 1],
                                s.blocks[cf.word0.ADDR()]);
      break;
   case SQ_CF_INST::SQ_CF_INST_LOOP_START:
   case SQ_CF_INST::SQ_CF_INST_LOOP_END:
   case SQ_CF_INST::SQ_CF_INST_LOOP_START_DX10:
   case SQ_CF_INST::SQ_CF_INST_LOOP_START_NO_AL:
   case SQ_CF_INST::SQ_CF_INST_LOOP_CONTINUE:
   case SQ_CF_INST::SQ_CF_INST_LOOP_BREAK:
      break;
   }
}

static void
translateExport(spv::Builder &b, TranslateState &state, ControlFlowInst cf)
{
   auto id = cf.exp.word1.CF_INST();
   auto name = getInstructionName(id);
}

static void
translateControlFlowALU(spv::Builder &b, TranslateState &s, ControlFlowInst cf)
{
   uint32_t lastGroup = -1;
   auto id = cf.alu.word1.CF_INST();
   auto name = getInstructionName(id);
   auto addr = cf.alu.word0.ADDR();
   auto count = cf.alu.word1.COUNT() + 1;
   auto clause = reinterpret_cast<const AluInst *>(s.binary->data() + 8 * addr);
   auto hasPushedBeforePredSet = false;

   for (size_t slot = 0u; slot < count; ) {
      auto units = AluGroupUnits {};
      auto group = AluGroup { clause + slot };
      auto didReduction = false;
      auto updatePreviousVector = false;
      auto updatePreviousScalar = false;

      for (auto j = 0u; j < group.instructions.size(); ++j) {
         auto &inst = group.instructions[j];
         auto unit = units.addInstructionUnit(inst);


         if (inst.word1.ENCODING() == SQ_ALU_ENCODING::OP2) {
            if (inst.op2.ALU_INST() == SQ_OP2_INST::SQ_OP2_INST_PRED_SETE) {
               if (id == SQ_CF_ALU_INST::SQ_CF_INST_ALU_PUSH_BEFORE && !hasPushedBeforePredSet) {
                  s.push();
                  hasPushedBeforePredSet = true;
               }

               s.predicate = b.createBinOp(spv::Op::OpFOrdEqual, s.floatType,
                                           b.makeFloatConstant(0.0f),
                                           b.makeFloatConstant(1.0f));
            }
         } else {

         }
      }

      slot = group.getNextSlot(slot);
   }

   if (id == SQ_CF_ALU_INST::SQ_CF_INST_ALU_POP_AFTER) {
      s.pop_n(1);
   } else if (id == SQ_CF_ALU_INST::SQ_CF_INST_ALU_POP2_AFTER) {
      s.pop_n(2);
   } else if (id == SQ_CF_ALU_INST::SQ_CF_INST_ALU_ELSE_AFTER) {
      s.predicate = b.createUnaryOp(spv::Op::OpLogicalNot, s.boolType, s.predicate);
   }
}

int main(int argc, char **argv)
{
   gfd::GFDFile file;
   auto path = argv[1];

   try {
      if (!gfd::readFile(file, path)) {
         return -1;
      }
   } catch (gfd::GFDReadException ex) {
      return -1;
   }

   auto &gshShader = file.pixelShaders.front();
   const auto &binary = gshShader.data;

   auto b = spv::Builder { 0x10000, 0xFFFFFFFF, nullptr };
   b.setSource(spv::SourceLanguage::SourceLanguageUnknown, 0);
   b.setMemoryModel(spv::AddressingModel::AddressingModelLogical, spv::MemoryModel::MemoryModelVulkanKHR);
   b.addCapability(spv::Capability::CapabilityShader);

   auto state = TranslateState { };
   state.predicate = b.makeBoolConstant(true);
   state.binary = &binary;
   state.boolType = b.makeBoolType();
   state.floatType = b.makeFloatType(32);

   b.makeEntryPoint("main");
   auto prevBlock = b.getBuildPoint();

   for (auto i = 0; i < binary.size(); i += sizeof(ControlFlowInst)) {
      auto cf = *reinterpret_cast<const ControlFlowInst *>(binary.data() + i);
      auto block = &b.makeNewBlock();
      block->addPredecessor(prevBlock);
      state.blocks.push_back(block);

      if (cf.word1.CF_INST_TYPE() == SQ_CF_INST_TYPE_NORMAL
          || cf.word1.CF_INST_TYPE() == SQ_CF_INST_TYPE_EXPORT) {
         if (cf.word1.END_OF_PROGRAM()) {
            break;
         }
      }
   }

   state.cfPC = 0;

   for (auto i = 0; i < binary.size(); i += sizeof(ControlFlowInst)) {
      auto cf = *reinterpret_cast<const ControlFlowInst *>(binary.data() + i);
      auto id = cf.word1.CF_INST();

      b.setBuildPoint(state.blocks[state.cfPC]);


      switch (cf.word1.CF_INST_TYPE()) {
      case SQ_CF_INST_TYPE_NORMAL:
         translateNormal(b, state, cf);
         break;
      case SQ_CF_INST_TYPE_EXPORT:
         translateExport(b, state, cf);
         break;
      case SQ_CF_INST_TYPE_ALU:
      case SQ_CF_INST_TYPE_ALU_EXTENDED:
         translateControlFlowALU(b, state, cf);
         break;
      default:
         throw "cunt";
      }

      if (cf.word1.CF_INST_TYPE() == SQ_CF_INST_TYPE_NORMAL
          || cf.word1.CF_INST_TYPE() == SQ_CF_INST_TYPE_EXPORT) {
         if (cf.word1.END_OF_PROGRAM()) {
            break;
         }
      }

      state.cfPC++;
   }

   std::vector<unsigned int> outputBinary;
   b.dump(outputBinary);
   spv::Disassemble(std::cout, outputBinary);
   return 0;
}
