// Expected product allocation under high resource pressure.
void check_stress_borrowing(const MachDeviceLayout& layout) {
 { // MS0 OUT sha256 5b2e4e0a0ead40da57f8096f4484a65dc7364b1fbcca9c22f08ca8f4911ddef1
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,-1,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MS1 OUT sha256 3055a3d6e53a59bbbf953097a0c6aadc2939b1569eacedeb8a164ac6b0d9f106
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==12);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,4,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,0,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MS2 OUT sha256 afc0cc6834a76b7f9a7743c527a83e19c95c00b5ab777ebdc7d7600b6ceacee3
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=2;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="T";s.output=true;s.order.serial=6;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==0);
 CHECK(p.cells[9]==8);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,-1,-1,-1,8,8,-1,-1,12,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,8,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MS3 OUT sha256 e5a945795e67d268ae3c35e21b8ad71a084c6b947ec3405b3403ac2b7ac56ccb
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MS4 OUT sha256 f05c91200e94665ec2f1aace3fd2e2bbea007c91b67da29cdeb52330cc992d2e
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==12);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MS5 OUT sha256 2017a34685d50c77fc574a3797b7dbcb3db99fddcb16871e036146972e2c7e47
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="T";s.output=true;s.order.serial=6;s.order.fixed_pin=s.order.serial;s.order.product_terms=9;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==0);
 CHECK(p.cells[9]==8);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,4,4,-1,-1,8,8,-1,-1,12,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,4,-1,-1,-1,8,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MS6 OUT sha256 5ade9752538d6e4a98c7500d1d781cb4df470285d4cc99e095099d82dd9a4d68
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MS7 OUT sha256 50b0ab8e578bac233643d4b5650d21eacde6e4972560cd785e83d2665d629b61
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==12);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MS8 OUT sha256 0123ddebcc0e8bbe6dcc408c413b096c4a738cc6e780f9d3f2f705aec1741b10
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="T";s.output=true;s.order.serial=6;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==12);
 CHECK(p.cells[9]==8);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,12,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,0,-1,4,-1,-1,-1,8,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MS9 OUT sha256 f1c554cfff33e1813bf0453b2797f03223ada78330e278cb9fe6455c7fb0fd9c
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,12,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MS10 OUT sha256 dc2033faec6cfc9d2260f3609450cc84ae327e6e10b18d0319e3bc4b38c145f4
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==1);
 CHECK(p.cells[7]==0);
 CHECK(p.cells[8]==12);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,12,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MS11 OUT sha256 877dfcb5feb1702eb1407d070333c6d9faf3e4901829bb1fa94f79015e192d84
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=2;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="T";s.output=true;s.order.serial=6;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==1);
 CHECK(p.cells[7]==0);
 CHECK(p.cells[8]==12);
 CHECK(p.cells[9]==8);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,1,0,-1,-1,-1,-1,-1,8,-1,-1,-1,12,12,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,0,-1,-1,-1,-1,-1,8,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MS12 OUT sha256 12a42bae95c9580aa4aca36420c7535959378d0e4545c84a8bcb44d2564ec79a
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MS13 OUT sha256 5c8b9848671f78b3741a8452f38f4847c1bfcfd552e3d9b44c25ef2d8d308852
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==12);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MS14 OUT sha256 22c4e7f322caf1181af88c34859d18007ffe51ee8ef0a9375246554b62e7be3a
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=9;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=2;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="T";s.output=true;s.order.serial=6;s.order.fixed_pin=s.order.serial;s.order.product_terms=9;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==12);
 CHECK(p.cells[9]==8);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,-1,-1,-1,8,8,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,8,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MS15 OUT sha256 1101bec32bd0ebee7dc5a1a155f58cd8962ed973f9b805105244e92bba40c2af
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MS16 OUT sha256 9c21b7f2bc94390642ed7ca44838c4c5ab2c8581ccbacb464197d36a229614e0
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=9;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,-1,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MS17 OUT sha256 857c8e3fc166f001a6a8dca88af3d26acb73601a380aacf9c09ef8b7ce727177
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="T";s.output=true;s.order.serial=6;s.order.fixed_pin=s.order.serial;s.order.product_terms=2;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==0);
 CHECK(p.cells[9]==8);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,4,-1,-1,-1,8,-1,-1,-1,12,12,12,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,12,-1}));
 }
 { // MS18 OUT sha256 c6ab01dfb2dbab07865b7e93935aa85cc991aceccdbd78b19ec27d9db64835c5
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=2;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MS19 OUT sha256 cf5bac97541d44005bcbfb7889dbdac919182ea2aa2fba4f7a589d4e04b71fcd
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,4,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,0,-1,4,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MS20 OUT sha256 8d8ac0f5a9b296659ce5df85ab6c887e1fbbcda38979368f26ab6fd5566be290
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=2;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="T";s.output=true;s.order.serial=6;s.order.fixed_pin=s.order.serial;s.order.product_terms=9;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==12);
 CHECK(p.cells[9]==8);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,4,-1,-1,-1,8,8,-1,-1,12,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,8,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MS21 OUT sha256 2318dcf8e5cc3fed6a16c185e1e6c2ac9ab496c774667fe6fabff18e6aaddf58
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=9;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MS22 OUT sha256 dc4dde3bee92cfa0935483caf120875fce74a759f13323a2563e884d03203e0c
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==1);
 CHECK(p.cells[7]==0);
 CHECK(p.cells[8]==12);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MS23 OUT sha256 9b2d271a803edc225f236d9dc9f651a6ae0c48373a9213dbfd835f8fe18f6e13
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=2;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="T";s.output=true;s.order.serial=6;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==0);
 CHECK(p.cells[8]==1);
 CHECK(p.cells[9]==8);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,1,0,-1,-1,-1,-1,8,8,8,-1,-1,12,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,0,-1,-1,-1,-1,8,8,8,-1,-1,12,-1,-1,-1}));
 }
 { // MS24 OUT sha256 e5a945795e67d268ae3c35e21b8ad71a084c6b947ec3405b3403ac2b7ac56ccb
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MS25 OUT sha256 9ca3c7eddfc79122ba5d467abad5a953c27676d53b89c8c00f8482ab5a514e87
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==0);
 CHECK(p.cells[8]==1);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,1,-1,-1,-1,-1,-1,-1,-1,12,12,12,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MS26 OUT sha256 a4191162506615f82af654f95e605c3fbd96b50dca627bcd81efd058241359aa
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=9;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="T";s.output=true;s.order.serial=6;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==1);
 CHECK(p.cells[7]==0);
 CHECK(p.cells[8]==12);
 CHECK(p.cells[9]==8);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,-1,-1,-1,-1,-1,8,8,8,8,-1,12,12,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,1,-1,-1,-1,-1,-1,-1,8,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MS27 OUT sha256 591107ca3c12838f9b2047830b891fc653bdcdbe3cd5e821d1cc6b64a2f6d25c
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=9;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,-1,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MS28 OUT sha256 7fa526929ccba2fe5a0ab1ff280b50697d771538349a759f981ce3344220065a
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=9;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==1);
 CHECK(p.cells[7]==0);
 CHECK(p.cells[8]==12);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,1,0,1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,1,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MS29 OUT sha256 3bb57cf5f2e8598b97a90153f232d4d74e61b1fca0d8657487a9dee542a01ffd
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="T";s.output=true;s.order.serial=6;s.order.fixed_pin=s.order.serial;s.order.product_terms=2;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==12);
 CHECK(p.cells[9]==8);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,4,4,-1,8,-1,-1,-1,12,12,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,4,4,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
}
