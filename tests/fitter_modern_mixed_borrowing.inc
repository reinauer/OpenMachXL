// Expected mixed product allocation for the saved test cases.
void check_mixed_borrowing(const MachDeviceLayout& layout) {
 { // MM0 OUT sha256 dc74f9c85b4402c180377c20c0bb6c7a20e8df5a07d82f3fa9e32074b1ec89f5
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MM1 OUT sha256 02bd0912cfd59928ed8df281bbb029a915daaca3d4e847461b13e8f4b6ae7b98
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MM2 OUT sha256 1961fdbc6a536252738b78d5db4e7fd7ef90e170be67686262933567ec80d2b4
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MM3 OUT sha256 5ade9752538d6e4a98c7500d1d781cb4df470285d4cc99e095099d82dd9a4d68
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
 { // MM4 OUT sha256 eabb8c1fdd48ca430380b39e519b80d0193fa6230eefbca2ce32b9c404bdbcc3
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MM5 OUT sha256 c5cb585b9e0e16e0a0dc59e28ad9075c8702bdd75b9ccca051d7c0c4bbf7ba90
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MM6 OUT sha256 1101bec32bd0ebee7dc5a1a155f58cd8962ed973f9b805105244e92bba40c2af
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
 { // MM7 OUT sha256 2c89705e90fb19daae7e9d5f5d251c2b5456e747d5bd65bfda72a76d4a811bd9
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,0,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MM8 OUT sha256 9e5caa57988dbb2613e59c65bf995ef882945a4380e85bdacbfb53897d82a937
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==1);
 CHECK(p.cells[7]==0);
 CHECK(p.cells[8]==12);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MM9 OUT sha256 758ddedd463e572bf506d160d1848459a15d5d8eae3658f71a3c1b9175dd1f8a
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=6;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=6;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=6;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==1);
 CHECK(p.cells[7]==0);
 CHECK(p.cells[8]==12);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,1,0,1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MM10 OUT sha256 9165f5797ca9bfca587113a9c463af93bb6e34147aefeeb6bccd798a4e5aa46a
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=false;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==12);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // ML1 OUT sha256 46e096bd04f17cdc353cf1fe9490442bf64ed408b44e2540ef91000d955e44e7
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // ML2 OUT sha256 3d223c07e523a16a7dae8b3ab3c0052fc40ac521c4a87ff57b55ef686f424d74
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=2;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // ML3 OUT sha256 f13e074ad0d38d907ffda9132b7e8c2024809a14a9747d9c2b4dba187a7f51be
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=3;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // ML4 OUT sha256 5f05ae326dfce48e490b1996a91112d93c2a6b77fa695db24a460f9aa45b96dd
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // ML5 OUT sha256 917f5748115d584c76fc56152dcb36489717fd6b8dbc89491ff0d1ee0222b6d4
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // ML6 OUT sha256 cfa5f33ccd6d1c7521215f0d5ce67abb998823f9267527b2e923f5a06b23bcf4
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=6;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // ML8 OUT sha256 9729d65d65039f26c69d416381ee3115a02bb644b794f305fa85a273f45c00b1
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // ML10 OUT sha256 a2c6dcfc6c49d2c9d4bcea7b3841f502e7786fe561ae22e4695c1590b5de2e6c
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // ML12 OUT sha256 1ffe0ae421c9885ffbd4521cb9613530283eb5bd3089d1246e534238be45c35c
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
}
