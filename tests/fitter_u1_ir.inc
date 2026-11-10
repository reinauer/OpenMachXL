// U1 fitter input from the saved EQN2.PLA reference.
// SHA256 e48457951833990e81a3a9be1fe8ce089050a2ad17c4da7f2a059ab03b2c7c6b
// This fixture is used by tests only.
std::vector<Mach111PlaceSignal> u1_placement_ir() {
    std::vector<Mach111PlaceSignal> result;
    { Mach111PlaceSignal s;
      s.name="DISABLE_N";s.order.serial=17;s.order.type=5;
      s.output=true;s.order.product_terms=2;s.fanins={2,5,6,8,9,10,12,16,17,23,28,31,34,35,36,37,38,40};
      s.order.fixed_pin=17;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="Z2SEL_N";s.order.serial=18;s.order.type=5;
      s.output=true;s.order.product_terms=2;s.fanins={5,8,9,10,12,17,28,30,31,32,34,35,36,37,38,40,77};
      s.order.fixed_pin=18;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="WE_N";s.order.serial=27;s.order.type=0;
      s.output=true;s.order.product_terms=2;s.fanins={5,6,8,9,10,12,25,28,32,34,35,36,37,38,40,78};
      s.order.fixed_pin=27;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="SELRAM0_N";s.order.serial=42;s.order.type=5;
      s.output=true;s.order.product_terms=2;s.fanins={5,6,8,9,10,12,28,32,34,35,36,37,38,40,78};
      s.order.fixed_pin=42;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="F0SEL_N";s.order.serial=3;s.order.type=5;
      s.output=true;s.order.product_terms=1;s.fanins={5,8,9,10,12,17,28,32,34,35,36,37,38,40};
      s.order.fixed_pin=3;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="F0SEL2_N";s.order.serial=39;s.order.type=0;
      s.output=true;s.order.product_terms=1;s.fanins={5,8,9,10,12,17,28,32,34,35,36,37,38,40};
      s.order.fixed_pin=39;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="TCI_N";s.order.serial=26;s.order.type=0;
      s.output=true;s.order.product_terms=5;s.fanins={5,8,9,10,12,28,32,34,35,36,37,38,40};
      s.order.fixed_pin=26;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="AMIGA_N";s.order.serial=13;s.order.type=0;
      s.output=true;s.order.product_terms=1;s.fanins={3,5,8,9,10,12,18,28,38,40,42};
      s.order.fixed_pin=13;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="MA11";s.order.serial=7;s.order.type=0;
      s.output=true;s.order.product_terms=3;s.fanins={5,12,25,40,42};
      s.order.fixed_pin=7;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="SELRAM3_N";s.order.serial=20;s.order.type=0;
      s.output=true;s.order.product_terms=1;s.fanins={10,28,38,40};
      s.order.fixed_pin=20;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="SELRAM2_N";s.order.serial=29;s.order.type=0;
      s.output=true;s.order.product_terms=1;s.fanins={10,28,38,40};
      s.order.fixed_pin=29;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="MA8";s.order.serial=24;s.order.type=0;
      s.output=true;s.order.product_terms=2;s.fanins={25,31,32};
      s.order.fixed_pin=24;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="MA9";s.order.serial=4;s.order.type=0;
      s.output=true;s.order.product_terms=2;s.fanins={25,34,35};
      s.order.fixed_pin=4;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="MA10";s.order.serial=14;s.order.type=0;
      s.output=true;s.order.product_terms=2;s.fanins={25,36,37};
      s.order.fixed_pin=14;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="TBI_N";s.order.serial=19;s.order.type=0;
      s.output=true;s.order.product_terms=2;s.fanins={38,40,42};
      s.order.fixed_pin=19;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="RSTHI";s.order.serial=1;s.order.type=0;
      s.output=true;s.order.product_terms=1;s.fanins={41};
      s.order.fixed_pin=1;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="MAPROM";s.order.serial=78;s.order.type=3;
      s.output=true;s.order.product_terms=2;s.fanins={5,6,8,9,10,12,15,16,23,28,30,31,32,34,35,36,37,38,40,78};
      s.buried=true;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="CONFIG";s.order.serial=77;s.order.type=3;
      s.output=true;s.order.product_terms=2;s.fanins={5,6,8,9,10,12,17,23,28,30,31,32,34,35,36,37,38,40,41,77};
      s.buried=true;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="LONGRST";s.order.serial=2;s.order.type=1;
      s.order.fixed_pin=2;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="A24";s.order.serial=5;s.order.type=1;
      s.order.fixed_pin=5;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="R_W";s.order.serial=6;s.order.type=1;
      s.order.fixed_pin=6;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="A29";s.order.serial=8;s.order.type=1;
      s.order.fixed_pin=8;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="A28";s.order.serial=9;s.order.type=1;
      s.order.fixed_pin=9;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="A26";s.order.serial=10;s.order.type=1;
      s.order.fixed_pin=10;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="A25";s.order.serial=12;s.order.type=1;
      s.order.fixed_pin=12;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="MAPJP_N";s.order.serial=15;s.order.type=1;
      s.order.fixed_pin=15;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="PWRON_N";s.order.serial=16;s.order.type=1;
      s.order.fixed_pin=16;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="LSTB_N";s.order.serial=23;s.order.type=1;
      s.order.fixed_pin=23;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="MUX";s.order.serial=25;s.order.type=1;
      s.order.fixed_pin=25;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="A27";s.order.serial=28;s.order.type=1;
      s.order.fixed_pin=28;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="A17";s.order.serial=30;s.order.type=1;
      s.order.fixed_pin=30;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="A18";s.order.serial=31;s.order.type=1;
      s.order.fixed_pin=31;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="A19";s.order.serial=32;s.order.type=1;
      s.order.fixed_pin=32;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="A20";s.order.serial=34;s.order.type=1;
      s.order.fixed_pin=34;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="A21";s.order.serial=35;s.order.type=1;
      s.order.fixed_pin=35;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="A22";s.order.serial=36;s.order.type=1;
      s.order.fixed_pin=36;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="A23";s.order.serial=37;s.order.type=1;
      s.order.fixed_pin=37;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="A31";s.order.serial=38;s.order.type=1;
      s.order.fixed_pin=38;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="A30";s.order.serial=40;s.order.type=1;
      s.order.fixed_pin=40;
      result.push_back(s); }
    { Mach111PlaceSignal s;
      s.name="RST_N";s.order.serial=41;s.order.type=1;
      s.order.fixed_pin=41;
      result.push_back(s); }
    return result;
}
