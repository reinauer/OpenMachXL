#include "logic.hpp"

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using namespace machxl;

namespace {

void check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

template<class F> void rejects(F operation, const std::string& message) {
    try { operation(); }
    catch (const LogicError&) { return; }
    throw std::runtime_error("did not reject: " + message);
}

bool sop_value(const Sop& sop, const std::map<std::string, bool>& values) {
    for (const auto& cube : sop) {
        bool matches = true;
        for (const auto& literal : cube)
            if (values.at(literal.first) != literal.second) matches = false;
        if (matches) return true;
    }
    return false;
}

void check_sop(const std::string& text) {
    auto expr = parse_expression(text);
    auto sop = sum_of_products(expr);
    for (unsigned row = 0; row < 16; ++row) {
        std::map<std::string, bool> values{{"A", bool(row & 1)}, {"B", bool(row & 2)},
                                         {"C", bool(row & 4)}, {"D", bool(row & 8)}};
        check(evaluate(expr, values) == sop_value(sop, values), "SOP truth table: " + text);
    }
}

} // namespace

int main() {
    try {
        for (unsigned row = 0; row < 8; ++row) {
            bool a = row & 1, b = row & 2, c = row & 4;
            std::map<std::string, bool> values{{"A", a}, {"B", b}, {"C", c}};
            check(evaluate(parse_expression("/A*B + C"), values) == ((!a && b) || c), "NOT/AND/OR");
            check(evaluate(parse_expression("A :+: B"), values) == (a != b), "XOR");
            check(evaluate(parse_expression("A :*: B"), values) == (a == b), "XNOR");
            check(evaluate(parse_expression("A+B :+: C"), values) == ((a || b) != c), "OR/XOR precedence");
            check(evaluate(parse_expression("A :+: B+C"), values) == (a != (b || c)), "XOR/OR precedence");
            check(evaluate(parse_expression("A :+: B*C"), values) == (a != (b && c)), "XOR/AND precedence");
            check(evaluate(parse_expression("/(A+B)*C"), values) == (!(a || b) && c), "parentheses");
        }
        check(!evaluate(parse_expression("A+B:+:C"), {{"A", true}, {"B", false}, {"C", true}}),
              "original MACHXL precedence witness");
        check(evaluate(parse_expression("vcc * /gNd + 0"), {}), "constants");
        check(evaluate(parse_expression("_signal[ 12 ]"), {{"_signal[12]", true}}), "indexed identifier");
        rejects([] { evaluate(parse_expression("VCC + missing"), {}); }, "missing signal despite OR true");
        for (const auto& bad : {"", "A B", "A+", "A && B", "A[2..0]", "A[-1]", "A[]",
                                "A := B", "(A+B", "A+B)", "#B1", "2", "A.CLKF", "A;comment"})
            rejects([bad] { parse_expression(bad); }, bad);
        rejects([] { parse_expression(std::string(300, '/') + "A"); }, "excessive nesting");

        for (const auto& text : {"A*/A", "A+A", "A+A*B", "A*B + A*/B", "A+/A",
                                "A+/A*B", "/A*B+A", "A*C+/A*B*C", "/A*C+A*B*C",
                                "A*B+/A*/B*C", "A*B+/A*C", "A+/A*B*/C+/A*/B*C",
                                "(A+B)*(C+D)", "/((A+B)*(C+D))", "A :+: B :+: C",
                                "A :*: (B+C)", "/(A :*: (B+C))", "(A :+: B)*(C :+: D)",
                                "(A+B+C+D)*(A+/B+C+/D)", "0", "1", "/VCC", "/GND"})
            check_sop(text);
        check(sum_of_products(parse_expression("A*/A")).empty(), "contradictory cube removed");
        check(sum_of_products(parse_expression("A+A")).size() == 1, "duplicates removed");
        check(sum_of_products(parse_expression("A+A*B")) == Sop{Cube{{"A", true}}}, "absorption");
        check(sum_of_products(parse_expression("A*B+A*/B")) == Sop{Cube{{"A", true}}}, "adjacent merge");
        check(sum_of_products(parse_expression("A+/A*B")) ==
              Sop{Cube{{"A", true}}, Cube{{"B", true}}}, "covered literal removed");
        check(sum_of_products(parse_expression("A*C+/A*B*C")) ==
              Sop{Cube{{"A", true}, {"C", true}}, Cube{{"B", true}, {"C", true}}},
              "covered literal with shared condition");
        check(sum_of_products(parse_expression("A+/A*B*/C+/A*/B*C")) ==
              Sop{Cube{{"A", true}}, Cube{{"B", true}, {"C", false}},
                  Cube{{"B", false}, {"C", true}}}, "several covered literals removed");
        check(sum_of_products(parse_expression("A+/A")) == Sop{Cube{}}, "tautology");
        check(sum_of_products(parse_expression("A+B")) == Sop{Cube{{"A", true}}, Cube{{"B", true}}}, "stable order");
        rejects([] { sum_of_products(parse_expression("(A+B)*(C+D)"), {3, 1000}); }, "cube budget");
        rejects([] { sum_of_products(parse_expression("(A+B)*(C+D)"), {100, 2}); }, "work budget");
        rejects([] { sum_of_products(parse_expression("1"), {0, 100}); }, "zero cube budget");
        rejects([] { sum_of_products(nullptr); }, "null expression");
        std::cout << "logic tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
