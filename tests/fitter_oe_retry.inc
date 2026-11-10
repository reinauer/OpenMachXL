// Original PLC maps and enable groups, including spread-retry fallback.
void check_split_enable_retry() {
 { // OSPLIT3 PLCsha256 a7b7880bc8278bd0f8da0107109ecbd86a283387727a7d84c6e613b05dc4ce1a
 std::vector<Mach111PlaceSignal> s={input("A",9,1),input("B",31,1),input("E",10,1),input("F",12,1),input("G",32,1)};
 for(auto& x:s)x.order.fixed_pin=x.order.serial;
 { auto x=output("P0",77,{9,10});x.order.block=0;x.oe_product="E";x.oe_members=7;s.push_back(x); }
 { auto x=output("P1",78,{31,10});x.order.block=0;x.oe_product="E";x.oe_members=7;s.push_back(x); }
 { auto x=output("P2",79,{9,10});x.order.block=0;x.oe_product="E";x.oe_members=7;s.push_back(x); }
 { auto x=output("P3",80,{31,10});x.order.block=0;x.oe_product="E";x.oe_members=7;s.push_back(x); }
 { auto x=output("P4",81,{9,10});x.order.block=0;x.oe_product="E";x.oe_members=7;s.push_back(x); }
 { auto x=output("P5",82,{31,10});x.order.block=0;x.oe_product="E";x.oe_members=7;s.push_back(x); }
 { auto x=output("P6",83,{9,10});x.order.block=0;x.oe_product="E";x.oe_members=7;s.push_back(x); }
 { auto x=output("P7",84,{31,12});x.order.block=0;x.oe_product="F";x.oe_members=5;s.push_back(x); }
 { auto x=output("P8",85,{9,12});x.order.block=0;x.oe_product="F";x.oe_members=5;s.push_back(x); }
 { auto x=output("P9",86,{31,12});x.order.block=0;x.oe_product="F";x.oe_members=5;s.push_back(x); }
 { auto x=output("P10",87,{9,12});x.order.block=0;x.oe_product="F";x.oe_members=5;s.push_back(x); }
 { auto x=output("P11",88,{31,12});x.order.block=0;x.oe_product="F";x.oe_members=5;s.push_back(x); }
 { auto x=output("P12",89,{9,32});x.order.block=0;x.oe_product="G";x.oe_members=4;s.push_back(x); }
 { auto x=output("P13",90,{31,32});x.order.block=0;x.oe_product="G";x.oe_members=4;s.push_back(x); }
 { auto x=output("P14",91,{9,32});x.order.block=0;x.oe_product="G";x.oe_members=4;s.push_back(x); }
 { auto x=output("P15",92,{31,32});x.order.block=0;x.oe_product="G";x.oe_members=4;s.push_back(x); }
 const auto placed=place_mach111(s);
 CHECK(placed.pins[5]==2);
 CHECK(placed.pins[6]==5);
 CHECK(placed.pins[7]==8);
 CHECK(placed.pins[8]==15);
 CHECK(placed.pins[9]==18);
 CHECK(placed.pins[10]==21);
 CHECK(placed.pins[11]==3);
 CHECK(placed.pins[12]==17);
 CHECK(placed.pins[13]==19);
 CHECK(placed.pins[14]==20);
 CHECK(placed.pins[15]==16);
 CHECK(placed.pins[16]==14);
 CHECK(placed.pins[17]==4);
 CHECK(placed.pins[18]==6);
 CHECK(placed.pins[19]==7);
 CHECK(placed.pins[20]==9);
 }
 { // OSPLIT32 PLCsha256 b012cd57a7a7f8c4b2a1c6c4307870f9dc02b8e0ffc8fedb5738bb48a32cc6b8
 std::vector<Mach111PlaceSignal> s={input("A",9,1),input("B",31,1),input("E",10,1),input("F",12,1),input("G",32,1)};
 for(auto& x:s)x.order.fixed_pin=x.order.serial;
 { auto x=output("P0",77,{9,10});x.order.block=0;x.oe_product="E";x.oe_members=9;s.push_back(x); }
 { auto x=output("P1",78,{31,10});x.order.block=0;x.oe_product="E";x.oe_members=9;s.push_back(x); }
 { auto x=output("P2",79,{9,10});x.order.block=0;x.oe_product="E";x.oe_members=9;s.push_back(x); }
 { auto x=output("P3",80,{31,10});x.order.block=0;x.oe_product="E";x.oe_members=9;s.push_back(x); }
 { auto x=output("P4",81,{9,10});x.order.block=0;x.oe_product="E";x.oe_members=9;s.push_back(x); }
 { auto x=output("P5",82,{31,10});x.order.block=0;x.oe_product="E";x.oe_members=9;s.push_back(x); }
 { auto x=output("P6",83,{9,10});x.order.block=0;x.oe_product="E";x.oe_members=9;s.push_back(x); }
 { auto x=output("P7",84,{31,10});x.order.block=0;x.oe_product="E";x.oe_members=9;s.push_back(x); }
 { auto x=output("P8",85,{9,10});x.order.block=0;x.oe_product="E";x.oe_members=9;s.push_back(x); }
 { auto x=output("P9",86,{31,12});x.order.block=0;x.oe_product="F";x.oe_members=3;s.push_back(x); }
 { auto x=output("P10",87,{9,12});x.order.block=0;x.oe_product="F";x.oe_members=3;s.push_back(x); }
 { auto x=output("P11",88,{31,12});x.order.block=0;x.oe_product="F";x.oe_members=3;s.push_back(x); }
 { auto x=output("P12",89,{9,32});x.order.block=0;x.oe_product="G";x.oe_members=3;s.push_back(x); }
 { auto x=output("P13",90,{31,32});x.order.block=0;x.oe_product="G";x.oe_members=3;s.push_back(x); }
 { auto x=output("P14",91,{9,32});x.order.block=0;x.oe_product="G";x.oe_members=3;s.push_back(x); }
 const auto placed=place_mach111(s);
 CHECK(placed.pins[5]==2);
 CHECK(placed.pins[6]==5);
 CHECK(placed.pins[7]==8);
 CHECK(placed.pins[8]==15);
 CHECK(placed.pins[9]==18);
 CHECK(placed.pins[10]==21);
 CHECK(placed.pins[11]==3);
 CHECK(placed.pins[12]==6);
 CHECK(placed.pins[13]==16);
 CHECK(placed.pins[14]==9);
 CHECK(placed.pins[15]==4);
 CHECK(placed.pins[16]==7);
 CHECK(placed.pins[17]==14);
 CHECK(placed.pins[18]==17);
 CHECK(placed.pins[19]==19);
 }
}
