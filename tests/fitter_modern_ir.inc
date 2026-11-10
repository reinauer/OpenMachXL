// Test signal inputs and expected cells/pins from the saved PLC files.
// Source and PLC hashes identify the reference cases below.
// TEST2 PDS sha256 c437bab3fdd472813dff6c35db269068f8862ce4eea376bf32167daae5583f05
// PLC sha256 e7ca3f0a7d77536700e9725b3107506d2bd61ee8fa15418b95d8f260a184deb4
void check_test2(const MachDeviceLayout& layout) {
std::vector<Mach111PlaceSignal> signals;
{ Mach111PlaceSignal s; s.name="CLOCK1"; s.order.serial=213; s.order.type=9; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="SENSOR"; s.order.serial=214; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="VOUT[5]"; s.order.serial=215; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=1; s.fanins={224,236};s.reset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="VOUT[4]"; s.order.serial=216; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=1; s.fanins={214,224,235,236};s.reset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="VOUT[3]"; s.order.serial=217; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=2; s.fanins={214,224,235,236};s.reset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="VOUT[2]"; s.order.serial=218; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=1; s.fanins={224,236};s.reset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="VOUT[1]"; s.order.serial=219; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=1; s.fanins={214,224,235,236};s.reset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="VOUT[0]"; s.order.serial=220; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=2; s.fanins={214,224,235,236};s.reset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ST[1]"; s.order.serial=221; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=1; s.fanins={224,236};s.reset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ST[0]"; s.order.serial=222; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=2; s.fanins={214,224,235,236};s.reset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="CLOCK2"; s.order.serial=223; s.order.type=9; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="_RESET1"; s.order.serial=224; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="_RESET2"; s.order.serial=225; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="RING"; s.order.serial=226; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ENDGREETING"; s.order.serial=227; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="DIALTONE"; s.order.serial=228; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ENDMESSAGES"; s.order.serial=229; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ST2[1]"; s.order.serial=230; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=2; s.fanins={225,227,228,229,237,238};s.reset=2;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ST2[0]"; s.order.serial=231; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=2; s.fanins={225,226,227,228,237,238};s.reset=2;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ANSWER"; s.order.serial=232; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={225,226,228,229,237,238};s.reset=2;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PLAY"; s.order.serial=233; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=2; s.fanins={225,226,227,228,237,238};s.reset=2;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="RECORD"; s.order.serial=234; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=2; s.fanins={225,227,228,229,237,238};s.reset=2;signals.push_back(s); }
for(auto& s:signals)s.global_reset=s.reset;
const auto p=place_mach_modern(layout,signals);
CHECK(!p.global_controls[1][0] && p.global_controls[1][1]==1);
CHECK((p.pins==std::vector<int>{20,6,12,75,73,54,56,14,77,71,23,5,4,9,7,3,13,10,35,50,29,8}));
CHECK((p.cells==std::vector<int>{-1,-1,20,116,100,84,80,16,112,96,-1,-1,-1,-1,-1,-1,-1,4,48,64,32,0}));
}
// P7_SRC PDS sha256 5aa7d731a240471db7fb564188829a26c40fa536a10f57d166ad8113af3ef6f0
// PLC sha256 3c854068f0de3827e543baf4b859c9377c49e6f693b4d69c145a6c34432f47e4
void check_p7_src(const MachDeviceLayout& layout) {
std::vector<Mach111PlaceSignal> signals;
{ Mach111PlaceSignal s; s.name="CLK"; s.order.serial=213; s.order.type=9; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="CE"; s.order.serial=214; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="LD"; s.order.serial=215; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="RST"; s.order.serial=216; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[0]"; s.order.serial=217; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[1]"; s.order.serial=218; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[2]"; s.order.serial=219; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[3]"; s.order.serial=220; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[4]"; s.order.serial=221; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[5]"; s.order.serial=222; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[6]"; s.order.serial=223; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[7]"; s.order.serial=224; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[8]"; s.order.serial=225; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[9]"; s.order.serial=226; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[10]"; s.order.serial=227; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[11]"; s.order.serial=228; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[12]"; s.order.serial=229; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[13]"; s.order.serial=230; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[14]"; s.order.serial=231; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[15]"; s.order.serial=232; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCAX"; s.order.serial=233; s.order.type=3; s.order.fixed_pin=-1; s.output=true; s.buried=true; s.order.product_terms=2; s.fanins={215,216,217,218,219,220,221,222,223,224,250,251,252,253,254,255,256,257};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[0]"; s.order.serial=234; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,217,250};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[1]"; s.order.serial=235; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,218,250,251};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[2]"; s.order.serial=236; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,219,250,251,252};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[3]"; s.order.serial=237; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,220,250,251,252,253};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[4]"; s.order.serial=238; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,221,250,251,252,253,254};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[5]"; s.order.serial=239; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,222,250,251,252,253,254,255};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[6]"; s.order.serial=240; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,223,250,251,252,253,254,255,256};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[7]"; s.order.serial=241; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,224,250,251,252,253,254,255,256,257};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[8]"; s.order.serial=242; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,225,233,257,258};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[9]"; s.order.serial=243; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,226,233,257,258,259};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[10]"; s.order.serial=244; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,227,233,257,258,259,260};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[11]"; s.order.serial=245; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,228,233,257,258,259,260,261};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[12]"; s.order.serial=246; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,229,233,257,258,259,260,261,262};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[13]"; s.order.serial=247; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,230,233,257,258,259,260,261,262,263};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[14]"; s.order.serial=248; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,231,233,257,258,259,260,261,262,263,264};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[15]"; s.order.serial=249; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,232,233,257,258,259,260,261,262,263,264,265};s.reset=2;s.preset=1;signals.push_back(s); }
const auto p=place_mach_modern(layout,signals);
CHECK((p.pins==std::vector<int>{20,5,4,3,17,16,15,13,9,7,6,30,28,27,26,25,24,23,19,18,-1,73,39,33,35,12,71,14,50,54,56,10,75,8,31,29,77}));
CHECK((p.cells==std::vector<int>{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,68,100,56,52,48,20,96,16,64,84,80,4,116,0,36,32,112}));
}
// P8_SRC PDS sha256 af3acc400a0a9bfe0399a8c750a62843d6d5c578970481f943161d26c30f437b
// PLC sha256 e5dd101bc8dd677aabf84fb699e81f2e4904c4d05cbc6e067587750dd4bdbe1d
void check_p8_src(const MachDeviceLayout& layout) {
std::vector<Mach111PlaceSignal> signals;
{ Mach111PlaceSignal s; s.name="CLK"; s.order.serial=213; s.order.type=9; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="CE"; s.order.serial=214; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="LD"; s.order.serial=215; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="RST"; s.order.serial=216; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[0]"; s.order.serial=217; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[1]"; s.order.serial=218; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[2]"; s.order.serial=219; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[3]"; s.order.serial=220; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[4]"; s.order.serial=221; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[5]"; s.order.serial=222; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[6]"; s.order.serial=223; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[7]"; s.order.serial=224; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[8]"; s.order.serial=225; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[9]"; s.order.serial=226; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[10]"; s.order.serial=227; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[11]"; s.order.serial=228; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[12]"; s.order.serial=229; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[13]"; s.order.serial=230; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[14]"; s.order.serial=231; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="ID[15]"; s.order.serial=232; s.order.type=1; s.order.fixed_pin=-1; s.output=false; s.buried=false; s.order.product_terms=0; s.fanins={};signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[0]"; s.order.serial=233; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,217,252};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[1]"; s.order.serial=234; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,218,252,253};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[2]"; s.order.serial=235; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,219,252,253,254};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[3]"; s.order.serial=236; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,220,252,253,254,255};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[4]"; s.order.serial=237; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,221,249,256};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[5]"; s.order.serial=238; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,222,249,256,257};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[6]"; s.order.serial=239; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,223,249,256,257,258};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[7]"; s.order.serial=240; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,224,249,256,257,258,259};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[8]"; s.order.serial=241; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,225,249,250,260};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[9]"; s.order.serial=242; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,226,249,250,260,261};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[10]"; s.order.serial=243; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,227,249,250,260,261,262};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[11]"; s.order.serial=244; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,228,249,250,260,261,262,263};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[12]"; s.order.serial=245; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,229,249,251,264};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[13]"; s.order.serial=246; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,230,249,251,264,265};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[14]"; s.order.serial=247; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,231,249,251,264,265,266};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCA[15]"; s.order.serial=248; s.order.type=0; s.order.fixed_pin=-1; s.output=true; s.buried=false; s.order.product_terms=3; s.fanins={214,215,216,232,249,251,264,265,266,267};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCAX"; s.order.serial=249; s.order.type=3; s.order.fixed_pin=-1; s.output=true; s.buried=true; s.order.product_terms=1; s.fanins={216,252,253,254,255};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCAY"; s.order.serial=250; s.order.type=3; s.order.fixed_pin=-1; s.output=true; s.buried=true; s.order.product_terms=1; s.fanins={216,256,257,258,259};s.reset=2;s.preset=1;signals.push_back(s); }
{ Mach111PlaceSignal s; s.name="PCAZ"; s.order.serial=251; s.order.type=3; s.order.fixed_pin=-1; s.output=true; s.buried=true; s.order.product_terms=1; s.fanins={216,250,260,261,262,263};s.reset=2;s.preset=1;signals.push_back(s); }
const auto p=place_mach_modern(layout,signals);
CHECK((p.pins==std::vector<int>{20,5,4,3,30,28,27,26,25,24,23,19,18,17,16,15,13,9,7,6,31,54,56,29,33,35,75,77,52,12,14,50,10,73,8,71,-1,-1,-1}));
CHECK((p.cells==std::vector<int>{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,36,84,80,32,52,48,116,112,68,20,16,64,4,100,0,96,88,56,24}));
}
