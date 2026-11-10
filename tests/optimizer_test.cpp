#include "optimizer.hpp"
#include <stdexcept>
#include <iostream>
#include <map>
#include <string>
using namespace machxl;
static void require(bool condition) {
    if (!condition) throw std::runtime_error("optimizer test failed");
}
static bool covered(const Sop& cover, const std::map<std::string, bool>& values) {
    for (const auto& cube : cover) {
        bool match = true;
        for (const auto& lit : cube) match &= values.at(lit.first) == lit.second;
        if (match) return true;
    }
    return false;
}
int main() {
    auto chained_xor = parse_expression("A :+: B :+: C");
    require(chained_xor->left->kind == ExprKind::Identifier);
    require(chained_xor->right->kind == ExprKind::Xor);
    auto grouped_xor = parse_expression("(A :+: B) :+: C");
    require(grouped_xor->left->kind == ExprKind::Xor);
    require(grouped_xor->right->kind == ExprKind::Identifier);
    require(parse_expression("A+B+C")->right->kind == ExprKind::Or);
    require(parse_expression("A*B*C")->right->kind == ExprKind::And);
    // Keep the pre-minimizer XOR rows in the expected order.
    require(machmin_products(parse_expression("A :+: B"), {"A", "B"}) ==
        (Sop{Cube{{"A", false}, {"B", true}}, Cube{{"A", true}, {"B", false}}}));
    // REGCOST Q4 places the second operand's product rows in D,C,B,A order.
    Sop regcost_raw;
    for (const std::string name : {"D", "C", "B", "A"})
        regcost_raw.push_back(Cube{{"Q4", false}, {name, true}});
    regcost_raw.push_back(Cube{{"Q4", true}, {"A", false}, {"B", false},
                                {"C", false}, {"D", false}});
    require(machmin_products(parse_expression("Q4 :+: (A+B+C+D)"),
        {"Q4", "A", "B", "C", "D"}) == regcost_raw);
    require(machmin_products(parse_expression("A + /A*B"), {"A", "B"}) ==
        (Sop{Cube{{"A", true}}, Cube{{"B", true}}}));
    require(machmin_products(parse_expression("A*B + /A*B"), {"A", "B"}) ==
        (Sop{Cube{{"B", true}}}));
    require(machmin_unreduced_products(parse_expression("A+A")) ==
        (Sop{Cube{{"A", true}}, Cube{{"A", true}}}));
    require(machmin_unreduced_products(parse_expression("A+A*B")) ==
        (Sop{Cube{{"A", true}}, Cube{{"A", true}, {"B", true}}}));
    require(machmin_unreduced_products(parse_expression("(A+B)*(C+D)")) ==
        (Sop{Cube{{"B", true}, {"D", true}}, Cube{{"A", true}, {"D", true}},
             Cube{{"B", true}, {"C", true}}, Cube{{"A", true}, {"C", true}}}));
    auto xor_tie = minimize_xor(parse_expression("A :+: B"), {}, {});
    require(!xor_tie.split && !xor_tie.inverted && xor_tie.plain.size() == 2);
    auto xnor_tie = minimize_xor(parse_expression("A :*: B"), {}, {});
    require(!xnor_tie.split && xnor_tie.inverted && xnor_tie.plain.size() == 2);
    require(!xnor_tie.phase_inverted);
    for (bool initial_order : {false, true}) {
        OptimizerState state{initial_order};
        auto zero = minimize_phase({}, {}, nullptr, {}, true, {}, &state);
        require(zero.cover.empty() && !zero.inverted);
        require(state.reduce_order == initial_order);
        auto one = minimize_phase(Sop{Cube{}}, {}, nullptr, {"A"}, true, {}, &state);
        require(one.cover == Sop{Cube{}} && !one.inverted);
        require(state.reduce_order == initial_order);
        OptimizerState reference{initial_order};
        auto input = machmin_products(parse_expression("A :+: B"), {"A", "B"});
        require(minimize_cover(input, {}, {"A", "B"}, {}, &state) ==
                minimize_cover(input, {}, {"A", "B"}, {}, &reference));
        require(state.reduce_order == reference.reduce_order);
    }
    OptimizerOptions xor_best;
    xor_best.best_polarity = true;
    auto xnor_split = minimize_xor(parse_expression("A*B :*: C"), {}, {}, xor_best);
    require(xnor_split.split && xnor_split.inverted && !xnor_split.phase_inverted);
    auto xor_product = minimize_xor(parse_expression("A*B :+: C"), {}, {});
    require(xor_product.split && !xor_product.inverted);
    require(xor_product.left == (Sop{Cube{{"A", true}, {"B", true}}}));
    require(xor_product.right == (Sop{Cube{{"C", true}}}));
    auto xor_complement = minimize_xor(parse_expression("(A+B) :+: C"), {}, {});
    require(xor_complement.split && !xor_complement.inverted);
    require(xor_complement.left == (Sop{Cube{{"C", false}}}));
    require(xor_complement.right == (Sop{Cube{{"A", false}, {"B", false}}}));
    // Preparing both register forms must not consume minimizer state.
    // A globally pruned form must also leave the next equation's state alone.
    for (bool initial_order : {false, true}) {
        const auto expression = parse_expression("A*B :+: (C+D+E)");
        const auto prepared = prepare_xor(expression, {}, {});
        OptimizerState wrapped{initial_order}, staged{initial_order};
        const auto a = minimize_xor(expression, {}, {}, {}, &wrapped);
        const auto b = minimize_prepared_xor(prepared, prepared.minimum_terms(), {}, &staged);
        require(a.available && b.available && a.split == b.split);
        require(a.plain == b.plain && a.left == b.left && a.right == b.right);
        require(a.inverted == b.inverted && a.phase_inverted == b.phase_inverted);
        require(wrapped.reduce_order == staged.reduce_order);
        PreparedXor pruned;
        pruned.variables = {"A", "B", "C", "D", "E", "F", "G"};
        for (const auto& name : pruned.variables) pruned.plain.push_back(Cube{{name, true}});
        OptimizerState skipped{initial_order};
        require(!minimize_prepared_xor(pruned, 1, {}, &skipped).available);
        require(skipped.reduce_order == initial_order);
        pruned.plain.pop_back();
        require(minimize_prepared_xor(pruned, 1, {}, &skipped).available);
    }
    // An explicit source opposite cover disables hardware splitting, but
    // automatic internal phase derives the true complement unless Best is set.
    auto inconsistent = prepare_xor(parse_expression("A"), {}, parse_expression("A*B"));
    require(!inconsistent.split_possible);
    require(minimize_prepared_xor(inconsistent, inconsistent.minimum_terms()).plain ==
            (Sop{Cube{{"A", true}}}));
    auto xnor_dual = prepare_xor(parse_expression("(A+B) :*: C"), {});
    require(xnor_dual.dual_complement);
    auto corrected_xnor = minimize_prepared_xor(xnor_dual, xnor_dual.minimum_terms());
    OptimizerOptions original_xnor;
    original_xnor.dos_compatibility = true;
    auto historical_xnor = minimize_prepared_xor(xnor_dual, xnor_dual.minimum_terms(), original_xnor);
    require(corrected_xnor.split && historical_xnor.split);
    require(corrected_xnor.inverted && !historical_xnor.inverted);
    require(corrected_xnor.left == historical_xnor.left && corrected_xnor.right == historical_xnor.right);
    for (int bits = 0; bits < 8; ++bits) {
        std::map<std::string, bool> values{{"A", bool(bits&1)}, {"B", bool(bits&2)}, {"C", bool(bits&4)}};
        bool value = covered(corrected_xnor.left, values) != covered(corrected_xnor.right, values);
        require((value != corrected_xnor.inverted) == evaluate(parse_expression("(A+B) :*: C"), values));
    }
    auto xor_two_pairs = minimize_xor(parse_expression("(A :+: B) :+: (C :+: D)"), {}, {});
    require(!xor_two_pairs.split && xor_two_pairs.plain.size() == 8);
    auto xor_chain = minimize_xor(parse_expression("A :+: B :+: C"), {}, {});
    require(xor_chain.split && xor_chain.left == (Sop{Cube{{"A", true}}}));
    require(xor_chain.right == (Sop{Cube{{"B", false}, {"C", true}},
                                   Cube{{"B", true}, {"C", false}}}));
    // Original XORNEG: equal raw F/R counts do not preselect a phase.
    // XORR later chooses four negative terms and toggles the common '%'.
    const std::string xor_negative_right =
        "(B*/C*D*/E*/F+/C*F+/C*D*/E*F+C*E+/B*/C*E+B*/C*D*E*/F*/G)";
    OptimizerState xor_negative_state;
    for (const std::string left : {"A*H", "A"}) {
        auto expression = parse_expression("(" + left + ") :+: " + xor_negative_right);
        auto result = minimize_xor(expression, {}, {}, {}, &xor_negative_state);
        require(result.split && result.inverted && result.phase_inverted);
        require(result.left == expanded_products(parse_expression(left)));
        require(result.right == (Sop{
            Cube{{"C", true}, {"E", false}},
            Cube{{"B", true}, {"C", false}, {"D", false}, {"F", false}},
            Cube{{"B", false}, {"E", false}, {"F", false}},
            Cube{{"B", true}, {"C", false}, {"E", true}, {"F", false}, {"G", true}}}));
        for (unsigned bits = 0; bits < 256; ++bits) {
            std::map<std::string, bool> values;
            for (unsigned i = 0; i < 8; ++i)
                values[std::string(1, char('A' + i))] = (bits >> i) & 1;
            require(evaluate(expression, values) ==
                    ((covered(result.left, values) != covered(result.right, values)) != result.inverted));
        }
    }
    require(product_term_blocks(0, 4) == 0);
    require(product_term_blocks(1, 4) == 1);
    require(product_term_blocks(4, 4) == 1);
    require(product_term_blocks(5, 4) == 2);
    require(product_term_blocks(8, 4) == 2);
    require(product_term_blocks(9, 4) == 3);
    require(!prefer_t_candidate(4, 1));
    require(prefer_t_candidate(5, 4));
    require(!prefer_t_candidate(4, 5));
    require(!prefer_t_candidate(8, 5));
    require(prefer_t_candidate(3, 2, 1));
    require(!prefer_t_candidate(3, 3, 1));
    require(!prune_optimizer_candidate(6, 1));
    require(prune_optimizer_candidate(7, 1));
    require(!prune_optimizer_candidate(12, 2));
    require(prune_optimizer_candidate(13, 2));
    require(!prune_optimizer_candidate(0, 0));
    require(prune_optimizer_candidate(1, 0));
    bool invalid_block = false;
    try { product_term_blocks(1, 0); } catch (const LogicError&) { invalid_block = true; }
    require(invalid_block);
    check_optimizer_term_limit(960);
    bool too_many_terms = false;
    try { check_optimizer_term_limit(961); } catch (const LogicError&) { too_many_terms = true; }
    require(too_many_terms);
    Sop phase_on = expanded_products(parse_expression("A + B"));
    Sop phase_off = expanded_products(parse_expression("/A * /B"));
    OptimizerState combined_state, separate_state;
    auto phase = minimize_phase(phase_on, {}, &phase_off, {"A", "B"},
                                true, {}, &combined_state);
    require(phase.inverted && phase.cover == phase_off);
    auto pos = minimize_phase(phase_on, {}, &phase_off, {"A", "B"},
                              false, {}, &separate_state);
    auto neg = minimize_phase(phase_off, {}, &pos.cover, {"A", "B"},
                              false, {}, &separate_state);
    require(phase.cover == neg.cover && combined_state.reduce_order == separate_state.reduce_order);
    auto parity_on = expanded_products(parse_expression("A*/B + /A*B"));
    auto parity_off = expanded_products(parse_expression("A*B + /A*/B"));
    require(!minimize_phase(parity_on, {}, &parity_off, {"A", "B"}, true).inverted);
    Sop empty;
    auto tautology = minimize_phase(expanded_products(parse_expression("A + /A")),
                                    {}, &empty, {"A"}, true);
    require(!tautology.inverted && tautology.cover == Sop{Cube{}});
    // The original raw-size gate declines an off-set at eight times F.
    Sop expensive_off(16, phase_off.front());
    require(!minimize_phase(phase_on, {}, &expensive_off, {"A", "B"}, true).inverted);
    require(minimize_cover({}, {}, {}) == Sop{});
    require(minimize_cover({Cube{}}, {}, {}) == Sop{Cube{}});
    auto expression = parse_expression("A*/B + /A*B + C");
    auto raw = expanded_products(expression);
    auto result = minimize_cover(raw, {}, {"A", "B", "C"});
    // Preserve the ordered COUNTER.TC[0] product rows.
    Sop expected = {Cube{{"C", true}}, Cube{{"A", false}, {"B", true}},
                    Cube{{"A", true}, {"B", false}}};
    require(result == expected);
    for (int pattern = 0; pattern < 8; ++pattern) {
        std::map<std::string, bool> values{{"A", (pattern & 1) != 0},
            {"B", (pattern & 2) != 0}, {"C", (pattern & 4) != 0}};
        require(covered(result, values) == evaluate(expression, values));
    }
    // DC must remove A, although the onset initially contains one cube.
    require((minimize_cover({Cube{{"A", true}, {"B", true}}},
        {Cube{{"A", false}, {"B", true}}}, {"A", "B"}) == Sop{Cube{{"B", true}}}));
    // Cross 16-bit word boundaries and the maximum original equation width.
    std::vector<std::string> names;
    for (int i = 0; i < 64; ++i) names.push_back("S" + std::to_string(i));
    Sop boundary = {Cube{{names[7], true}, {names[8], true}, {names[63], false}},
                    Cube{{names[7], false}, {names[8], true}, {names[63], false}}};
    Sop boundary_expected = {Cube{{names[8], true}, {names[63], false}}};
    require(minimize_cover(boundary, {}, names) == boundary_expected);
    OptimizerOptions fast; fast.fast = true;
    OptimizerOptions exact; exact.exact = true;
    require(minimize_cover(boundary, {}, names, fast) == boundary_expected);
    require(minimize_cover(boundary, {}, names, exact) == boundary_expected);
    // Espresso 2.5.1 dropped the cost updates that keep its improvement loops
    // running. Its stock driver takes 13 products / 52 literals here; the
    // corrected driver takes 12 / 48. Guard quality without fixing cube order.
    // See research/optimizer/upstream-2.5.1-audit for the controlled comparison.
    std::vector<std::string> quality_names = {"A", "B", "C", "D", "E", "F"};
    Sop quality_input;
    for (unsigned bits : {0u, 5u, 7u, 8u, 9u, 11u, 12u, 13u, 15u, 17u,
            18u, 20u, 21u, 22u, 23u, 24u, 25u, 26u, 28u, 32u, 33u, 34u,
            40u, 41u, 42u, 43u, 44u, 47u, 48u, 50u, 52u, 53u, 56u, 57u,
            60u, 61u}) {
        Cube term;
        for (unsigned v = 0; v < 6; ++v)
            term[quality_names[v]] = ((bits >> (5 - v)) & 1u) != 0;
        quality_input.push_back(std::move(term));
    }
    for (bool compatibility : {false, true}) {
        OptimizerOptions quality_options;
        quality_options.dos_compatibility = compatibility;
        auto output = minimize_cover(quality_input, {}, quality_names, quality_options);
        std::size_t literals = 0;
        for (const auto& term : output) literals += term.size();
        require(output.size() < 12 || (output.size() == 12 && literals <= 48));
        for (unsigned bits = 0; bits < 64; ++bits) {
            std::map<std::string, bool> values;
            for (unsigned v = 0; v < 6; ++v)
                values[quality_names[v]] = ((bits >> (5 - v)) & 1u) != 0;
            require(covered(output, values) == covered(quality_input, values));
        }
    }
    // Odd-parity minterms cannot combine without covering an even-parity
    // off-set point. Exercise the limit after a real optimization pass.
    std::vector<std::string> eleven;
    for (int i = 0; i < 11; ++i) eleven.push_back("P" + std::to_string(i));
    Sop parity_limit;
    for (unsigned bits = 0; bits < 2048 && parity_limit.size() < 961; ++bits) {
        unsigned population = 0;
        for (unsigned value = bits; value; value >>= 1) population += value & 1;
        if (!(population & 1)) continue;
        Cube term;
        for (int i = 0; i < 11; ++i) term[eleven[i]] = ((bits >> i) & 1) != 0;
        parity_limit.push_back(std::move(term));
    }
    too_many_terms = false;
    try { minimize_cover(parity_limit, {}, eleven, fast); }
    catch (const LogicError& error) {
        too_many_terms = std::string(error.what()).find("960-product-term") != std::string::npos;
    }
    require(too_many_terms);
    parity_limit.pop_back();
    require(minimize_cover(parity_limit, {}, eleven, fast).size() == 960);
    // Deterministic generated covers check heuristic choices against the full
    // truth table, including cases requiring multiple expansion/reduce passes.
    unsigned seed = 0x12345678;
    std::vector<std::string> six = {"a", "b", "c", "d", "e", "f"};
    for (int trial = 0; trial < 64; ++trial) {
        Sop input;
        for (int term = 0; term < 9; ++term) {
            Cube cube;
            for (const auto& name : six) {
                seed = seed * 1664525u + 1013904223u;
                unsigned literal = (seed >> 16) % 3;
                if (literal < 2) cube[name] = literal != 0;
            }
            input.push_back(cube);
        }
        auto output = minimize_cover(input, {}, six);
        ExprPtr generated = make_constant(false);
        for (const auto& cube : input) {
            ExprPtr term = make_constant(true);
            for (const auto& literal : cube) {
                auto symbol = make_identifier(literal.first);
                term = make_binary(ExprKind::And, term,
                    literal.second ? symbol : make_not(symbol));
            }
            generated = make_binary(ExprKind::Or, generated, term);
        }
        auto lowered = machmin_products(generated, six);
        auto lowered_not = machmin_products(make_not(generated), six);
        for (int bits = 0; bits < 64; ++bits) {
            std::map<std::string, bool> values;
            for (int v = 0; v < 6; ++v) values[six[v]] = ((bits >> v) & 1) != 0;
            require(covered(input, values) == covered(output, values));
            require(covered(input, values) == covered(lowered, values));
            require(covered(input, values) != covered(lowered_not, values));
        }
    }
    // Every call rebuilds the global workspace and resets heuristic toggles.
    for (int i = 0; i < 20; ++i) require(minimize_cover(raw, {}, {"A", "B", "C"}) == expected);
    OptimizerState first, second;
    for (int i = 0; i < 10; ++i) {
        auto a = minimize_cover(raw, {}, {"A", "B", "C"}, {}, &first);
        auto b = minimize_cover(raw, {}, {"A", "B", "C"}, {}, &second);
        require(a == b && first.reduce_order == second.reduce_order);
    }
    bool rejected = false;
    try { minimize_cover(raw, {}, {"A", "B"}); } catch (const LogicError&) { rejected = true; }
    require(rejected);
    require(expanded_products(parse_expression("A + A*B")).size() == 2);
    std::cout << "optimizer: truth, don't-care, order, 16-bit boundaries, repeatability passed\n";
}
