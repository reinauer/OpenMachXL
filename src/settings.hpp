#pragma once

#include <map>
#include <string>

namespace machxl {
class CompilerSettings
{
   public:
    CompilerSettings();
    const std::string& get(const std::string& key) const;
    bool yes_no(const std::string& key) const;
    bool fitter_yes_no(const std::string& key) const;
    unsigned integer(const std::string& key) const;
    bool dos_compatibility() const { return dos_compatibility_; }
    const std::map<std::string, std::string>& values() const { return values_; }
    void parse(const std::string& source, bool dos_compatibility = false);

   private:
    std::map<std::string, std::string> values_;
    bool dos_compatibility_ = false;
};
}  // namespace machxl
