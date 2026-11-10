#ifndef MACHXL_OPTIMIZER_HPP
#define MACHXL_OPTIMIZER_HPP
#include "logic.hpp"

namespace machxl {
// Keep one state per compilation; the original reduce ordering alternates
// across equations. Explicit state prevents concurrent builds interfering.
struct OptimizerState
{
    bool reduce_order = true;
};

struct OptimizerOptions
{
    bool dos_compatibility = false;
    bool fast              = false;
    bool exact             = false;
    bool skip_make_sparse  = true;
    // Candidate generation/selection is performed by the frontend while
    // minimizing equations, using one shared OptimizerState.
    bool best_registers = false;
    bool best_jk_rs     = false;
    bool best_polarity  = false;
    // -1 preserves source phase; 0/1 force high/low after minimization.
    int forced_polarity = -1;
    // Original -CON (EQN_22V10=Y): Best polarity may change the external
    // signal phase. Without it, MACH3/4 use an internal XOR '%' instead.
    bool external_best_polarity = true;
    bool gate_split             = false;
    bool product_term_clocks    = false;
    unsigned gate_threshold = 20, gate_bucket = 20;
    unsigned gate_threshold_mach1 = 12, gate_bucket_mach1 = 12;
    unsigned gate_threshold_mach2 = 16, gate_bucket_mach2 = 16;
    // Reverse returned cubes to preserve compatible expression order.
    bool reverse_output     = true;
    bool reset_reduce_order = true;
};

// Prune raw candidates before minimization. Paired XOR candidates pass
// the sum of both halves as candidate_terms.
bool prune_optimizer_candidate(std::size_t candidate_terms, std::size_t minimum_terms);
// Compare occupied product-term blocks.
// MACH1/MACH2 use four terms per block; a tie retains the D candidate.
std::size_t product_term_blocks(std::size_t terms, std::size_t block_size);
bool prefer_t_candidate(std::size_t d_terms, std::size_t t_terms, std::size_t block_size = 4);
// Check every processed equation before selecting candidate winners,
// including equations with minimization disabled.
void check_optimizer_term_limit(std::size_t terms);

struct OptimizedPhase
{
    Sop cover;
    bool inverted = false;
};
struct OptimizedXor
{
    bool split = false;
    Sop plain;
    Sop left;
    Sop right;
    // Internal XOR/XNOR phase (the original PLA '%' marker).
    bool inverted = false;
    // Portion selected by minimization, excluding structural XNOR.
    // Best polarity may externalize this phase while retaining XNOR '%'.
    bool phase_inverted = false;
    bool available      = true;  // False when global D/T pruning removes both forms.
};
struct PreparedXor
{
    std::vector<std::string> variables;
    Sop plain, left, right, dontcare, offset;
    bool explicit_offset = false;
    bool split_possible  = false;
    bool dual_complement = false;
    bool inverted        = false;  // Structural XNOR, before selected phase.
    std::size_t minimum_terms() const;
};
// Candidate construction precedes all minimization. Best D/T supplies
// one minimum across both prepared records, preserving six-times pruning.
// Converted excitation is built after the source XOR phase pass: disable
// optimize_legs to retain its feedback leg instead of dual-complementing it.
PreparedXor prepare_xor(const ExprPtr& expression, const ExprPtr& care, const ExprPtr& explicit_offset = {},
                        bool optimize_legs = true);
OptimizedXor minimize_prepared_xor(const PreparedXor& prepared, std::size_t minimum_terms,
                                   OptimizerOptions options = {}, OptimizerState* state = nullptr,
                                   bool automatic_phase = true);
// MACH3/4 hardware-XOR candidate pipeline. The plain candidate and XORR
// consume the shared state in that order; XORL is never minimized.
OptimizedXor minimize_xor(const ExprPtr& expression, const ExprPtr& care, const ExprPtr& explicit_offset,
                          OptimizerOptions options = {}, OptimizerState* state = nullptr, bool automatic_phase = true);
// Compatible expression-to-cover operations: ordered Espresso
// intersections/complements and cross-list OR reduction.
// The unreduced form preserves cross-list duplicate/subsumed OR terms.
Sop machmin_products(const ExprPtr& expression, const std::vector<std::string>& variables, SopLimits limits = {});
Sop machmin_unreduced_products(const ExprPtr& expression, SopLimits limits = {});
// Keep an explicit ordered off-set from source lowering. Passing nullptr
// computes its complement. Best phase applies MACHMIN's cost gate and
// minimizes F, then R against minimized F, without resetting shared state.
OptimizedPhase minimize_phase(const Sop& onset, const Sop& dontcare, const Sop* offset,
                              const std::vector<std::string>& variables, bool best_phase, OptimizerOptions options = {},
                              OptimizerState* state = nullptr);

// Berkeley Espresso with the original IBMPC 16-bit packed-set layout.
// Variable order is semantic input: it affects tied heuristic choices.
// Register and polarity selection run in a separate pass.
Sop minimize_cover(const Sop& onset, const Sop& dontcare, const std::vector<std::string>& variables,
                   OptimizerOptions options = {}, OptimizerState* state = nullptr);
}  // namespace machxl
#endif
