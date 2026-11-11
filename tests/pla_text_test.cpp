#include "pla_text.hpp"
#include <cassert>

int main()
{
    using namespace machxl;
    {
        const auto p = parse_pla_text("#$ TOOL AMD 2.7\n.type f\n.i 3\n.o 2\n.ilb A B\n C\n.ob Q.REG\n Q.C%\n.phase 11\n"
                                      ".p 3\n11- 1~\n--1 ~1\n0-- 1~\n.e\n1-- 11\n");
        assert(!p.malformed);
        assert((p.inputs == std::vector<std::string>{"A", "B", "C"}));
        assert((p.outputs == std::vector<std::string>{"Q.REG", "Q.C%"}));
        assert((p.terms[0] == std::vector<std::string>{"11-", "0--"}));
        assert((p.terms[1] == std::vector<std::string>{"--1"}));
    }
    {
        // Without inputs a row is the output mask alone.
        const auto p = parse_pla_text(".i 0\n.o 2\n.ob X Y\n.p 2\n1~\n11\n.e\n");
        assert(!p.malformed && p.inputs.empty());
        assert(p.terms[0].size() == 2 && p.terms[0][0].empty() && p.terms[1].size() == 1);
    }
    {
        // Rows of the wrong width are skipped and reported.
        const auto p = parse_pla_text(".i 2\n.o 1\n.ilb A B\n.ob X\n.p 2\n1 1\n10 1\n.e\n");
        assert(p.malformed && (p.terms[0] == std::vector<std::string>{"10"}));
        assert(parse_pla_text(".i 2\n.o 1\n.ilb A\n.ob X\n.e\n").malformed);
        assert(parse_pla_text(".i 1\n.o 2\n.ilb A\n.ob X\n.e\n").malformed);
    }
    {
        // An output without rows still has its (empty) column.
        const auto p = parse_pla_text(".i 1\n.o 2\n.ilb A\n.ob X Y\n.p 1\n1 1~\n.e\n");
        assert(!p.malformed && p.terms.size() == 2 && p.terms[1].empty());
    }
}
