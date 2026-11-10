// Expected asynchronous product allocation under high resource pressure.
void check_async_stress(const MachDeviceLayout& layout) {
 { // MX0 OUT sha256 273c7b1d5b2f5448c34119db4a794fd58f1cab74bd60916b1e02525ec1c827e0
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,12,12,12,12,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MX1 OUT sha256 b5c20c1db8daeda26ea693aa9c80780db254f9ce1f7ed77b3589075a303cc1fe
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=2;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==12);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,4,4,4,4,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MX2 OUT sha256 0f48fd326d63a99df596504aab3f966c600f7f9ad6ee00119a94747891ce66b6
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="T";s.output=true;s.order.serial=6;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==1);
 CHECK(p.cells[7]==0);
 CHECK(p.cells[8]==12);
 CHECK(p.cells[9]==8);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,1,-1,-1,-1,8,8,8,8,-1,12,12,12,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,-1,1,-1,-1,-1,-1,8,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MX3 OUT sha256 f887ef3c1e96b04208f3059b9418820720028195f45a55404a4fb46fb2c67fc6
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,4,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MX4 OUT sha256 579a17488dc25e37ffb5ddb43011fa784b63beccb4eb5308d41fa3cf8c853d31
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==12);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,4,4,-1,-1,-1,-1,-1,12,12,12,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MX5 OUT sha256 a596f11e2dfaded2f1b86cd7900b26ec42014cba42b93e67eace1835739f2a02
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=9;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="T";s.output=true;s.order.serial=6;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==12);
 CHECK(p.cells[9]==8);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,4,4,4,4,-1,8,8,8,-1,12,12,12,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,-1,-1,-1,8,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MX6 OUT sha256 7d70028eeba248acd8b5a5611f0dafeae1816e7400fbcab16f472b198a0d2418
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,4,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MX7 OUT sha256 23b32a6a7f9c59ec5ad77c4670e49616b40f968ebb5bbc369b0cf3b8020e350b
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==12);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,12,12,12,12,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,-1,-1,-1,-1,-1,-1,12,12,12,-1,-1}));
 }
 { // MX8 OUT sha256 f817710162ecfc5338e75ab36b59ef8810233c3eeb3f889694ff3c6e326a23a0
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="T";s.output=true;s.order.serial=6;s.order.fixed_pin=s.order.serial;s.order.product_terms=2;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==0);
 CHECK(p.cells[9]==8);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,4,4,-1,-1,8,-1,-1,-1,12,12,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,12,12,-1,-1}));
 }
 { // MX9 OUT sha256 fef624b27904bc18677702ff9b30f4b97f370875529cb2243bdf8aa3ac9b3a5f
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,-1,4,4,4,4,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MX10 OUT sha256 5101abab52a3b553bdf56868617de9d6bbaa5746a8482a6ffb3b14c223557f08
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=9;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==0);
 CHECK(p.cells[8]==1);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,1,-1,-1,-1,-1,-1,-1,-1,12,12,12,12,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,12,-1}));
 }
 { // MX11 OUT sha256 c2b3de565bd46b99dc1075383aaaaf2f2b1f7450953f216bf5801cd38a03c088
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="T";s.output=true;s.order.serial=6;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==0);
 CHECK(p.cells[9]==8);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,4,4,-1,8,8,8,8,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,0,-1,4,-1,-1,8,8,8,-1,-1,12,-1,-1,-1}));
 }
 { // MX12 OUT sha256 9604dea2d8d725e63fdea22d46dd29868b37a83fc73b7e7fb0bc4d679694f949
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MX13 OUT sha256 f1bf9898f7a4e46bbca27827524df7c2fe4675821432b45263434ce71822b1ec
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=2;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==12);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,12,12,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{-1,-1,-1,-1,4,-1,-1,-1,-1,-1,-1,12,12,12,-1,-1}));
 }
 { // MX14 OUT sha256 31ce35f7868e8155f7cecac55ed184d3d9ebbb305c7e19ccb5c8f5d9e704672d
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=9;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="T";s.output=true;s.order.serial=6;s.order.fixed_pin=s.order.serial;s.order.product_terms=9;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==0);
 CHECK(p.cells[9]==8);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,4,4,-1,-1,8,8,8,-1,12,12,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,4,-1,-1,-1,8,-1,-1,-1,12,12,-1,-1}));
 }
}
