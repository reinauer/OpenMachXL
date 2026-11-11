#include "optimizer.hpp"
#include "optimizer_bridge.h"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <set>

extern "C" void espresso_set_reduce_order(int);
extern "C" int espresso_get_reduce_order(void);

namespace machxl {
namespace {
std::mutex espresso_mutex;
std::string encode_cover_rows(const Sop& cover, const std::vector<std::string>& variables)
{
    if (cover.size() > 32767)
        throw LogicError("optimizer cover exceeds the signed 16-bit row limit");
    std::string rows;
    rows.reserve(cover.size() * variables.size());
    std::set<std::string> known(variables.begin(), variables.end());
    for (const auto& cube : cover) {
        for (const auto& literal : cube)
            if (!known.count(literal.first))
                throw LogicError("optimizer variable order omits " + literal.first);
        for (const auto& name : variables) {
            auto it = cube.find(name);
            rows += it == cube.end() ? '-' : it->second ? '1' : '0';
        }
    }
    return rows;
}

bool cube_contains(const Cube& broad, const Cube& narrow)
{
    for (const auto& literal : broad) {
        auto found = narrow.find(literal.first);
        if (found == narrow.end() || found->second != literal.second)
            return false;
    }
    return true;
}

class OrderedCoverExpander
{
   public:
    OrderedCoverExpander(const std::vector<std::string>& variables, SopLimits limits, bool reduce = true)
        : variables_(variables), limits_(limits), reduce_(reduce)
    {
    }

    Sop invert_cover(const Sop& cover) { return complement(cover); }
    Sop xor_covers(const Sop& left, const Sop& right)
    {
        auto not_left = complement(left), not_right = complement(right);
        return unite(intersection(not_left, right), intersection(not_right, left));
    }

    Sop expand(const ExprPtr& expression, std::size_t depth = 0)
    {
        if (!expression)
            throw LogicError("null expression");
        if (depth > 4096)
            throw LogicError("SOP nesting exceeds 4096 levels");
        consume_work_budget();
        if (expression->kind == ExprKind::Constant)
            return expression->value ? Sop{Cube{}} : Sop{};
        if (expression->kind == ExprKind::Identifier)
            return {Cube{{expression->name, true}}};
        auto left = expand(expression->left, depth + 1);
        if (expression->kind == ExprKind::Not)
            return complement(left);
        auto right = expand(expression->right, depth + 1);
        switch (expression->kind) {
        case ExprKind::And:
            return intersection(left, right);
        case ExprKind::Or:
            return unite(std::move(left), std::move(right));
        case ExprKind::Xor:
        case ExprKind::Xnor: {
            auto not_left  = complement(left);
            auto not_right = complement(right);
            if (expression->kind == ExprKind::Xor)
                return unite(intersection(not_left, right), intersection(not_right, left));
            return unite(intersection(left, right), intersection(not_left, not_right));
        }
        default:
            throw LogicError("invalid expression kind");
        }
    }

   private:
    const std::vector<std::string>& variables_;
    SopLimits limits_;
    std::size_t operations_ = 0;
    bool reduce_;

    void consume_work_budget(std::size_t amount = 1)
    {
        if (amount > limits_.max_products - operations_)
            throw LogicError("SOP work budget exceeded");
        operations_ += amount;
    }
    void check_cube_budget(std::size_t size)
    {
        if (size > limits_.max_cubes)
            throw LogicError("SOP cube budget exceeded");
    }
    Sop apply_cover_operation(const Sop& left, const Sop& right, int kind)
    {
        auto a = encode_cover_rows(left, variables_), b = encode_cover_rows(right, variables_);
        char* data        = nullptr;
        std::size_t count = 0;
        std::lock_guard<std::mutex> lock(espresso_mutex);
        int failed = machxl_espresso_cover_operation(a.data(), left.size(), b.data(), right.size(), variables_.size(),
                                                     kind, &data, &count);
        std::unique_ptr<char, decltype(&std::free)> result(data, std::free);
        if (failed)
            throw LogicError("Espresso cover operation failed");
        check_cube_budget(count);
        Sop cover;
        cover.reserve(count);
        // Preserve the reversed cover order used by compatible expression lowering.
        for (std::size_t i = count; i > 0; --i) {
            Cube cube;
            for (std::size_t v = 0; v < variables_.size(); ++v) {
                char c = data[(i - 1) * variables_.size() + v];
                if (c != '-')
                    cube.emplace(variables_[v], c == '1');
            }
            cover.push_back(std::move(cube));
        }
        return cover;
    }
    Sop complement(const Sop& cover)
    {
        if (cover.empty())
            return {Cube{}};
        if (cover.size() == 1 && cover.front().empty())
            return {};
        return apply_cover_operation(cover, {}, 0);
    }
    Sop intersection(const Sop& left, const Sop& right)
    {
        if (left.empty() || right.empty())
            return {};
        if (left.size() == 1 && left.front().empty())
            return right;
        if (right.size() == 1 && right.front().empty())
            return left;
        if (left.size() > limits_.max_products / right.size())
            throw LogicError("SOP work budget exceeded");
        consume_work_budget(left.size() * right.size());
        return apply_cover_operation(left, right, 1);
    }
    // Compare only across the two input lists. Mutations do not restart
    // earlier comparisons; equal terms retain the left cube.
    static void reduce_cube_pair(Cube& left, Cube& right, bool& erase_left, bool& erase_right)
    {
        std::string conflict;
        for (const auto& literal : left) {
            auto found = right.find(literal.first);
            if (found != right.end() && found->second != literal.second) {
                if (!conflict.empty())
                    return;
                conflict = literal.first;
            }
        }
        if (conflict.empty()) {
            if (cube_contains(left, right))
                erase_right = true;
            else if (cube_contains(right, left))
                erase_left = true;
            return;
        }
        auto l = left, r = right;
        l.erase(conflict);
        r.erase(conflict);
        if (cube_contains(r, l)) {
            left.erase(conflict);
            if (cube_contains(l, r))
                erase_right = true;
        } else if (cube_contains(l, r))
            right.erase(conflict);
    }
    Sop unite(Sop left, Sop right)
    {
        if (left.empty())
            return right;
        if (right.empty())
            return left;
        if (left.front().empty() || right.front().empty())
            return {Cube{}};
        for (auto a = left.begin(); reduce_ && a != left.end();) {
            bool erase_left = false;
            for (auto b = right.begin(); b != right.end();) {
                consume_work_budget();
                bool erase_right = false;
                reduce_cube_pair(*a, *b, erase_left, erase_right);
                if (erase_right)
                    b = right.erase(b);
                else
                    ++b;
            }
            if (erase_left)
                a = left.erase(a);
            else
                ++a;
        }
        check_cube_budget(left.size() + right.size());
        left.insert(left.end(), right.begin(), right.end());
        return left;
    }
};
void append_unique_variables(std::vector<std::string>& target, const std::vector<std::string>& source)
{
    for (const auto& name : source)
        if (std::find(target.begin(), target.end(), name) == target.end())
            target.push_back(name);
}
bool is_constant_cover(const Sop& cover)
{
    return cover.empty() || (cover.size() == 1 && cover.front().empty());
}
}  // namespace
Sop machmin_products(const ExprPtr& expression, const std::vector<std::string>& variables, SopLimits limits)
{
    if (variables.size() > 64)
        throw LogicError("optimizer equation exceeds the MACHXL 64-signal limit");
    if (std::set<std::string>(variables.begin(), variables.end()).size() != variables.size())
        throw LogicError("optimizer variable order contains duplicates");
    auto cover = OrderedCoverExpander(variables, limits).expand(expression);
    // Validate even identifier-only expressions that never call Espresso.
    (void)encode_cover_rows(cover, variables);
    return cover;
}
Sop machmin_unreduced_products(const ExprPtr& expression, SopLimits limits)
{
    // MINIMIZE_OFF bypasses cross-list OR reduction. Intersection and
    // complement still use the ordinary cover operations. Each structural
    // XOR leg collects variables in first-occurrence order before joining.
    auto variables = expression_variables(expression);
    if (variables.size() > 64)
        throw LogicError("optimizer equation exceeds the MACHXL 64-signal limit");
    return OrderedCoverExpander(variables, limits, false).expand(expression);
}
bool prune_optimizer_candidate(std::size_t candidate_terms, std::size_t minimum_terms)
{
    // Avoid overflow for callers outside the original 16-bit count range.
    if (minimum_terms > std::numeric_limits<std::size_t>::max() / 6)
        return false;
    return candidate_terms > 6 * minimum_terms;
}
std::size_t product_term_blocks(std::size_t terms, std::size_t block_size)
{
    if (!block_size)
        throw LogicError("optimizer product-term block size is zero");
    return terms / block_size + (terms % block_size != 0);
}
bool prefer_t_candidate(std::size_t d_terms, std::size_t t_terms, std::size_t block_size)
{
    return product_term_blocks(t_terms, block_size) < product_term_blocks(d_terms, block_size);
}
void check_optimizer_term_limit(std::size_t terms)
{
    if (terms > 960)
        throw LogicError("optimizer equation exceeds the MACHXL 960-product-term limit");
}
std::size_t PreparedXor::minimum_terms() const
{
    return split_possible ? std::min(plain.size(), left.size() + right.size()) : plain.size();
}
PreparedXor prepare_xor(const ExprPtr& expression, const ExprPtr& care, const ExprPtr& explicit_offset,
                        bool optimize_legs)
{
    if (!expression)
        throw LogicError("null expression");
    PreparedXor result;
    if (explicit_offset || (expression->kind != ExprKind::Xor && expression->kind != ExprKind::Xnor)) {
        result.variables = expression_variables(expression);
        append_unique_variables(result.variables, expression_variables(care));
        append_unique_variables(result.variables, expression_variables(explicit_offset));
        result.plain           = machmin_products(expression, result.variables);
        result.dontcare        = care ? machmin_products(make_not(care), result.variables) : Sop{};
        result.explicit_offset = bool(explicit_offset);
        if (explicit_offset)
            result.offset = machmin_products(explicit_offset, result.variables);
        return result;
    }
    result.inverted                    = expression->kind == ExprKind::Xnor;
    auto left_variables                = expression_variables(expression->left);
    auto right_variables               = expression_variables(expression->right);
    auto left                          = machmin_products(expression->left, left_variables);
    auto right                         = machmin_products(expression->right, right_variables);
    auto orient_single_product_xor_leg = [](Sop& l, Sop& r, auto& lv, auto& rv) {
        // Prefer a single-product or constant left XOR input.
        if ((l.size() > 1 && r.size() < 2) || is_constant_cover(r)) {
            std::swap(l, r);
            std::swap(lv, rv);
        }
    };
    orient_single_product_xor_leg(left, right, left_variables, right_variables);
    auto alternate_left           = OrderedCoverExpander(left_variables, {}).invert_cover(left);
    auto alternate_right          = OrderedCoverExpander(right_variables, {}).invert_cover(right);
    auto alternate_left_variables = left_variables, alternate_right_variables = right_variables;
    orient_single_product_xor_leg(alternate_left, alternate_right, alternate_left_variables, alternate_right_variables);
    // Compare raw product counts before any Espresso pass.
    if (optimize_legs && alternate_left.size() < 2 && (left.size() > 1 || alternate_right.size() < right.size())) {
        result.dual_complement = true;
        left                   = std::move(alternate_left);
        right                  = std::move(alternate_right);
        left_variables         = std::move(alternate_left_variables);
        right_variables        = std::move(alternate_right_variables);
    }
    // Preserve right-leg variable order, then append unseen left-leg variables.
    auto variables = right_variables;
    append_unique_variables(variables, left_variables);
    append_unique_variables(variables, expression_variables(care));
    if (variables.size() > 64)
        throw LogicError("optimizer equation exceeds the MACHXL 64-signal limit");
    auto plain            = OrderedCoverExpander(variables, {}).xor_covers(left, right);
    result.plain          = std::move(plain);
    result.left           = std::move(left);
    result.right          = std::move(right);
    result.variables      = std::move(variables);
    result.dontcare       = care ? machmin_products(make_not(care), result.variables) : Sop{};
    result.split_possible = result.left.size() < 2;
    return result;
}

OptimizedXor minimize_prepared_xor(const PreparedXor& prepared, std::size_t minimum_terms, OptimizerOptions options,
                                   OptimizerState* supplied_state, bool automatic_phase)
{
    OptimizerState independent_state;
    auto* state = supplied_state ? supplied_state : &independent_state;
    OptimizedXor result;
    // DOS compatibility clears XNOR phase when both legs are complemented.
    // Corrected mode retains the structural XNOR phase.
    result.inverted       = prepared.inverted && !(options.dos_compatibility && prepared.dual_complement);
    const auto& plain     = prepared.plain;
    const auto& left      = prepared.left;
    const auto& right     = prepared.right;
    const auto& variables = prepared.variables;
    const auto& dc        = prepared.dontcare;
    const bool use_plain  = !prune_optimizer_candidate(plain.size(), minimum_terms);
    const bool use_split =
        prepared.split_possible && !prune_optimizer_candidate(left.size() + right.size(), minimum_terms);
    result.available = use_plain || use_split;
    if (!result.available)
        return result;
    bool plain_inverted = false;
    bool right_inverted = false;
    if (use_plain) {
        if (is_constant_cover(plain))
            result.plain = plain;
        else {
            auto optimized = minimize_phase(
                plain, dc, prepared.explicit_offset && options.best_polarity ? &prepared.offset : nullptr, variables,
                automatic_phase, options, state);
            result.plain   = std::move(optimized.cover);
            plain_inverted = optimized.inverted;
        }
    }
    if (use_split) {
        result.left = left;
        // Minimize only the right XOR leg. A negative-phase winner changes
        // the common XOR inversion, preserving the single-product left leg.
        if (is_constant_cover(right))
            result.right = right;
        else {
            auto optimized = minimize_phase(right, dc, nullptr, variables, automatic_phase, options, state);
            result.right   = std::move(optimized.cover);
            right_inverted = optimized.inverted;
        }
        // Retain the unsplit candidate on equal product count.
        result.split = !use_plain || result.left.size() + result.right.size() < result.plain.size();
    }
    result.phase_inverted = result.split ? right_inverted : plain_inverted;
    result.inverted       = result.inverted != result.phase_inverted;
    return result;
}

OptimizedXor minimize_xor(const ExprPtr& expression, const ExprPtr& care, const ExprPtr& explicit_offset,
                          OptimizerOptions options, OptimizerState* state, bool automatic_phase)
{
    const auto prepared = prepare_xor(expression, care, explicit_offset);
    return minimize_prepared_xor(prepared, prepared.minimum_terms(), options, state, automatic_phase);
}
Sop minimize_cover(const Sop& onset, const Sop& dontcare, const std::vector<std::string>& variables,
                   OptimizerOptions options, OptimizerState* state)
{
    return minimize_phase(onset, dontcare, nullptr, variables, false, options, state).cover;
}
OptimizedPhase minimize_phase(const Sop& onset, const Sop& dontcare, const Sop* offset,
                              const std::vector<std::string>& variables, bool best_phase, OptimizerOptions options,
                              OptimizerState* state)
{
    if (variables.size() > 64)
        throw LogicError("optimizer equation exceeds the MACHXL 64-signal limit");
    if (std::set<std::string>(variables.begin(), variables.end()).size() != variables.size())
        throw LogicError("optimizer variable order contains duplicates");
    for (const auto& name : variables)
        if (name.empty())
            throw LogicError("optimizer variable name is empty");
    auto on  = encode_cover_rows(onset, variables);
    auto dc  = encode_cover_rows(dontcare, variables);
    auto off = offset ? encode_cover_rows(*offset, variables) : std::string{};
    // Bypass Espresso for constants: even an empty invocation changes
    // persistent reduction order and affects later equations.
    if (is_constant_cover(onset))
        return {onset, false};
    char* data        = nullptr;
    std::size_t count = 0;
    std::lock_guard<std::mutex> lock(espresso_mutex);
    if (state)
        espresso_set_reduce_order(state->reduce_order);
    int inverted = 0;
    int failed   = machxl_espresso_minimize_phase(
        on.data(), onset.size(), dc.data(), dontcare.size(), offset ? off.c_str() : nullptr,
        offset ? offset->size() : 0, variables.size(), best_phase,
        {options.fast, options.exact, options.skip_make_sparse, state ? false : options.reset_reduce_order}, &data,
        &count, &inverted);
    if (state)
        state->reduce_order = espresso_get_reduce_order() != 0;
    std::unique_ptr<char, decltype(&std::free)> result(data, std::free);
    if (failed)
        throw LogicError("Espresso result failed exact Boolean verification");
    check_optimizer_term_limit(count);
    Sop cover;
    cover.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        Cube cube;
        for (std::size_t v = 0; v < variables.size(); ++v) {
            char c = data[i * variables.size() + v];
            if (c != '-')
                cube.emplace(variables[v], c == '1');
        }
        cover.push_back(std::move(cube));
    }
    if (options.reverse_output)
        std::reverse(cover.begin(), cover.end());
    return {std::move(cover), inverted != 0};
}
}  // namespace machxl
