#include "frontend.hpp"
#include <iostream>
#include <stdexcept>

using namespace machxl;
namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void rejects(const std::string& source) {
    try { (void)emit_pla(parse_design(source), "test.pds"); }
    catch (const std::exception&) { return; }
    throw std::runtime_error("invalid/unsupported source was accepted");
}
}
int main() {
    try {
        const std::string prefix = "CHIP T MACH111\nPIN ? A\nPIN ? B\nPIN ? Y[1..0]\n";
        rejects("CHIP PAIR MACH111\nPIN ? A\nPIN ? Q\nEQUATIONS\nQ=A\n");
        rejects("CHIP LAT MACH111\nPIN ? A\nPIN ? Q\nEQUATIONS\nQ=A\n");
        rejects("CHIP MACREF MACH111\nPIN ? A\nPIN ? Q\nEQUATIONS\nQ=A\n");
        auto metadata=parse_design("TITLE A PIN test with NODE text\n"+prefix+"EQUATIONS\nY[0]=A\n");
        require(metadata.metadata.at("TITLE")=="A PIN test with NODE text","metadata keyword text");
        auto d = parse_design(prefix + "EQUATIONS\nY[1]=A Y[0]=/B\nSIMULATION\nCHECK Y[1]\n");
        require(d.equations.size() == 2, "same-line equations");
        require(evaluate(d.equations[1].expression, {{"B", false}}), "inverted equation");
        auto vector = parse_design(prefix + "EQUATIONS\nY[1..0]=A+B\n");
        require(vector.equations[0].signal == "Y[1]" && vector.equations[1].signal == "Y[0]", "vector order");
        auto alias = parse_design(prefix + "STRING X 'A+\nB'\nEQUATIONS\nY[1]=/X Y[0]=GND\n");
        require(evaluate(alias.equations[0].expression, {{"A", true}, {"B", true}}), "STRING must not add parentheses");
        const std::string auxiliary="; external commands\nSIMULATION\nSTIM\nCHECK Y[0]\n";
        auto auxiliary_design=parse_design(prefix+"STRING STIM 'SETF A'\nEQUATIONS\nY[0]=A\nSIMULATION\nSETF /A\n",{},&auxiliary);
        require(auxiliary_design.simulation.find("SETF /A")<auxiliary_design.simulation.find("SETF A"),"auxiliary commands append after embedded simulation and expand source STRING");
        const std::string bad_auxiliary="SETF A\n";
        bool bad_aux_rejected=false;try{(void)parse_design(prefix+"EQUATIONS\nY[0]=A\n",{},&bad_auxiliary);}catch(const std::exception&){bad_aux_rejected=true;}
        require(bad_aux_rejected,"original auxiliary file requires SIMULATION header");
        const std::string auxiliary_table="STRING TABLE_INPUT 'SETF A'\nSTRING TABLE_OUTPUT 'Y[0]'\nSIMULATION\nTABLE_INPUT\nCHECK TABLE_OUTPUT\n";
        auto table_design=parse_design(prefix+"EQUATIONS\nY[0]=A\n",{},&auxiliary_table);
        require(table_design.simulation.find("SETF A")!=std::string::npos&&table_design.simulation.find("CHECK Y[0]")!=std::string::npos,"auxiliary TBL aliases expand only simulation commands");
        require(table_design.equations.size()==1&&table_design.pins.size()==4,"auxiliary table does not change fitted logic");

        rejects(prefix + "EQUATIONS\nY[0]=UNKNOWN*GND\n");
        rejects(prefix + "EQUATIONS\nY[0]=A*0+B*1\n");
        rejects(prefix + "EQUATIONS\nY[0]=Y[1..0]\n");
        auto joined=parse_design(prefix + "EQUATIONS\nY[0]=A Y[0]=B\n");
        require(evaluate(joined.equations[0].expression,{{"A",false},{"B",true}}),"repeated equations OR their covers");
        auto conditional=parse_design(prefix+"EQUATIONS\nIF A THEN BEGIN Y[0]=B END ELSE BEGIN Y[0]=/B END\n");
        for(bool a:{false,true})for(bool b:{false,true})require(evaluate(conditional.equations[0].expression,{{"A",a},{"B",b}})==(a?b:!b),"IF/ELSE lowering");
        // Both first and later CASE branches retain the same truth table
        // despite their different original numeric-scanner high words.
        for(unsigned lo=0;lo<8;++lo)for(unsigned hi=lo+1;hi<8;++hi)for(bool later:{false,true})for(bool dos_mode:{false,true}) {
            auto text=std::string("CHIP R MACH111\nPIN ? A[2..0]\nPIN ? Q\nPIN ? Z\nEQUATIONS\nCASE(A[2..0]) BEGIN ")+
                (later?"7: BEGIN Z=1 END ":"")+std::to_string(lo)+".."+std::to_string(hi)+": BEGIN Q=1 END END\n";
            FrontendOptions opts;opts.dos_compatibility=dos_mode;auto range=parse_design(text,opts);
            for(const auto& eq:range.equations)if(eq.signal=="Q")for(unsigned value=0;value<8;++value)
                require(evaluate(eq.expression,{{"A[0]",bool(value&1)},{"A[1]",bool(value&2)},{"A[2]",bool(value&4)}})==(lo<=value&&value<=hi),"ordered CASE range truth table");
        }
        FrontendOptions dos_options;dos_options.dos_compatibility=true;
        for(bool range:{false,true}) {
            auto source=std::string("CHIP W MACH435\nPIN ? A[16..0]\nPIN ? Q\nEQUATIONS\nCASE(A[16..0]) BEGIN ")+
                (range?"65536..65542":"65536")+": BEGIN Q=1 END END\n";
            auto normal=parse_design(source),compat=parse_design(source,dos_options);
            for(unsigned value:{0u,1u,6u,7u,65535u,65536u,65542u,65543u,131071u}) {
                std::map<std::string,bool> inputs;
                for(unsigned bit=0;bit<17;++bit)inputs["A["+std::to_string(bit)+"]"]=bool(value&(1u<<bit));
                require(evaluate(normal.equations[0].expression,inputs)==(value>=65536&&value<=(range?65542u:65536u)),"default CASE uses full numeric value");
                require(evaluate(compat.equations[0].expression,inputs)==(value<=(range?6u:0u)),"compatibility CASE retains DOS truncation");
            }
        }
        auto cases=parse_design(prefix+"EQUATIONS\nCASE (A,B) BEGIN #B00..#B10: BEGIN Y[1..0]=#B01 END OTHERWISE: BEGIN Y[1..0]=#B10 END END\n");
        require(cases.equations[0].inverted && !cases.equations[1].inverted,"first numeric assignment selects polarity");
        for(bool a:{false,true})for(bool b:{false,true}) {
            require(evaluate(cases.equations[0].expression,{{"A",a},{"B",b}})==!(a&&b),"CASE range off-set");
            require(evaluate(cases.equations[1].expression,{{"A",a},{"B",b}})==!(a&&b),"CASE OTHERWISE");
        }
        FrontendOptions dc;dc.conditional_default_off=false;
        auto cared=parse_design(prefix+"EQUATIONS\nIF A THEN BEGIN Y[0]=B END\n",dc);
        require(!evaluate(cared.equations[0].care,{{"A",false}})&&evaluate(cared.equations[0].care,{{"A",true}}),"conditional care preserved");
        auto copied=parse_design(prefix+"EQUATIONS\nY[0]=A Y[0]={Y[0]}+B Y[1]={Y[0]}\n");
        require(evaluate(copied.equations[1].expression,{{"A",false},{"B",true}}),"self and other equation copies");
        auto latch=parse_design("CHIP T MACH435\nPIN ? D PIN ? C PIN ? Q\nEQUATIONS\nQ*=D Q.CLKF=C\nMINIMIZE_OFF\nD=Q*C+Q*/C\n");
        require(latch.equations[0].function=="G"&&!latch.equations[2].minimize,"latch and minimization policy");
        const std::string machine="CHIP T MACH231\nPIN ? C\nPIN ? I\nPIN ? Q REG\nEQUATIONS\nQ.CLKF=C\nSTATE\nMOORE_MACHINE\nS0=/Q S1=Q S0:=GO->S1 S1:=GO->S0\nCONDITIONS\nGO=I\n";
        auto state=parse_design(machine);auto next=state.equations.back().expression;
        for(bool q:{false,true})for(bool i:{false,true})require(evaluate(next,{{"Q",q},{"I",i}})==(!q&&i),"original STATE absent-branch zero behavior");
        const std::string startup="CHIP S MACH111\nPIN ? A\nPIN ? CLK\nPIN ? Y\nSTATE\nMOORE_MACHINE\nCLKF=CLK\nSTART_UP:=POWER_UP->S2\nS0:=A->S1 +->S0\nS1:=A->S2 +->S0\nS2:=VCC->S0\nS0.OUTF=/Y S1.OUTF=Y S2.OUTF=Y\n";
        for(bool compat:{false,true}) {
            FrontendOptions opts;opts.dos_compatibility=compat;
            auto initialized=parse_design(startup,opts);bool found=false;
            for(const auto& eq:initialized.equations)if(eq.signal=="Y") {
                require(evaluate(eq.expression,{{"_ST0",false},{"_ST1",false}})==!compat,"START_UP correction versus original ignored target");found=true;
            }
            require(found,"startup state output exists");
        }
        rejects(prefix+"EQUATIONS\nIF A THEN Y[0]=B\n");
        rejects(prefix+"EQUATIONS\nIF A THEN BEGIN Y[0]=B\n");
        rejects(prefix+"EQUATIONS\nCASE(A) BEGIN #B10: Y[0]=B END\n");
        rejects(prefix + "STRING X 'X'\nEQUATIONS\nY[0]=X\n");
        rejects("CHIP T MACH111\nPIN 22 A\nPIN ? Y\nEQUATIONS\nY=A\n");
        auto unclocked=parse_design("CHIP T MACH111\nPIN ? A\nPIN ? Y REG\nEQUATIONS\nY:=A\n");
        require(unclocked.equations.size()==1&&unclocked.equations[0].function=="REG","unspecified register clock remains unspecified in PLA");
        auto startup_output=startup;
        startup_output.replace(startup_output.find("PIN ? Y"),7,"PIN ? Y REG");
        startup_output.insert(startup_output.find("S0:="),"START_UP.OUTF:=POWER_UP->Y\n");
        auto requested=parse_design(startup_output);
        require(requested.requested_power_up.at("Y"),"registered output startup requirement");
        require(emit_pla(requested,"test.pds").find("Y.REG-")!=std::string::npos,"startup forces physical output phase");
        auto ignored=parse_design(startup_output,dos_options);
        require(ignored.requested_power_up.empty()&&!ignored.diagnostics.empty(),"compatibility startup omission is diagnosed");
        const std::string constant_pair="CHIP CONST MACH435\nPIN ? Q\nPIN ? R\nEQUATIONS\nQ=GND\nR=VCC\n";
        require(emit_pla(parse_design(constant_pair),"test.pds").find("\n ~1\n")!=std::string::npos,"corrected zero-input constants retain false output");
        require(emit_pla(parse_design(constant_pair,dos_options),"test.pds").find("\n 11\n")!=std::string::npos,"compatibility constant conversion aliases first output bits");
        auto both_true=constant_pair;both_true.replace(both_true.find("Q=GND"),5,"Q=VCC");
        require(emit_pla(parse_design(both_true,dos_options),"test.pds").find(".p 2\n 11\n 11\n")!=std::string::npos,"compatibility constant rows preserve occupied-output merge rule");
        const std::string constant_single="CHIP CONST MACH435\nPIN ? Q\nEQUATIONS\nQ=VCC\n";
        require(emit_pla(parse_design(constant_single),"test.pds").find(".ilb\n")!=std::string::npos,"corrected single constant has mandatory input label directive");
        require(emit_pla(parse_design(constant_single,dos_options),"test.pds").find(".ilb")==std::string::npos,"compatibility reproduces original missing input directive");
        std::cout << "frontend tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
