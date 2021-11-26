#!/usr/bin/python3
#
# http://niviuk.free.fr/lte_band.php
#
# This decodes a copy/paste of the table in the above web page and translates it into
# a constant initialiser suitable for earfcn.h. Make sure to paste the table header as well,
# starting at "Band"
#
# Iwo
Table="""
Band    Name    Downlink (MHz)  Bandwidth
DL/UL (MHz)     Uplink (MHz)    Duplex spacing
(MHz)   Geographical
area    3GPP
release
Low     Middle  High    Low     Middle  High
Earfcn  Earfcn
1       2100    2110
0       2140
300     2170
599     60      1920
18000   1950
18300   1980
18599   190     Global  8
2       1900 PCS        1930
600     1960
900     1990
1199    60      1850
18600   1880
18900   1910
19199   80      NAR     8
3       1800+   1805
1200    1842.5
1575    1880
1949    75      1710
19200   1747.5
19575   1785
19949   95      Global  8
4       AWS-1   2110
1950    2132.5
2175    2155
2399    45      1710
19950   1732.5
20175   1755
20399   400     NAR     8
5       850     869
2400    881.5
2525    894
2649    25      824
20400   836.5
20525   849
20649   45      NAR     8
6       UMTS only       875
2650    880
2700    885
2749    10      830
20650   835
20700   840
20749   45      APAC    8
7       2600    2620
2750    2655
3100    2690
3449    70      2500
20750   2535
21100   2570
21449   120     EMEA    8
8       900 GSM         925
3450    942.5
3625    960
3799    35      880
21450   897.5
21625   915
21799   45      Global  8
9       1800    1844.9
3800    1862.5
3975    1879.9
4149    35      1749.9
21800   1767.5
21975   1784.9
22149   95      APAC    8
10      AWS-1+  2110
4150    2140
4450    2170
4749    60      1710
22150   1740
22450   1770
22749   400     NAR     8
11      1500 Lower      1475.9
4750    1486
4850    1495.9
4949    20      1427.9
22750   1438
22850   1447.9
22949   48      Japan   8
12      700 a   729
5010    737.5
5095    746
5179    17      699
23010   707.5
23095   716
23179   30      NAR     8.4
13      700 c   746
5180    751
5230    756
5279    10      777
23180   782
23230   787
23279   -31     NAR     8
14      700 PS  758
5280    763
5330    768
5379    10      788
23280   793
23330   798
23379   -30     NAR     8
17      700 b   734
5730    740
5790    746
5849    12      704
23730   710
23790   716
23849   30      NAR     8.3
18      800 Lower       860
5850    867.5
5925    875
5999    15      815
23850   822.5
23925   830
23999   45      Japan   9
19      800 Upper       875
6000    882.5
6075    890
6149    15      830
24000   837.5
24075   845
24149   45      Japan   9
20      800 DD  791
6150    806
6300    821
6449    30      832
24150   847
24300   862
24449   -41     EMEA    9
21      1500 Upper      1495.9
6450    1503.5
6525    1510.9
6599    15      1447.9
24450   1455.5
24525   1462.9
24599   48      Japan   9
22      3500    3510
6600    3550
7000    3590
7399    80      3410
24600   3450
25000   3490
25399   100     EMEA    10.4
23      2000 S-band     2180
7500    2190
7600    2200
7699    20      2000
25500   2010
25600   2020
25699   180     NAR     10.3
24      1600 L-band     1525
7700    1542
7870    1559
8039    34      1626.5
25700   1643.5
25870   1660.5
26039   -101.5  NAR     10.1
25      1900+   1930
8040    1962.5
8365    1995
8689    65      1850
26040   1882.5
26365   1915
26689   80      NAR     10
26      850+    859
8690    876.5
8865    894
9039    35      814
26690   831.5
26865   849
27039   45      NAR     11.0
27      800 SMR         852
9040    860.5
9125    869
9209    17      807
27040   815.5
27125   824
27209   45      NAR     11.1
28      700 APT         758
9210    780.5
9435    803
9659    45      703
27210   725.5
27435   748
27659   55      APAC,EU         11.1
29      700 d   717
9660    722.5
9715    728
9769    11      Downlink only   NAR     11.3
30      2300 WCS        2350
9770    2355
9820    2360
9869    10      2305
27660   2310
27710   2315
27759   45      NAR     12.0
31      450     462.5
9870    465
9895    467.5
9919    5       452.5
27760   455
27785   457.5
27809   10      Global  12.0
32      1500 L-band     1452
9920    1474
10140   1496
10359   44      Downlink only   EMEA    12.4
65      2100+   2110
65536   2155
65986   2200
66435   90      1920
131072  1965
131522  2010
131971  190     Global  13.2
66      AWS-3   2110
66436   2155
66886   2200
67335   90 / 70         1710
131972  1745
132322  1780
132671  400     NAR     13.2
67      700 EU  738
67336   748
67436   758
67535   20      Downlink only   EMEA    13.2
68      700 ME  753
67536   768
67686   783
67835   30      698
132672  713
132822  728
132971  55      EMEA    13.3
69      DL 2500         2570
67836   2595
68086   2620
68335   50      Downlink only           14.0
70      AWS-4   1995
68336   2007.5
68461   2020
68585   25 / 15         1695
132972  1702.5
133047  1710
133121  300     NAR     14.0
71      600     617
68586   634.5
68761   652
68935   35      663
133122  680.5
133297  698
133471  -46     NAR     15.0
72      450 PMR/PAMR    461
68936   463.5
68961   466
68985   5       451
133472  453.5
133497  456
133521  10      EMEA    15.0
73      450 APAC        460
68986   462.5
69011   465
69035   5       450
133522  452.5
133547  455
133571  10      APAC    15.0
74      L-band  1475
69036   1496.5
69251   1518
69465   43      1427
133572  1448.5
133787  1470
134001  48      NAR     15.0
75      DL 1500+        1432
69466   1474.5
69891   1517
70315   85      Downlink only   NAR     15.0
76      DL 1500-        1427
70316   1429.5
70341   1432
70365   5       Downlink only   NAR     15.0
85      700 a+  728
70366   737
70456   746
70545   18      698
134002  707
134092  716
134181  30      NAR     15.2
87      410     420
70546   422.5
70571   425
70595   5       410
134182  412.5
134207  415
134231  10      EMEA    16.2
88      410+    422
70596   424.5
70621   427
70645   5       412
134232  414.5
134257  417
134281  10      EMEA    16.2
252     Unlicensed NII-1        5150
255144  5200
255644  5250
256143  100     Downlink only   Global  LTE-U
255     Unlicensed NII-3        5725
260894  5787.5
261519  5850
262143  125     Downlink only   Global  LTE-U
        TDD
33      TD 1900         1900
36000   1910
36100   1920
36199   20              EMEA    8
34      TD 2000         2010
36200   2017.5
36275   2025
36349   15              EMEA    8
35      TD PCS Lower    1850
36350   1880
36650   1910
36949   60              NAR     8
36      TD PCS Upper    1930
36950   1960
37250   1990
37549   60              NAR     8
37      TD PCS Center gap       1910
37550   1920
37650   1930
37749   20              NAR     8
38      TD 2600         2570
37750   2595
38000   2620
38249   50              EMEA    8
39      TD 1900+        1880
38250   1900
38450   1920
38649   40              China   8
40      TD 2300         2300
38650   2350
39150   2400
39649   100             China   8
41      TD 2600+        2496
39650   2593
40620   2690
41589   194             Global  10
42      TD 3500         3400
41590   3500
42590   3600
43589   200                     10
43      TD 3700         3600
43590   3700
44590   3800
45589   200                     10
44      TD 700  703
45590   753
46090   803
46589   100             APAC    11.1
45      TD 1500         1447
46590   1457
46690   1467
46789   20              China   13.2
46      TD Unlicensed   5150
46790   5537.5
50665   5925
54539   775             Global  13.2
47      TD V2X  5855
54540   5890
54890   5925
55239   70              Global  14.1
48      TD 3600         3550
55240   3625
55990   3700
56739   150             Global  14.2
49      TD 3600r        3550
56740   3625
57490   3700
58239   150             Global  15.1
50      TD 1500+        1432
58240   1474.5
58665   1517
59089   85                      15.0
51      TD 1500-        1427
59090   1429.5
59115   1432
59139   5                       15.0
52      TD 3300         3300
59140   3350
59640   3400
60139   100                     15.2
53      TD 2500         2483.5
60140   2489.5
60197   2495
60254   11.5                    16.0
"""

import re
reldata = []
"""
Normal FDD bands
1       2100+   2110      # Band, Name w space, FLowDL
0       2140              # DLearfcn_low, FMidDL
300     2170              # DLearfcn_mid, FHighDL
599     60      1920      # DLearfcn_high, DL/UL BW, FlowUL
18000   1950              # ULearfcn_low, FMidUL
18300   1980              # ULearfcn_mid, FHighUL
18599   190     Global  8 # ULearfcn)high, +-duplex spacing, geo string, 3gpp rel N.M|LTE-U
"""
reg_fdd = re.compile(r"""^([0-9]{1,3}) +(.+) +([0-9.]{3,}) *
([0-9]+) +([0-9.]{3,}) *
([0-9]+) +([0-9.]{3,}) *
([0-9]+) +(.+) +([0-9.]{3,}) *
([0-9]+) +([0-9.]{3,}) *
([0-9]+) +([0-9.]{3,}) *
([0-9]+) +([0-9.-]+) +(.+) +([0-9.]+) *
""", re.MULTILINE)
fdd_matches = reg_fdd.findall(Table)
for i in fdd_matches:
    print("FDD",i)
    A={"type":"FDD", "band":int(i[0]), "name":i[1].strip(),
       "DLF":float(i[2]), "DLno":int(i[3]), "DLnr":int(i[7])-int(i[3])+1,
       "ULF":float(i[9]), "ULno":int(i[10]), "ULnr":int(i[14])-int(i[10])+1,
       "geo":i[16].strip()}
    reldata.append(A)
"""
TDD
51      TD 1500-        1427            # Band, Name w space, FLowDL
59090   1429.5                          # earfcn_low, FMid
59115   1432                            # earfcn_mid, FHigh
59139   5                       15.0    # earfcn_high, BW, geo string, 3gpp rel N.M|LTE-U
40      TD 2300         2300
38650   2350
39150   2400
39649   100             China   8
"""
reg_tdd = re.compile(r"""^([0-9]{1,3}) +(.+) +([0-9.]{3,}) *
([0-9]+) +([0-9.]{3,}) *
([0-9]+) +([0-9.]{3,}) *
([0-9]+) +([0-9.-]+) {10,}(.+) +([0-9.]+) *
""", re.MULTILINE)
tdd_matches = reg_tdd.findall(Table)
for i in tdd_matches:
    print("TDD",i)
    A={"type":"TDD", "band":int(i[0]), "name":i[1].strip(),
       "DLF":float(i[2]), "DLno":int(i[3]), "DLnr":int(i[7])-int(i[3])+1,
       "ULF":float(i[2]), "ULno":int(i[3]), "ULnr":int(i[7])-int(i[3])+1,  # UL same as DL
       "geo":i[9].strip()}
    reldata.append(A)

"""
FDD downlink only
29      700 d   717                             # Band, Name w space, FLowDL
9660    722.5                                   # DLearfcn_low, FMidDL
9715    728                                     # DLearfcn_mid, FHighDL
9769    11      Downlink only   NAR     11.3    # DLearfcn_high, DL BW, Downlink only, geo string, 3gpp rel N.M|LTE-U
"""
reg_dl = re.compile(r"""^([0-9]{1,3}) +(.+) +([0-9.]{3,}) *
([0-9]+) +([0-9.]{3,}) *
([0-9]+) +([0-9.]{3,}) *
([0-9]+) +(.+) +(Downlink only) +(.+) +([0-9.]+|LTE-U) *
""", re.MULTILINE)
dl_matches = reg_dl.findall(Table)
for i in dl_matches:
    print("FDL",i)
    A={"type":"FDL", "band":int(i[0]), "name":i[1].strip(),
       "DLF":float(i[2]), "DLno":int(i[3]), "DLnr":int(i[7])-int(i[3])+1,
       "ULF":0.0, "ULno":-1, "ULnr":0,  # No UL
       "geo":i[10].strip()}
    reldata.append(A)

popular_bands=(
    1,  # AU
    2,  # USA Canada
    3,  # AU
    4,  # USA Canada
    5,  # AU USA Canada
    7,  # AU Canada
    12, # USA Canada
    13, # Canada
    14, # USA
    17, # USA Canada
    25, # USA
    26, # USA
    28, # AU
    29, # Canada
    30, # USA
    38, # Canada
    40, # AU Canada
    41, # USA
    42, # Canada
    46, # USA
    66, # Canada
)

reldata=sorted(reldata,key=lambda k: k['band']) # Sort by band
popular=[i for i in reldata if i['band'] in popular_bands] # Extract the popular ones
unpopular=[i for i in reldata if not i['band'] in popular_bands] # Extract the unpopular ones
reldata=popular + unpopular # Bolt them together

# presorting band list by popular ones first

for i in reldata:
    print(i)

# Write into bands file

with open("bands_lte.h","w") as f:
    f.write("/* Automatically generated by bands.py, do not edit here */\n")
    f.write("/* Band(int), Name(char*), DLFreq(float), DLearfcn offset(int), DLearfcn width(int), Same for UL */\n")
    f.write("/* Downlink only bands have ULearfcn offset set to -1. TDD have same values in UL and DL */\n")
    f.write("const static struct lte_band lte_bands[] = {\n")
    for i in reldata:
        l='    {%d, "%s", %.1f, %d, %d, %.1f, %d, %d},\n' % (i['band'],i['name'],
                                                       i['DLF'],i['DLno'],i['DLnr'],
                                                       i['ULF'],i['ULno'],i['ULnr'])
        f.write(l)
    f.write("};\n")

# TODO: check for earfcn overlaps
