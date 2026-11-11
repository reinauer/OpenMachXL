#include "logic.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <utility>

namespace machxl {

void append_expression_variables(std::vector<std::string>& variables, const ExprPtr& expression)
{
    std::set<std::string> seen(variables.begin(), variables.end());
    const auto visit = [&](auto&& self, const ExprPtr& e) -> void {
        if (!e)
            return;
        if (e->kind == ExprKind::Identifier && seen.insert(e->name).second)
            variables.push_back(e->name);
        self(self, e->left);
        self(self, e->right);
    };
    visit(visit, expression);
}
std::vector<std::string> expression_variables(const ExprPtr& expression)
{
    std::vector<std::string> variables;
    append_expression_variables(variables, expression);
    return variables;
}
ExprPtr make_constant(bool value)
{
    return std::make_shared<Expr>(Expr{ExprKind::Constant, value, {}, {}, {}});
}

ExprPtr make_identifier(std::string name)
{
    if (name.empty())
        throw LogicError("empty identifier");
    return std::make_shared<Expr>(Expr{ExprKind::Identifier, false, std::move(name), {}, {}});
}

ExprPtr make_not(ExprPtr operand)
{
    if (!operand)
        throw LogicError("null NOT operand");
    return std::make_shared<Expr>(Expr{ExprKind::Not, false, {}, std::move(operand), {}});
}

ExprPtr make_binary(ExprKind kind, ExprPtr left, ExprPtr right)
{
    if (!left || !right)
        throw LogicError("null binary operand");
    if (kind != ExprKind::And && kind != ExprKind::Or && kind != ExprKind::Xor && kind != ExprKind::Xnor)
        throw LogicError("invalid binary expression kind");
    return std::make_shared<Expr>(Expr{kind, false, {}, std::move(left), std::move(right)});
}

namespace {

bool is_identifier_letter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_decimal_digit(char c)
{
    return c >= '0' && c <= '9';
}

class Parser
{
   public:
    explicit Parser(std::string_view input) : input_(input) {}

    ExprPtr parse()
    {
        auto result = parse_xor();
        space();
        if (pos_ != input_.size())
            fail("unexpected trailing token");
        return result;
    }

   private:
    std::string_view input_;
    std::size_t pos_   = 0;
    std::size_t nodes_ = 0;
    std::size_t depth_ = 0;

    [[noreturn]] void fail(const char* message) const
    {
        throw LogicError(std::string(message) + " at byte " + std::to_string(pos_));
    }

    void space()
    {
        while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_])))
            ++pos_;
    }

    bool take(std::string_view token)
    {
        space();
        if (input_.substr(pos_, token.size()) != token)
            return false;
        pos_ += token.size();
        return true;
    }

    ExprPtr counted(ExprPtr expr)
    {
        if (++nodes_ > 4096)
            fail("expression exceeds 4096 AST nodes");
        return expr;
    }

    ExprPtr parse_or()
    {
        std::vector<ExprPtr> operands{parse_and()};
        while (take("+"))
            operands.push_back(parse_and());
        auto result = operands.back();
        for (std::size_t i = operands.size(); i > 1; --i)
            result = counted(make_binary(ExprKind::Or, operands[i - 2], result));
        return result;
    }

    ExprPtr parse_xor()
    {
        std::vector<ExprPtr> operands{parse_or()};
        std::vector<ExprKind> operations;
        for (;;) {
            if (take(":+:"))
                operations.push_back(ExprKind::Xor);
            else if (take(":*:"))
                operations.push_back(ExprKind::Xnor);
            else
                break;
            operands.push_back(parse_or());
        }
        // Original hardware-XOR lowering distinguishes A^(B^C) from
        // (A^B)^C: an unparenthesized chain matches the former.
        auto result = operands.back();
        for (std::size_t i = operations.size(); i > 0; --i)
            result = counted(make_binary(operations[i - 1], operands[i - 1], result));
        return result;
    }

    ExprPtr parse_and()
    {
        std::vector<ExprPtr> operands{parse_unary()};
        while (take("*"))
            operands.push_back(parse_unary());
        auto result = operands.back();
        for (std::size_t i = operands.size(); i > 1; --i)
            result = counted(make_binary(ExprKind::And, operands[i - 2], result));
        return result;
    }

    ExprPtr parse_unary()
    {
        if (++depth_ > 256)
            fail("expression nesting exceeds 256 levels");
        ExprPtr result;
        if (take("/")) {
            result = counted(make_not(parse_unary()));
        } else if (take("(")) {
            result = parse_xor();
            if (!take(")"))
                fail("expected closing parenthesis");
        } else {
            result = atom();
        }
        --depth_;
        return result;
    }

    ExprPtr atom()
    {
        space();
        if (pos_ == input_.size())
            fail("expected expression");
        if (input_[pos_] == '0' || input_[pos_] == '1') {
            bool value = input_[pos_++] == '1';
            return counted(make_constant(value));
        }
        if (!is_identifier_letter(input_[pos_]))
            fail("expected scalar identifier or constant");
        auto start = pos_++;
        while (pos_ < input_.size() && (is_identifier_letter(input_[pos_]) || is_decimal_digit(input_[pos_])))
            ++pos_;
        std::string name(input_.substr(start, pos_ - start));
        bool indexed = false;
        if (take("[")) {
            indexed = true;
            space();
            start = pos_;
            while (pos_ < input_.size() && is_decimal_digit(input_[pos_]))
                ++pos_;
            if (pos_ == start)
                fail("expected nonnegative scalar index");
            auto index = input_.substr(start, pos_ - start);
            if (!take("]"))
                fail("expected closing scalar-index bracket");
            name += "[" + std::string(index) + "]";
        }
        std::string upper = name;
        std::transform(upper.begin(), upper.end(), upper.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (!indexed && (upper == "VCC" || upper == "GND"))
            return counted(make_constant(upper == "VCC"));
        return counted(make_identifier(std::move(name)));
    }
};

bool evaluate_expression_node(const ExprPtr& expr, const std::map<std::string, bool>& values, std::size_t depth)
{
    if (!expr)
        throw LogicError("null expression");
    if (depth > 4096)
        throw LogicError("evaluation nesting exceeds 4096 levels");
    switch (expr->kind) {
    case ExprKind::Constant:
        return expr->value;
    case ExprKind::Identifier: {
        auto found = values.find(expr->name);
        if (found == values.end())
            throw LogicError("missing value for " + expr->name);
        return found->second;
    }
    case ExprKind::Not:
        return !evaluate_expression_node(expr->left, values, depth + 1);
    default:
        break;
    }
    // Evaluate both operands so an undeclared/missing signal never hides
    // behind short-circuit behavior.
    bool left  = evaluate_expression_node(expr->left, values, depth + 1);
    bool right = evaluate_expression_node(expr->right, values, depth + 1);
    switch (expr->kind) {
    case ExprKind::And:
        return left && right;
    case ExprKind::Or:
        return left || right;
    case ExprKind::Xor:
        return left != right;
    case ExprKind::Xnor:
        return left == right;
    default:
        throw LogicError("invalid expression kind");
    }
}

bool subsumes(const Cube& broad, const Cube& narrow)
{
    if (broad.size() > narrow.size())
        return false;
    for (const auto& literal : broad) {
        auto found = narrow.find(literal.first);
        if (found == narrow.end() || found->second != literal.second)
            return false;
    }
    return true;
}

// If broad differs from a subset of narrow at exactly one literal, the
// opposite-valued portion of narrow is already covered by broad. Thus
// AC + /ABC = AC + BC. Removing that literal only enlarges narrow within
// the existing union, and strictly decreases the total literal count.
bool remove_covered_literal(const Cube& broad, Cube& narrow)
{
    if (broad.size() >= narrow.size())
        return false;
    auto complement = narrow.end();
    for (const auto& literal : broad) {
        auto found = narrow.find(literal.first);
        if (found == narrow.end())
            return false;
        if (found->second != literal.second) {
            if (complement != narrow.end())
                return false;
            complement = found;
        }
    }
    if (complement == narrow.end())
        return false;
    narrow.erase(complement);
    return true;
}

class Expander
{
   public:
    explicit Expander(SopLimits limits, bool minimize = true) : limits_(limits), minimize_(minimize) {}

    Sop expand(const ExprPtr& expr, bool negate = false, std::size_t depth = 0)
    {
        if (!expr)
            throw LogicError("null expression");
        if (depth > 4096)
            throw LogicError("SOP nesting exceeds 4096 levels");
        tick();
        switch (expr->kind) {
        case ExprKind::Constant: {
            Sop result = (expr->value != negate) ? Sop{Cube{}} : Sop{};
            check_size(result.size());
            return result;
        }
        case ExprKind::Identifier:
            check_size(1);
            return Sop{Cube{{expr->name, !negate}}};
        case ExprKind::Not:
            return expand(expr->left, !negate, depth + 1);
        case ExprKind::And:
        case ExprKind::Or: {
            auto left        = expand(expr->left, negate, depth + 1);
            auto right       = expand(expr->right, negate, depth + 1);
            bool use_product = (expr->kind == ExprKind::And) != negate;
            return use_product ? product(left, right) : unite(left, right);
        }
        case ExprKind::Xor:
        case ExprKind::Xnor: {
            auto left      = expand(expr->left, false, depth + 1);
            auto not_left  = expand(expr->left, true, depth + 1);
            auto right     = expand(expr->right, false, depth + 1);
            auto not_right = expand(expr->right, true, depth + 1);
            bool exclusive = (expr->kind == ExprKind::Xor) != negate;
            if (exclusive)
                return unite(product(left, not_right), product(not_left, right));
            return unite(product(left, right), product(not_left, not_right));
        }
        }
        throw LogicError("invalid expression kind");
    }

   private:
    SopLimits limits_;
    bool minimize_;
    std::size_t operations_ = 0;

    void tick()
    {
        if (operations_ == limits_.max_products)
            throw LogicError("SOP work budget exceeded");
        ++operations_;
    }

    void check_size(std::size_t size) const
    {
        if (size > limits_.max_cubes)
            throw LogicError("SOP cube budget exceeded");
    }

    // Preserve first-seen cube order, dropping exact duplicates.
    void append(Sop& out, std::set<Cube>& seen, Cube cube)
    {
        if (seen.insert(cube).second) {
            check_size(out.size() + 1);
            out.push_back(std::move(cube));
        }
    }

    Sop reduce(Sop cubes)
    {
        if (!minimize_)
            return cubes;
        bool changed = true;
        while (changed) {
            changed = false;
            for (std::size_t i = 0; i < cubes.size() && !changed; ++i) {
                for (std::size_t j = i + 1; j < cubes.size(); ++j) {
                    tick();
                    if (subsumes(cubes[i], cubes[j])) {
                        cubes.erase(cubes.begin() + j);
                        changed = true;
                        break;
                    }
                    if (subsumes(cubes[j], cubes[i])) {
                        cubes.erase(cubes.begin() + i);
                        changed = true;
                        break;
                    }
                    if (cubes[i].size() != cubes[j].size()) {
                        if (remove_covered_literal(cubes[i], cubes[j]) || remove_covered_literal(cubes[j], cubes[i])) {
                            changed = true;
                            break;
                        }
                        continue;
                    }
                    std::size_t differences = 0;
                    std::string differing_name;
                    auto a = cubes[i].begin();
                    auto b = cubes[j].begin();
                    for (; a != cubes[i].end(); ++a, ++b) {
                        if (a->first != b->first) {
                            differences = 2;
                            break;
                        }
                        if (a->second != b->second) {
                            ++differences;
                            differing_name = a->first;
                        }
                    }
                    if (differences == 1) {
                        cubes[i].erase(differing_name);
                        cubes.erase(cubes.begin() + j);
                        changed = true;
                        break;
                    }
                }
            }
        }
        return cubes;
    }

    Sop unite(const Sop& left, const Sop& right)
    {
        Sop result;
        std::set<Cube> seen;
        for (const auto& cube : left)
            append(result, seen, cube);
        for (const auto& cube : right)
            append(result, seen, cube);
        return reduce(std::move(result));
    }

    Sop product(const Sop& left, const Sop& right)
    {
        Sop result;
        std::set<Cube> seen;
        for (const auto& a : left) {
            for (const auto& b : right) {
                tick();
                Cube cube          = a;
                bool contradiction = false;
                for (const auto& literal : b) {
                    auto inserted = cube.emplace(literal);
                    if (!inserted.second && inserted.first->second != literal.second) {
                        contradiction = true;
                        break;
                    }
                }
                if (!contradiction)
                    append(result, seen, std::move(cube));
            }
        }
        return reduce(std::move(result));
    }
};

}  // namespace

ExprPtr parse_expression(std::string_view text)
{
    return Parser(text).parse();
}

bool evaluate(const ExprPtr& expr, const std::map<std::string, bool>& values)
{
    return evaluate_expression_node(expr, values, 0);
}

Sop sum_of_products(const ExprPtr& expr, SopLimits limits)
{
    return Expander(limits).expand(expr);
}

Sop expanded_products(const ExprPtr& expr, SopLimits limits)
{
    return Expander(limits, false).expand(expr);
}

}  // namespace machxl
