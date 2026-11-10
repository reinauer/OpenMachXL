#ifndef MACHXL_LOGIC_HPP
#define MACHXL_LOGIC_HPP

#include <cstddef>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace machxl {

enum class ExprKind
{
    Constant,
    Identifier,
    Not,
    And,
    Or,
    Xor,
    Xnor
};

struct Expr;
using ExprPtr = std::shared_ptr<const Expr>;

struct Expr
{
    ExprKind kind = ExprKind::Constant;
    bool value    = false;
    std::string name;
    ExprPtr left;
    ExprPtr right;
};

class LogicError : public std::runtime_error
{
   public:
    using std::runtime_error::runtime_error;
};

ExprPtr make_constant(bool value);
ExprPtr make_identifier(std::string name);
ExprPtr make_not(ExprPtr operand);
ExprPtr make_binary(ExprKind kind, ExprPtr left, ExprPtr right);

// Scalar PDS expressions: /, *, +, :+:, :*:, parentheses and NAME[index].
// Precedence, highest first: /, *, +, then the shared :+: and :*: level.
// AND, OR, and mixed XOR/XNOR chains associate right.
// 0, 1, VCC and GND are constants; VCC/GND are case insensitive. Other
// identifier spelling is preserved. Vector ranges and radix values are
// rejected. Limits bound nesting and AST size.
ExprPtr parse_expression(std::string_view text);
bool evaluate(const ExprPtr& expr, const std::map<std::string, bool>& values);

// true means a positive literal, false a complemented literal.
// SOP true is one empty cube; SOP false has no cubes.
using Cube = std::map<std::string, bool>;
using Sop  = std::vector<Cube>;

struct SopLimits
{
    std::size_t max_cubes = 65536;
    // Bounds both product attempts and simplification pair comparisons.
    std::size_t max_products = 1000000;
};

// Expands to products, drops contradictions/duplicates, absorbs supersets,
// and repeatedly merges terms differing in just one complementary literal.
// Also removes a literal whose opposite branch is covered by another cube
// (for example A + /A*B becomes A + B).
// Cube order is deterministic; literals are ordered by name. This is a
// local reduction, not a claim of globally minimal or MACHXL-identical SOP.
Sop sum_of_products(const ExprPtr& expr, SopLimits limits = {});

// Boolean expansion without heuristic reduction or absorption; retains
// source product order while dropping contradictions and exact duplicates.
Sop expanded_products(const ExprPtr& expr, SopLimits limits = {});

}  // namespace machxl

#endif
