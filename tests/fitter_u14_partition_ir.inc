// Original U14 partition IR; SHA256 45b7ab259cfffad4ef80c7236ca81d715e6cdad0bd5e8967f44f86b0f985392b
Clusters u14_partition_ir() {
    Clusters result;
    result.push_back({{2621,64,6},{28,31,37,40,41,78,79,83,85,92,93,97},{},true,0,1}); // LDOE
    result.push_back({{2601,72,17},{20,37,79,80,83,84,87,89,90,91},{},true,0,1}); // NEXT_N
    result.push_back({{2601,75,30},{5,7,8,9,10,78,79,80,81,96},{},true,1,1}); // AVEC_N
    result.push_back({{2601,87,19},{28,37,79,80,81,83,84,89,90,91},{},true,0,1}); // LCK01
    result.push_back({{2592,66,24},{78,80,81,85,89,90,91,94,96},{},true,1,2}); // ADONE_N
    result.push_back({{2591,82,1},{28,37,79,80,81,83,89,90,91},{},true,0,1}); // LCK23
    result.push_back({{2581,36,26},{14,16,27,28,82,83,95,97},{},true,1,1}); // DS_N
    result.push_back({{2561,25,23},{5,7,8,9,10,27},{},true,1,1}); // FC1
    result.push_back({{2561,25,29},{5,7,8,9,10,27},{},true,1,1}); // FC2
    result.push_back({{2561,25,36},{5,7,8,9,10,27},{},true,1,1}); // FC0
    result.push_back({{2561,28,39},{14,16,27,82,83,97},{},true,1,1}); // AS_N
    result.push_back({{2551,56,3},{28,78,79,80,81},{},true,0,1}); // ADOE
    result.push_back({{2541,37,25},{78,81,89,91},{},true,1,1}); // LSTB_N
    result.push_back({{2511,1,13},{2},{},true,0,1}); // CLKN
    result.push_back({{2511,1,15},{34},{},true,0,1}); // RSTOUT_N
    result.push_back({{81,68,80},{4,28,42,78,79,80,81,96},{},false,-1,1}); // TF
    result.push_back({{81,68,81},{4,28,42,78,79,80,81,96},{},false,-1,1}); // TL
    result.push_back({{71,54,91},{37,38,80,81,89,90,91},{},false,-1,1}); // FIN
    result.push_back({{71,60,82},{42,78,79,80,81,88,96},{},false,-1,1}); // SS
    result.push_back({{61,59,79},{42,78,79,80,81,96},{},false,-1,1}); // TM
    result.push_back({{51,26,84},{40,41,83,84,97},{},false,-1,1}); // P16
    result.push_back({{51,37,85},{20,37,79,83,84},{},false,-1,1}); // DN1
    result.push_back({{51,37,86},{20,37,79,83,84},{},false,-1,1}); // NS2
    result.push_back({{51,51,89},{78,80,81,89,90},{},false,-1,1}); // W1
    result.push_back({{41,21,83},{40,41,83,97},{},false,-1,1}); // ACK
    result.push_back({{41,44,90},{78,80,81,90},{},false,-1,1}); // W0
    result.push_back({{21,5,78},{14,77},{},false,-1,1}); // AR2
    result.push_back({{21,6,77},{14,31},{},false,-1,1}); // AR1
    result.push_back({{11,1,87},{86},{},false,-1,1}); // NS3
    result.push_back({{11,2,88},{87},{},false,-1,1}); // NS4
    return result;
}
