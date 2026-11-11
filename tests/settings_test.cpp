#include "settings.hpp"
#include "logic.hpp"
#include <iostream>
#include <stdexcept>

using namespace machxl;
int main() {
    try {
        CompilerSettings settings;
        settings.parse("; options\r\n[VERSION]=MACHXL 2.0\r\n[P_N]=N\r\n[THRESHOLD]= 12 \r\n[WORK_DIR]=C:\\WORK\r\n");
        if (settings.yes_no("p_n") || settings.integer("THRESHOLD") != 12 || settings.get("WORK_DIR") != "C:\\WORK")
            throw std::runtime_error("settings not preserved");
        settings.parse("[P_N]=Y\n[p_n]=N\n");
        if(settings.yes_no("P_N"))throw std::runtime_error("last setting must win");
        for (auto bad : { "[VERSION]=MACHXL 9", "[P_N]Y", "not a setting"}) {
            bool rejected = false;
            try { settings.parse(bad); } catch (const LogicError&) { rejected = true; }
            if (!rejected || settings.yes_no("P_N")) throw std::runtime_error("settings parse is not atomic");
        }
        CompilerSettings legacy;
        legacy.parse("[M_POLAR]=low, active low\n",true);
        if(legacy.get("M_POLAR")!="As specified in design file")throw std::runtime_error("DOS polarity first byte");
        legacy.parse("[M_POLAR]=Low, active low\n[M_POLAR] =High, active high\n",true);
        if(legacy.get("M_POLAR")!="As specified in design file")throw std::runtime_error("DOS delimiter boundary");
        legacy.parse("[D_T_REG_OPTM]=prefix T-type\n",true);
        if(legacy.get("D_T_REG_OPTM")!="Change all to T-type")throw std::runtime_error("DOS register substring");
        legacy.parse("[D_T_REG_OPTM]=12345678901234567890123456789T-type\n",true);
        if(legacy.get("D_T_REG_OPTM")!="D,T As specified. JK,RS to D")throw std::runtime_error("DOS fixed width");
        legacy.parse("ignored text\n[UNKNOWN]=Y\n[VERSION]=anything\n[p_n]=y\n[PAIR]=n\n[PTERM_CLK]=n\n",true);
        if(legacy.yes_no("P_N")||legacy.yes_no("PAIR")||!legacy.fitter_yes_no("P_N")||
           !legacy.fitter_yes_no("PAIR")||!legacy.yes_no("PTERM_CLK"))
            throw std::runtime_error("DOS stage-specific raw Boolean interpretation");
        legacy.parse("[FAST_MIN]=Yes\n [FAST_MIN]=N\n",true);
        if(!legacy.yes_no("FAST_MIN"))throw std::runtime_error("DOS keys require column zero");
        legacy.parse("[FAST_MIN]=Y\n[FAST_MIN] =Y\n",true);
        if(legacy.yes_no("FAST_MIN"))throw std::runtime_error("DOS missing delimiter clears field");
        legacy.parse("[P_N]=N\n",true);
        if(legacy.fitter_yes_no("P_N")||legacy.yes_no("BALANCED_PAR")||
           !legacy.yes_no("M3XX_SET_RST")||legacy.get("BPP_SIGNAL")!="Don't care")
            throw std::runtime_error("DOS partial file must start with zeroed buffers");
        legacy.parse("[THRESHOLD_12]=1234\n[THRESHOLD_2X]=16\n[BUCKET_SIZE]=12x\n",true);
        if(legacy.integer("THRESHOLD_12")!=12316||legacy.integer("BUCKET_SIZE")!=12)
            throw std::runtime_error("DOS fixed-width shared-buffer boundary");
        legacy.parse("[THRESHOLD]=0002\n[M3XX_RUNTIME]=12x\n[SIZE_REDUCE_N]=+2\n",true);
        if(legacy.integer("THRESHOLD")!=0||legacy.integer("M3XX_RUNTIME")!=12||
           legacy.integer("SIZE_REDUCE_N")!=0)
            throw std::runtime_error("DOS numeric consumer policies");
        for(const auto& record:{"[THRESHOLD]=12x\n","[THRESHOLD]=+2\n","[M3XX_RUNTIME]= 2\n"}) {
            legacy.parse(record,true);bool invalid=false;
            try {legacy.integer(std::string(record).find("RUNTIME")!=std::string::npos?"M3XX_RUNTIME":"THRESHOLD");}
            catch(const LogicError&){invalid=true;}
            if(!invalid)throw std::runtime_error("DOS invalid numeric token accepted");
        }
        legacy.parse("[SIM_PLACE]=Designjunk\n[BPP_SIGNAL]=off\n",true);
        if(legacy.get("SIM_PLACE")!="Design file"||legacy.get("BPP_SIGNAL")!="Don't care")
            throw std::runtime_error("DOS first-byte text policy");
        legacy.parse("[FAST_MIN]=Yes\x1a[FAST_MIN]=N\n",true);
        if(!legacy.yes_no("FAST_MIN"))throw std::runtime_error("DOS text EOF");
        legacy.parse(std::string(109,'x')+"[FAST_MIN]=Y\n",true);
        if(!legacy.yes_no("FAST_MIN"))throw std::runtime_error("DOS fgets record width");
        settings.parse("[THRESHOLD]=42949672960\n");
        bool rejected = false;
        try { settings.integer("THRESHOLD"); } catch (const LogicError&) { rejected = true; }
        if (!rejected) throw std::runtime_error("integer overflow accepted");
        std::cout << "settings tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
