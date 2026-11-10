// Expected asynchronous product allocation for the saved test cases.
void check_async_borrowing(const MachDeviceLayout& layout) {
 { // MA0 OUT sha256 94b67d7a295b1378ed918021a392eebab870ecb3803ce7eb2bfbd45d43aad50c
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MA1 OUT sha256 ede40ef0466ef632e8aab77c51cc38505b89c63608c72d105dbc20edc6047f3f
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MA2 OUT sha256 bfa5e5a438d179eae60072accf0c9bf2610d143cd1c15e4eac740158a2e2ed01
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MA3 OUT sha256 db77fff15764e9d9b9b0436ce5297a1d0d0658418df641488515376b6a002c61
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MA4 OUT sha256 94e20a85fd3152481d0dca492de13ddafc2b70415c2866968670b890fff65cfe
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=8;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=4;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MA5 OUT sha256 22e3b1145e204d65147039a75b3494e63e579da3d41547734d145b465d156dae
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=12;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 }
 { // MA6 OUT sha256 7d70028eeba248acd8b5a5611f0dafeae1816e7400fbcab16f472b198a0d2418
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
 { // MA7 OUT sha256 bbf56efaa399b648209a1bca25b94692aa6ac77afff1fd552180d728ba75f24c
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=15;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==12);
 CHECK(p.cells[7]==0);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,12,12,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,12,-1,-1}));
 }
 { // MA8 OUT sha256 cf0c24b3a2a32d26dbe726cd315d5e787fd04225301500e072407a703a3f5b29
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==1);
 CHECK(p.cells[7]==0);
 CHECK(p.cells[8]==12);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,-1,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1}));
 }
 { // MA9 OUT sha256 b18dafec79bdb8c59d2eb2061561fc4e0297569538b967285ba266c9ebe85b08
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=6;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=6;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=6;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==1);
 CHECK(p.cells[7]==0);
 CHECK(p.cells[8]==12);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,1,0,1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
 { // MA10 OUT sha256 4624f6b8635298e29cf7fdfa3c4c99df61305e4fcdcbbe3cba54f0f033ddd4b8
 std::vector<Mach111PlaceSignal> signals;
 { Mach111PlaceSignal s;s.name="A0";s.order.type=1;s.order.serial=11;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A1";s.order.type=1;s.order.serial=12;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A2";s.order.type=1;s.order.serial=13;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A3";s.order.type=1;s.order.serial=14;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A4";s.order.type=1;s.order.serial=15;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="A5";s.order.type=1;s.order.serial=16;s.order.fixed_pin=s.order.serial;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="Q";s.output=true;s.order.serial=7;s.order.fixed_pin=s.order.serial;s.order.product_terms=10;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="R";s.output=true;s.order.serial=9;s.order.fixed_pin=s.order.serial;s.order.product_terms=5;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 { Mach111PlaceSignal s;s.name="S";s.output=true;s.order.serial=8;s.order.fixed_pin=s.order.serial;s.order.product_terms=1;s.fanins={11,12,13,14,15,16};s.local_clock=true;signals.push_back(s); }
 const auto p=place_mach_modern(layout,signals);
 CHECK(p.cells[6]==0);
 CHECK(p.cells[7]==4);
 CHECK(p.cells[8]==12);
 CHECK((std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==std::vector<int>{0,0,0,-1,4,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}));
 CHECK((std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==std::vector<int>{0,0,-1,-1,4,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1}));
 }
}
