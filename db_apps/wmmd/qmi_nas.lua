-----------------------------------------------------------------------------------------------------------------------
-- Copyright (C) 2015 NetComm Wireless limited.
--
-----------------------------------------------------------------------------------------------------------------------

-- qmi nas module, radio network information

local QmiNas = require("wmmd.Class"):new()

function QmiNas:setup(rdbWatch, wrdb, dConfig)
  -- init syslog
  self.l = require("luasyslog")
  pcall(function() self.l.open("qmi_nas", "LOG_DAEMON") end)

  self.rdb = require("luardb")
  self.rdbWatch = rdbWatch
  self.luaq = require("luaqmi")
  self.bit = require("bit")
  self.watcher = require("wmmd.watcher")
  self.smachine = require("wmmd.smachine")
  self.config = require("wmmd.config")
  self.wrdb = wrdb
  self.ffi = require("ffi")
  self.dconfig = dConfig
  self.t = require("turbo")
  self.util = require("wmmd.util")

  self.m = self.luaq.m
  self.e = self.luaq.e
  self.sq = self.smachine.get_smachine("qmi_smachine")

  -- hex codes for band category boundaries
  -- 00 to 39     -- CDMA band classes
  -- 40 to 79     -- GSM band classes
  -- 80 to 91     -- WCDMA band classes
  -- 120 to 174   -- LTE band classes
  -- 200 to 205   -- TD-SCDMA band classes
  -- 250 to 290   -- NR-5G active band classes
  -- 768 to 808   -- NR-5G NSA band classes
  -- 1536 to 1576 -- NR-5G SA band classes
  self.HEX_GSM_MIN      = 40
  self.HEX_WCDMA_MIN    = 80
  self.HEX_LTE_MIN      = 120
  self.HEX_TDSCDMA_MIN  = 200
  self.HEX_NR5G_MIN     = 250
  self.HEX_NR5G_NSA_MIN = 768
  self.HEX_NR5G_SA_MIN  = 1536

  -- hex codes for band groups
  self.HEX_GROUP_GSM       = 0x0400
  self.HEX_GROUP_WCDMA     = 0x0800
  self.HEX_GROUP_LTE       = 0x1000
  self.HEX_GROUP_NR5G_NSA  = 0x2000
  self.HEX_GROUP_NR5G_SA   = 0x4000
  self.HEX_GROUP_NR5G      = 0x6000


  -- max bands in hex group for NR-5G (512 bands)
  self.HEX_GROUP_NR5G_COUNT = 0x0200

  -- Combination groups
  self.HEX_GROUP_GSM_WCDMA      = self.bit.bor(self.HEX_GROUP_GSM, self.HEX_GROUP_WCDMA)
  self.HEX_GROUP_GSM_LTE        = self.bit.bor(self.HEX_GROUP_GSM, self.HEX_GROUP_LTE)
  self.HEX_GROUP_GSM_NR5G       = self.bit.bor(self.HEX_GROUP_GSM, self.HEX_GROUP_NR5G)
  self.HEX_GROUP_GSM_NR5G_NSA   = self.bit.bor(self.HEX_GROUP_GSM, self.HEX_GROUP_NR5G_NSA)
  self.HEX_GROUP_GSM_NR5G_SA    = self.bit.bor(self.HEX_GROUP_GSM, self.HEX_GROUP_NR5G_SA)

  self.HEX_GROUP_WCDMA_LTE      = self.bit.bor(self.HEX_GROUP_WCDMA, self.HEX_GROUP_LTE)
  self.HEX_GROUP_WCDMA_NR5G     = self.bit.bor(self.HEX_GROUP_WCDMA, self.HEX_GROUP_NR5G)
  self.HEX_GROUP_WCDMA_NR5G_NSA = self.bit.bor(self.HEX_GROUP_WCDMA, self.HEX_GROUP_NR5G_NSA)
  self.HEX_GROUP_WCDMA_NR5G_SA  = self.bit.bor(self.HEX_GROUP_WCDMA, self.HEX_GROUP_NR5G_SA)

  self.HEX_GROUP_LTE_NR5G       = self.bit.bor(self.HEX_GROUP_LTE, self.HEX_GROUP_NR5G)
  self.HEX_GROUP_LTE_NR5G_NSA   = self.bit.bor(self.HEX_GROUP_LTE, self.HEX_GROUP_NR5G_NSA)
  self.HEX_GROUP_LTE_NR5G_SA    = self.bit.bor(self.HEX_GROUP_LTE, self.HEX_GROUP_NR5G_SA)

  self.HEX_GROUP_GSM_WCDMA_LTE      = self.bit.bor(self.HEX_GROUP_GSM_WCDMA, self.HEX_GROUP_LTE)
  self.HEX_GROUP_GSM_WCDMA_NR5G     = self.bit.bor(self.HEX_GROUP_GSM_WCDMA, self.HEX_GROUP_NR5G)
  self.HEX_GROUP_GSM_WCDMA_NR5G_NSA = self.bit.bor(self.HEX_GROUP_GSM_WCDMA, self.HEX_GROUP_NR5G_NSA)
  self.HEX_GROUP_GSM_WCDMA_NR5G_SA  = self.bit.bor(self.HEX_GROUP_GSM_WCDMA, self.HEX_GROUP_NR5G_SA)

  self.HEX_GROUP_GSM_LTE_NR5G       = self.bit.bor(self.HEX_GROUP_GSM_LTE, self.HEX_GROUP_NR5G)
  self.HEX_GROUP_GSM_LTE_NR5G_NSA   = self.bit.bor(self.HEX_GROUP_GSM_LTE, self.HEX_GROUP_NR5G_NSA)
  self.HEX_GROUP_GSM_LTE_NR5G_SA    = self.bit.bor(self.HEX_GROUP_GSM_LTE, self.HEX_GROUP_NR5G_SA)
  self.HEX_GROUP_WCDMA_LTE_NR5G     = self.bit.bor(self.HEX_GROUP_WCDMA_LTE, self.HEX_GROUP_NR5G)
  self.HEX_GROUP_WCDMA_LTE_NR5G_NSA = self.bit.bor(self.HEX_GROUP_WCDMA_LTE, self.HEX_GROUP_NR5G_NSA)
  self.HEX_GROUP_WCDMA_LTE_NR5G_SA  = self.bit.bor(self.HEX_GROUP_WCDMA_LTE, self.HEX_GROUP_NR5G_SA)

  self.HEX_GROUP_ALL                = self.bit.bor(self.HEX_GROUP_GSM_WCDMA, self.HEX_GROUP_LTE_NR5G)

  --[[
    This is a list of band hex code to band name mapping.
    Band name customisation and band exclusion can be done here.
  --]]
  self.band_names={
    [0]="CDMA 800-MHz (BC 0)",
    [1]="CDMA 1900-MHz (BC 1)",
    [2]="CDMA TACS (BC 2)",
    [3]="CDMA JTACS (BC 3)",
    [4]="CDMA Korean PCS (BC 4)",
    [5]="CDMA 450-MHz (BC 5)",
    [6]="CDMA 2-GHz IMT2000 (BC 6)",
    [7]="CDMA Upper 700-MHz (BC 7)",
    [8]="CDMA 1800-MHz (BC 8)",
    [9]="CDMA 900-MHz (BC 9)",
    [10]="CDMA Secondary 800 MHz (BC 10)",
    [11]="CDMA 400 MHz European PAMR (BC 11)",
    [12]="CDMA 800 MHz PAMR (BC 12)",
    [13]="CDMA 2.5 GHz IMT-2000 Extension (BC 13)",
    [14]="CDMA US PCS 1.9GHz (BC 14)",
    [15]="CDMA AWS (BC 15)",
    [16]="CDMA US 2.5GHz (BC 16)",
    [17]="CDMA US 2.5GHz Forward Link Only (BC 17)",
    [18]="CDMA 700 MHz Public Safety (BC 18)",
    [19]="CDMA Lower 700-MHz (BC 19)",
    [20]="CDMA L-Band (BC 20)",
    [21]="CDMA S-Band (BC 21)",

    [40]="GSM 450",
    [41]="GSM 480",
    [42]="GSM 750",
    [43]="GSM 850",
    [44]="GSM 900 (Extended)",
    [45]="GSM 900 (Primary)",
    [46]="GSM 900 (Railways)",
    [47]="GSM 1800",
    [48]="GSM 1900",

    [80]="WCDMA 2100",
    [81]="WCDMA PCS 1900",
    [82]="WCDMA DCS 1800",
    [83]="WCDMA 1700 (US)",
    [84]="WCDMA 850",
    [85]="WCDMA 800",
    [86]="WCDMA 2600",
    [87]="WCDMA 900",
    [88]="WCDMA 1700 (Japan)",
    [90]="WCDMA 1500",
    [91]="WCDMA 850 (Japan)",

    [120]="LTE Band 1 - 2100MHz",
    [121]="LTE Band 2 - 1900MHz",
    [122]="LTE Band 3 - 1800MHz",
    [123]="LTE Band 4 - 1700MHz",
    [124]="LTE Band 5 - 850MHz",
    [125]="LTE Band 6 - 800MHz",
    [126]="LTE Band 7 - 2600MHz",
    [127]="LTE Band 8 - 900MHz",
    [128]="LTE Band 9 - 1700MHz",
    [129]="LTE Band 10 - 1700MHz",
    [130]="LTE Band 11 - 1500MHz",
    [131]="LTE Band 12 - 700MHz",
    [132]="LTE Band 13 - 700MHz",
    [133]="LTE Band 14 - 700MHz",
    -- 15 reserved
    -- 16 reserved
    [134]="LTE Band 17 - 700MHz",
    [143]="LTE Band 18 - 800MHz",
    [144]="LTE Band 19 - 800MHz",
    [145]="LTE Band 20 - 800MHz",
    [146]="LTE Band 21 - 1500MHz",
    -- 22 unknown in QMI
    [152]="LTE Band 23 - 2000MHz",
    [147]="LTE Band 24 - 1600MHz",
    [148]="LTE Band 25 - 1900MHz",
    [153]="LTE Band 26 - 850MHz",
    -- 27 unknown in QMI
    [158]="LTE Band 28 - 700MHz",
    [159]="LTE Band 29 - 700MHz",
    [160]="LTE Band 30 - 2300MHz",
    [165]="LTE Band 31 - 450MHz",
    [154]="LTE Band 32 - 1500MHz",
    [135]="LTE Band 33 - TDD 2100",
    [136]="LTE Band 34 - TDD 2100",
    [137]="LTE Band 35 - TDD 1900",
    [138]="LTE Band 36 - TDD 1900",
    [139]="LTE Band 37 - TDD 1900",
    [140]="LTE Band 38 - TDD 2600",
    [141]="LTE Band 39 - TDD 1900",
    [142]="LTE Band 40 - TDD 2300",
    [149]="LTE Band 41 - TDD 2500",
    [150]="LTE Band 42 - TDD 3500",
    [151]="LTE Band 43 - TDD 3700",

    [163]="LTE Band 46 - TDD 5200",
    [167]="LTE Band 47 - TDD 5900",
    [168]="LTE Band 48 - TDD 3600",
    [161]="LTE Band 66 - 1700MHz",
    [166]="LTE Band 71 - 600MHz",
    [155]="LTE Band 125",
    [156]="LTE Band 126",
    [157]="LTE Band 127",
    [162]="LTE Band 250",

    -- Up to 512 NR-5G active bands:
    [250]="NR5G BAND 1",
    [251]="NR5G BAND 2",
    [252]="NR5G BAND 3",
    [253]="NR5G BAND 5",
    [254]="NR5G BAND 7",
    [255]="NR5G BAND 8",
    [256]="NR5G BAND 20",
    [257]="NR5G BAND 28",
    [258]="NR5G BAND 38",
    [259]="NR5G BAND 41",
    [260]="NR5G BAND 50",
    [261]="NR5G BAND 51",
    [262]="NR5G BAND 66",
    [263]="NR5G BAND 70",
    [264]="NR5G BAND 71",
    [265]="NR5G BAND 74",
    [266]="NR5G BAND 75",
    [267]="NR5G BAND 76",
    [268]="NR5G BAND 77",
    [269]="NR5G BAND 78",
    [270]="NR5G BAND 79",
    [271]="NR5G BAND 80",
    [272]="NR5G BAND 81",
    [273]="NR5G BAND 82",
    [274]="NR5G BAND 83",
    [275]="NR5G BAND 84",
    [276]="NR5G BAND 85",
    [277]="NR5G BAND 257",
    [278]="NR5G BAND 258",
    [279]="NR5G BAND 259",
    [280]="NR5G BAND 260",
    [281]="NR5G BAND 261",
    [282]="NR5G BAND 12",
    [283]="NR5G BAND 25",
    [284]="NR5G BAND 34",
    [285]="NR5G BAND 39",
    [286]="NR5G BAND 40",
    [287]="NR5G BAND 65",
    [288]="NR5G BAND 86",
    [289]="NR5G BAND 48",
    [290]="NR5G BAND 14",

    -- Up to 512 NR-5G NSA bands, start at 768 (0x300):
    [768]="NR5G NSA BAND 1",
    [769]="NR5G NSA BAND 2",
    [770]="NR5G NSA BAND 3",
    [771]="NR5G NSA BAND 5",
    [772]="NR5G NSA BAND 7",
    [773]="NR5G NSA BAND 8",
    [774]="NR5G NSA BAND 20",
    [775]="NR5G NSA BAND 28",
    [776]="NR5G NSA BAND 38",
    [777]="NR5G NSA BAND 41",
    [778]="NR5G NSA BAND 50",
    [779]="NR5G NSA BAND 51",
    [780]="NR5G NSA BAND 66",
    [781]="NR5G NSA BAND 70",
    [782]="NR5G NSA BAND 71",
    [783]="NR5G NSA BAND 74",
    [784]="NR5G NSA BAND 75",
    [785]="NR5G NSA BAND 76",
    [786]="NR5G NSA BAND 77",
    [787]="NR5G NSA BAND 78",
    [788]="NR5G NSA BAND 79",
    [789]="NR5G NSA BAND 80",
    [790]="NR5G NSA BAND 81",
    [791]="NR5G NSA BAND 82",
    [792]="NR5G NSA BAND 83",
    [793]="NR5G NSA BAND 84",
    [794]="NR5G NSA BAND 85",
    [795]="NR5G NSA BAND 257",
    [796]="NR5G NSA BAND 258",
    [797]="NR5G NSA BAND 259",
    [798]="NR5G NSA BAND 260",
    [799]="NR5G NSA BAND 261",
    [800]="NR5G NSA BAND 12",
    [801]="NR5G NSA BAND 25",
    [802]="NR5G NSA BAND 34",
    [803]="NR5G NSA BAND 39",
    [804]="NR5G NSA BAND 40",
    [805]="NR5G NSA BAND 65",
    [806]="NR5G NSA BAND 86",
    [807]="NR5G NSA BAND 48",
    [808]="NR5G NSA BAND 14",

    -- Up to 512 NR-5G SA bands, start at 1536 (0x600):
    [1536]="NR5G SA BAND 1",
    [1537]="NR5G SA BAND 2",
    [1538]="NR5G SA BAND 3",
    [1539]="NR5G SA BAND 5",
    [1540]="NR5G SA BAND 7",
    [1541]="NR5G SA BAND 8",
    [1542]="NR5G SA BAND 20",
    [1543]="NR5G SA BAND 28",
    [1544]="NR5G SA BAND 38",
    [1545]="NR5G SA BAND 41",
    [1546]="NR5G SA BAND 50",
    [1547]="NR5G SA BAND 51",
    [1548]="NR5G SA BAND 66",
    [1549]="NR5G SA BAND 70",
    [1550]="NR5G SA BAND 71",
    [1551]="NR5G SA BAND 74",
    [1552]="NR5G SA BAND 75",
    [1553]="NR5G SA BAND 76",
    [1554]="NR5G SA BAND 77",
    [1555]="NR5G SA BAND 78",
    [1556]="NR5G SA BAND 79",
    [1557]="NR5G SA BAND 80",
    [1558]="NR5G SA BAND 81",
    [1559]="NR5G SA BAND 82",
    [1560]="NR5G SA BAND 83",
    [1561]="NR5G SA BAND 84",
    [1562]="NR5G SA BAND 85",
    [1563]="NR5G SA BAND 257",
    [1564]="NR5G SA BAND 258",
    [1565]="NR5G SA BAND 259",
    [1566]="NR5G SA BAND 260",
    [1567]="NR5G SA BAND 261",
    [1568]="NR5G SA BAND 12",
    [1569]="NR5G SA BAND 25",
    [1570]="NR5G SA BAND 34",
    [1571]="NR5G SA BAND 39",
    [1572]="NR5G SA BAND 40",
    [1573]="NR5G SA BAND 65",
    [1574]="NR5G SA BAND 86",
    [1575]="NR5G SA BAND 48",
    [1576]="NR5G SA BAND 14",

    -- Band groups
    [self.HEX_GROUP_GSM]="GSM all",
    [self.HEX_GROUP_WCDMA]="WCDMA all",
    [self.HEX_GROUP_LTE]="LTE all",
    [self.HEX_GROUP_NR5G_NSA]="NR5G NSA all",
    [self.HEX_GROUP_NR5G_SA]="NR5G SA all",
    [self.HEX_GROUP_NR5G]="NR5G all",

    [self.HEX_GROUP_GSM_WCDMA]="GSM/WCDMA all",
    [self.HEX_GROUP_GSM_LTE]="GSM/LTE all",
    [self.HEX_GROUP_GSM_NR5G]="GSM/NR5G all",
    [self.HEX_GROUP_GSM_NR5G_NSA]="GSM/NR5G NSA all",
    [self.HEX_GROUP_GSM_NR5G_SA]="GSM/NR5G SA all",
    [self.HEX_GROUP_WCDMA_LTE]="WCDMA/LTE all",
    [self.HEX_GROUP_WCDMA_NR5G]="WCDMA/NR5G all",
    [self.HEX_GROUP_WCDMA_NR5G_NSA]="WCDMA/NR5G NSA all",
    [self.HEX_GROUP_WCDMA_NR5G_SA]="WCDMA/NR5G SA all",
    [self.HEX_GROUP_LTE_NR5G]="LTE/NR5G all",
    [self.HEX_GROUP_LTE_NR5G_NSA]="LTE/NR5G NSA all",
    [self.HEX_GROUP_LTE_NR5G_SA]="LTE/NR5G SA all",

    [self.HEX_GROUP_GSM_WCDMA_LTE]="GSM/WCDMA/LTE all",
    [self.HEX_GROUP_GSM_WCDMA_NR5G]="GSM/WCDMA/NR5G all",
    [self.HEX_GROUP_GSM_WCDMA_NR5G_NSA]="GSM/WCDMA/NR5G NSA all",
    [self.HEX_GROUP_GSM_WCDMA_NR5G_SA]="GSM/WCDMA/NR5G SA all",
    [self.HEX_GROUP_GSM_LTE_NR5G]="GSM/LTE/NR5G all",
    [self.HEX_GROUP_GSM_LTE_NR5G_NSA]="GSM/LTE/NR5G NSA all",
    [self.HEX_GROUP_GSM_LTE_NR5G_SA]="GSM/LTE/NR5G SA all",
    [self.HEX_GROUP_WCDMA_LTE_NR5G]="WCDMA/LTE/NR5G all",
    [self.HEX_GROUP_WCDMA_LTE_NR5G_NSA]="WCDMA/LTE/NR5G NSA all",
    [self.HEX_GROUP_WCDMA_LTE_NR5G_SA]="WCDMA/LTE/NR5G SA all",

    [self.HEX_GROUP_ALL]="All bands"
  }

  -- mapping from GSM/WCDMA band mask bit to hex code
  self.band_bit_to_hex = {
    [7] =47,
    [8] =44,
    [9] =45,
    [16]=40,
    [17]=41,
    [18]=42,
    [19]=43,
    [20]=46,
    [21]=48,
    [22]=80,
    [23]=81,
    [24]=82,
    [25]=83,
    [26]=84,
    [27]=85,
    [48]=86,
    [49]=87,
    [50]=88,
    [61]=90,
    [60]=91
  }

  -- mapping from LTE band mask bit to hex code
  -- for LTE bands, bit i -> band i+1 where i=0,..,42 with some reserved gaps
  self.lte_band_bit_to_hex = {
    [0] =120,
    [1] =121,
    [2] =122,
    [3] =123,
    [4] =124,
    [5] =125,
    [6] =126,
    [7] =127,
    [8] =128,
    [9] =129,
    [10]=130,
    [11]=131,
    [12]=132,
    [13]=133,
    [16]=134,
    [17]=143,
    [18]=144,
    [19]=145,
    [20]=146,
    [22]=152,
    [23]=147,
    [24]=148,
    [25]=153,
    [27]=158,
    [28]=159,
    [29]=160,
    [31]=154,
    [32]=135,
    [33]=136,
    [34]=137,
    [35]=138,
    [36]=139,
    [37]=140,
    [38]=141,
    [39]=142,
    [40]=149,
    [41]=150,
    [42]=151,
    [45]=163,
    [46]=167,
    [47]=168,
    [65]=161,
    [70]=166,
    [124]=155,
    [125]=156,
    [126]=157,
    [249]=162
  }

  -- @TODO:: There is no appropriate reference for this table, yet.
  --         Need to update with correct value, later.
  self.nr5g_band_bit_to_hex = {
    [0]=250,
    [1]=251,
    [2]=252,
    [3]=253,
    [4]=254,
    [5]=255,
    [6]=256,
    [7]=257,
    [8]=258,
    [9]=259,
    [10]=260,
    [11]=261,
    [12]=262,
    [13]=263,
    [14]=264,
    [15]=265,
    [16]=266,
    [17]=267,
    [18]=268,
    [19]=269,
    [20]=270,
    [21]=271,
    [22]=272,
    [23]=273,
    [24]=274,
    [25]=275,
    [26]=276,
    [27]=277,
    [28]=278,
    [29]=279,
    [30]=280,
    [31]=281,
    [32]=282,
    [33]=283,
    [34]=284,
    [35]=285,
    [36]=286,
    [37]=287,
    [38]=288,
    [39]=289,
  }

  -- Following are 2 tables to allow NR5G decimal bands mapping to the bit positions from Table A-1 Band to get the band name as required.
  -- NOTE: Keep in sync with self.band_names for NR5G NSA and SA (mapping table above).

  -- NR-5G NSA:
  self.nr5g_nsa_band_to_hex = {
    [1]=768,
    [2]=769,
    [3]=770,
    [5]=771,
    [7]=772,
    [8]=773,
    [20]=774,
    [28]=775,
    [38]=776,
    [41]=777,
    [50]=778,
    [51]=779,
    [66]=780,
    [70]=781,
    [71]=782,
    [74]=783,
    [75]=784,
    [76]=785,
    [77]=786,
    [78]=787,
    [79]=788,
    [80]=789,
    [81]=790,
    [82]=791,
    [83]=792,
    [84]=793,
    [85]=794,
    [257]=795,
    [258]=796,
    [259]=797,
    [260]=798,
    [261]=799,
    [12]=800,
    [25]=801,
    [34]=802,
    [39]=803,
    [40]=804,
    [65]=805,
    [86]=806,
    [48]=807,
    [14]=808
  }

  -- NR-5G SA:
  self.nr5g_sa_band_to_hex = {
    [1]=1536,
    [2]=1537,
    [3]=1538,
    [5]=1539,
    [7]=1540,
    [8]=1541,
    [20]=1542,
    [28]=1543,
    [38]=1544,
    [41]=1545,
    [50]=1546,
    [51]=1547,
    [66]=1548,
    [70]=1549,
    [71]=1550,
    [74]=1551,
    [75]=1552,
    [76]=1553,
    [77]=1554,
    [78]=1555,
    [79]=1556,
    [80]=1557,
    [81]=1558,
    [82]=1559,
    [83]=1560,
    [84]=1561,
    [85]=1562,
    [257]=1563,
    [258]=1564,
    [259]=1565,
    [260]=1566,
    [261]=1567,
    [12]=1568,
    [25]=1569,
    [34]=1570,
    [39]=1571,
    [40]=1572,
    [65]=1573,
    [86]=1574,
    [48]=1575,
    [14]=1576
  }

  self.radio_names={
    [0]="None (no service)",
    [1]="CDMA2000 1X",
    [2]="CDMA2000 HRPD (1xEV-DO)",
    [3]="AMPS",
    [4]="GSM",
    [5]="UMTS",
    [8]="LTE",
    [9]="TD-SCDMA",
    [12]="NR5G",
  }

  self.reg_state_names ={
    [0x00] = "not registered",
    [0x01] = "registered",
    [0x02] = "not registered, searching",
    [0x03] = "registration denied",
    [0x04] = "registration unknown",
  }

  self.srv_status_names = {
    [0] = "no service",
    [1] = "limited service",
    [2] = "service",
    [3] = "limited regional service",
    [4] = "power save",
  }

  self.nw_name_source_names = {
    [0] = "unknown",
    [1] = "opl_pnn",
    [2] = "cphs_ons",
    [3] = "nitz",
    [4] = "se13",
    [5] = "mcc_mnc",
    [6] = "spn",
  }

  -- MCC to country initial mapping. TODO: complete this list
  self.mcc_ci = {
    [310] = "US",
    [311] = "US",
    [312] = "US",
    [313] = "US",
    [314] = "US",
    [315] = "US",
    [316] = "US",
    [302] = "CA",
    [505] = "AU",
    [530] = "NZ",
    [460] = "CN",
    [454] = "HK",
    [455] = "MO",
    [466] = "TW",
    [234] = "GB",
    [235] = "GB",
    [208] = "FR",
    [262] = "DE",
    [244] = "FI",
    [202] = "GR",
    [222] = "IT",
    [240] = "SE",
    [228] = "CH",
    [204] = "NL",
    [440] = "JP",
    [441] = "JP",
    [450] = "KR",
    [404] = "IN",
    [405] = "IN",
  }

  -- Service Domain Selection Values
  self.service_domain = {
    ["CS_ONLY"]   = 0x00,
    ["PS_ONLY"]   = 0x01,
    ["CS_PS"]     = 0x02,
    ["PS_ATTACH"] = 0x03,
    ["PS_DETACH"] = 0x04,
    ["PS_DETACH_NO_PREF_CHANGE"] = 0x05,
    ["ON_DEMAND_PS_ATTACH"]      = 0x06,
    ["FORCE_PS_DETACH"]          = 0x07,
  }

  self.service_domain_names = {
    [0x00] = "CS_ONLY",
    [0x01] = "PS_ONLY",
    [0x02] = "CS_PS",
    [0x03] = "PS_ATTACH",
    [0x04] = "PS_DETACH",
    [0x05] = "PS_DETACH_NO_PREF_CHANGE",
    [0x06] = "ON_DEMAND_PS_ATTACH",
    [0x07] = "FORCE_PS_DETACH",
  }

  -- mapping from Mode Preference mask bit to RAT string
  self.rat_bit_to_string = {
    [0] = "CDMA2000 1X",
    [1] = "CDMA2000 HRPD",
    [2] = "GSM",
    [3] = "WCDMA",
    [4] = "LTE",
    [5] = "TDSCDMA",
    [6] = "NR5G"
  }

  self.rat_to_mask = {
    ["CDMA2000 1X"]   = 0x01,
    ["CDMA2000 HRPD"] = 0x02,
    ["GSM"]           = 0x04,
    ["WCDMA"]         = 0x08,
    ["LTE"]           = 0x10,
    ["TDSCDMA"]       = 0x20,
    ["NR5G"]          = 0x40
  }

  -- initial BPLMN scan time
  self.initial_bplmn_scan_attempt_time = nil
  -- PS attach status
  self.ps_attached = false

  -- Tables of band names to bit masks & hex codes that the module supports.
  -- get_module_band_list() will fill both.
  self.band_name_to_mask = {} -- each entry is a {mask, lte_mask, nr5g_nsa_mask, nr5g_sa_mask} pair
  self.band_name_to_hex = {} -- entry is a hex scalar

  -- last list of cell measurement information
  self.last_cell_measurements={}

  -- a flag to indicate manual PLMN scan mode
  self.manual_plmn_scan_mode = false

  -- Register radio band capability
  self.radio_capability={
    is_lte_supported=false,
    is_nr5g_supported=false
  }

  self.nr5g_band_pref_tbl = {
    [0] = {name = "bits_1_64", bits = {}},
    [1] = {name = "bits_65_128", bits = {}},
    [2] = {name = "bits_129_192", bits = {}},
    [3] = {name = "bits_193_256", bits = {}},
    [4] = {name = "bits_257_320", bits = {}},
    [5] = {name = "bits_321_384", bits = {}},
    [6] = {name = "bits_385_448", bits = {}},
    [7] = {name = "bits_449_512", bits = {}},
  }

  -- bandwidth enum in QMI_NAS_GET_RF_BAND_INFO and QMI_NAS_RF_BAND_INFO_IND
  self.rf_bandwidth_name = {
    [0] = "LTE 1.4MHz",
    [1] = "LTE 3MHz",
    [2] = "LTE 5MHz",
    [3] = "LTE 10MHz",
    [4] = "LTE 15MHz",
    [5] = "LTE 20MHz",
    [6] = "NR5G 5MHz",
    [7] = "NR5G 10MHz",
    [8] = "NR5G 15MHz",
    [9] = "NR5G 20MHz",
    [10] = "NR5G 25MHz",
    [11] = "NR5G 30MHz",
    [12] = "NR5G 40MHz",
    [13] = "NR5G 50MHz",
    [14] = "NR5G 60MHz",
    [15] = "NR5G 80MHz",
    [16] = "NR5G 90MHz",
    [17] = "NR5G 100MHz",
    [18] = "NR5G 200MHz",
    [19] = "NR5G 400MHz",
    [20] = "GSM 0.2MHz",
    [21] = "TDSCDMA 1.6MHz",
    [22] = "WCDMA 5MHz",
    [23] = "WCDMA 10MHz",
    [24] = "NR5G 70MHz",
  }

  -- Incremental network scan information list
  self.network_scan_info = {}

  self.network_inuse_status={
  [0x00] = "unknown",
  [0x01] = "current serving",
  [0x02] = "available"
  }
  self.network_roaming_status={
  [0x00] = "unknown",
  [0x01] = "home",
  [0x02] = "roam"
  }
  self.network_forbidden_status={
  [0x00] = "unknown",
  [0x01] = "forbidden",
  [0x02] = "not forbidden"
  }
  self.network_preferred_status={
  [0x00] = "unknown",
  [0x01] = "preferred",
  [0x02] = "not preferred"
  }

  -- network type mask
  -- Bit 0 – GSM
  -- Bit 1 – UMTS
  -- Bit 2 – LTE
  -- Bit 3 – TD-SCDMA
  -- Bit 4 – NR5G
  self.network_type_mask_bit={
    ["GSM"]   = 0x01,
    ["UMTS"]  = 0x02,
    ["LTE"]   = 0x04,
    ["TDSCDMA"]= 0x08,
    ["NR5G"]  = 0x10,
  }

end

--[[
    Get country initial prefix for a given PLMN ID (MCC+MNC)

    @param mccmnc A string of 5 or 6 characters for MCC+MNC
    @return The country initial prefix including the separator space or an empty
    string if no country initial is found.
    @note This prefix should be prepended to the network name if CI bit is set.
--]]
function QmiNas:get_country_initial(mccmnc)
  if not mccmnc then return '' end
  local mcc = mccmnc:sub(1,3)
  local ci = self.mcc_ci[tonumber(mcc)]
  return ci and ci .. ' ' or ''
end

-- helper functions to check if a band hex code belongs to a specific category
function QmiNas:hex_in_gsm_band(hex)
  return (hex>=self.HEX_GSM_MIN and hex<self.HEX_WCDMA_MIN) or
    (self.bit.band(hex, self.HEX_GROUP_GSM)==self.HEX_GROUP_GSM)
end
function QmiNas:hex_in_wcdma_band(hex)
  return (hex>=self.HEX_WCDMA_MIN and hex<self.HEX_LTE_MIN) or
    (self.bit.band(hex, self.HEX_GROUP_WCDMA)==self.HEX_GROUP_WCDMA)
end
function QmiNas:hex_in_lte_band(hex)
  return (hex>=self.HEX_LTE_MIN and hex<self.HEX_TDSCDMA_MIN) or
    (self.bit.band(hex, self.HEX_GROUP_LTE)==self.HEX_GROUP_LTE)
end
function QmiNas:hex_in_nr5g_nsa_band(hex)
  return (hex>=self.HEX_NR5G_NSA_MIN and hex<(self.HEX_NR5G_NSA_MIN+self.HEX_GROUP_NR5G_COUNT)) or
    (self.bit.band(hex, self.HEX_GROUP_NR5G_NSA)==self.HEX_GROUP_NR5G_NSA)
end
function QmiNas:hex_in_nr5g_sa_band(hex)
  return (hex>=self.HEX_NR5G_SA_MIN and hex<(self.HEX_NR5G_SA_MIN+self.HEX_GROUP_NR5G_COUNT)) or
    (self.bit.band(hex, self.HEX_GROUP_NR5G_SA)==self.HEX_GROUP_NR5G_SA)
end

function QmiNas:build_network_time_arg(universal_time,time_zone,daylt_sav_adj)
  local ia = {}

  -- copy time / Day of the week. 0 is Monday and 6 is Sunday.
  for _,m in ipairs{"year","month","day","hour","minute","second","day_of_week"} do
    ia[m] = tonumber(universal_time[m])
  end

  ia.time_zone=time_zone -- in increments of 15 min
  ia.daylt_sav_adj=daylt_sav_adj -- Daylight saving adjustment in hours. Possible values: 0, 1, and 2.

  return ia
end

function QmiNas:build_ext_modem_network_status_arg(resp)
  local ia = {}

  local radio_in_used = ""

  local function length_of_decimal(val)
    local val_str = string.format("%d",val)
    return #val_str
  end

  for _,k in ipairs{"gsm","wcdma","lte","nr5g"} do

    -- sys_info_valid
    local sys_info_valid_key = k .. "_sys_info_valid"

    if self.luaq.is_c_true(resp[sys_info_valid_key]) then

      local sys_info=resp[k .. "_sys_info"]
      local threegpp_specific_sys_info = sys_info.threegpp_specific_sys_info
      local common_sys_info = sys_info.common_sys_info

      -- add mnc & mcc
      if self.luaq.is_c_true(threegpp_specific_sys_info.network_id_valid) then
        ia.network_mcc = self.ffi.string(threegpp_specific_sys_info.network_id.mcc,3)
        local mnc_len = (threegpp_specific_sys_info.network_id.mnc[2] == 0xff) and 2 or 3
        ia.network_mnc = self.ffi.string(threegpp_specific_sys_info.network_id.mnc,mnc_len)
      end

      -- cell id
      if self.luaq.is_c_true(threegpp_specific_sys_info.cell_id_valid) then
        ia.cellid = tonumber(threegpp_specific_sys_info.cell_id)
      end

      -- lac
      if self.luaq.is_c_true(threegpp_specific_sys_info.lac_valid) then
        ia.lac = tonumber(threegpp_specific_sys_info.lac)
      end

      -- plmn
      if ia.network_mcc and ia.network_mnc then
        ia.plmn = ia.network_mcc .. ia.network_mnc
      end

      if ia.plmn and ia.cellid then
        if k == "gsm" or k == "wcdma" then

          -- cid
          ia.cid = self.bit.band(ia.cellid,0xffff)
          if ia.cellid ~= ia.cid then
            ia.lcid = ia.cellid -- Long CID = RNC ID + CID
            ia.rncid = self.bit.band(self.bit.rshift(ia.lcid,16),0xfff)
          end

          -- cgi
          if ia.lac then
            -- check validation
            if length_of_decimal(ia.cid)>5 then
              self.l.log("LOG_ERR",string.format("Total number of CID digits is more than 5 digits (cid=%d)",ia.cid))
            elseif length_of_decimal(ia.lac)>5 then
              self.l.log("LOG_ERR",string.format("Total number of LAC digits is more than 5 digits (lac=%d)",ia.lac))
            else
              ia.cgi = ia.plmn .. string.format("%05d%05d",ia.lac,ia.cid)
            end

          end
        elseif k == "lte" then
          -- eci
          ia.eci = ia.cellid

          -- check validation of ECI 28 bits
          if length_of_decimal(ia.eci)>9 then
            self.l.log("LOG_ERR",string.format("Total number of ECI digits is more than 9 digits (eci=%d)",ia.eci))
          else
            local eci_str = string.format("%09d",ia.eci)

            ia.enb_id = tostring(self.bit.rshift(ia.eci,8))
          end

        end
      end

      -- tac
      if k == "lte" then
        if self.luaq.is_c_true(sys_info.lte_specific_sys_info.tac_valid) then
          ia.tac=tonumber(sys_info.lte_specific_sys_info.tac)
        end
      end

      -- reject cause(Refer to Annex G of 3GPP TS 24.008 to get reject code details)
      if self.luaq.is_c_member_and_true(threegpp_specific_sys_info, "reg_reject_info_valid") then
          ia.last_reject_cause = threegpp_specific_sys_info.reg_reject_info.rej_cause
      end
    end

    -- srv_info_valid
    local srv_info_valid_key = k .. "_srv_status_info_valid"

    self.l.log("LOG_DEBUG",string.format("[radio-loss] report validation (k=%s,valid=%d)",k,resp[sys_info_valid_key]))
    if self.luaq.is_c_true(resp[sys_info_valid_key]) then

      -- get service info
      local srv_status_info=resp[k .. "_srv_status_info"]

      -- get members of service info
      local is_pref_data_path = self.luaq.is_c_true(srv_status_info.is_pref_data_path)
      local srv_status = tonumber(srv_status_info.srv_status)
      local true_srv_status = tonumber(srv_status_info.true_srv_status)

      self.l.log("LOG_DEBUG",string.format("[radio-loss] report status (k=%s,pref=%s,stat=%d,tstat=%d)",k,is_pref_data_path,srv_status,true_srv_status))

      -- set information in ia (information argument)
      if self.srv_status_names[srv_status] == "service" then
        ia.srv_status = k
      else
        ia.srv_status = self.srv_status_names[srv_status]
      end

      -- set information in ia (information argument)
      if self.srv_status_names[true_srv_status] == "service" then
        ia.true_srv_status = k
      else
        ia.true_srv_status = self.srv_status_names[true_srv_status]
      end

    end

  end

  -- no service by default only when there is no RAT is available
  if not ia.srv_status then
    ia.srv_status = self.srv_status_names[0]
  end

  -- no service by default only when there is no RAT is available
  if not ia.true_srv_status then
    ia.true_srv_status = self.srv_status_names[0]
  end

  -- reprot into syslog
  self.l.log("LOG_DEBUG",string.format("[radio-loss] report string status (stat='%s',tstat='%s')",ia.srv_status,ia.true_srv_status))

  -- rac
  for _,k in ipairs{"gsm","wcdma"} do
    local valid_key = k .. "_rac_valid"
    local rac_key =  k .. "_rac"

    if self.luaq.is_c_true(resp[valid_key]) then
      ia.rac=resp[rac_key]
    end
  end

  -- Upper layer 5G availability indication in LTE SIB2
  -- 0 : 5G not available
  -- 1 : 5G available
  if self.luaq.is_c_true(resp.endc_available_valid) then
    ia.endc=resp.endc_available
  end

  -- Get NR5G cell ID and primary PLMN to create NCGI
  if self.luaq.is_c_member(resp, "nr5g_cell_id_valid") and self.luaq.is_c_true(resp.nr5g_cell_id_valid) and
     self.luaq.is_c_member(resp, "primary_plmn_info_valid") and self.luaq.is_c_true(resp.primary_plmn_info_valid) then
    local nci = resp.nr5g_cell_id
    local p_mcc = self.ffi.string(resp.primary_plmn_info.plmn_id.mcc,3)
    local p_mnc_len = (resp.primary_plmn_info.plmn_id.mnc[2] == 0xff) and 2 or 3
    local p_mnc = self.ffi.string(resp.primary_plmn_info.plmn_id.mnc,p_mnc_len)
    ia.ncgi = string.format("%s%s%s%09d",p_mcc ,p_mnc,p_mnc_len==2 and "0" or "",tonumber(nci))
  end

  -- NR5G tac
  if self.luaq.is_c_member_and_true(resp, "nr5g_tac_info_valid") then
    local tac = resp.nr5g_tac_info.tac
    ia.nr5g_tac = self.bit.lshift(self.bit.band(tac[0], 0xff), 16) + self.bit.lshift(self.bit.band(tac[1], 0xff), 8) + self.bit.band(tac[2], 0xff)
  end

  return ia
end

function QmiNas:build_modem_network_status_arg(resp)
  local ia = {}

  if self.luaq.is_c_true(resp.current_plmn_valid) then
    ia.network_mcc = resp.current_plmn.mobile_country_code
    ia.network_mnc = resp.current_plmn.mobile_network_code
    ia.network_desc = self.ffi.string(resp.current_plmn.network_description)
  end

  if self.luaq.is_c_true(resp.roaming_indicator_valid) then
    ia.roaming_status = (resp.roaming_indicator=="NAS_ROAMING_IND_ON_V01") and "active" or "deactive"
  end

  ia.reg_state_no = tonumber(resp.serving_system.registration_state)
  ia.reg_state_name = self.reg_state_names[ia.reg_state_no]
  ia.reg_state = resp.serving_system.registration_state == "NAS_REGISTERED_V01"

  if self.luaq.is_c_true(resp.serving_system.radio_if_len) then
    local radio_no=tonumber(resp.serving_system.radio_if[0])
    if radio_no ~= nil then
      ia.serving_system=string.lower(self.radio_names[radio_no])
    else
      ia.serving_system=string.lower(self.radio_names[0])
    end
  end

  local ps_attach_state = resp.serving_system.ps_attach_state == "NAS_PS_ATTACHED_V01"
  ia.ps_attach_state=ps_attach_state

  -- maintain ps attache status in NAS module
  self.ps_attached = ps_attach_state

  return ia
end

function QmiNas:build_sig_info_arg(resp)
  local ia={}

  if self.luaq.is_c_true(resp.gsm_sig_info_valid) then
    ia.rssi = resp.gsm_sig_info
  end

  if self.luaq.is_c_true(resp.wcdma_sig_info_valid) then
    ia.rssi = resp.wcdma_sig_info.rssi
    ia.ecio = resp.wcdma_sig_info.ecio*(-0.5)
  end

  if self.luaq.is_c_true(resp.lte_sig_info_valid) then
    ia.rssi = resp.lte_sig_info.rssi
    ia.rsrq = resp.lte_sig_info.rsrq
    ia.rsrp = resp.lte_sig_info.rsrp
    ia.snr = resp.lte_sig_info.snr*0.1
  end

  -- get NR5G SNR and RSRP
  if self.luaq.is_c_member_and_true(resp, "nr5g_sig_info_valid") then
    if resp.nr5g_sig_info.rsrp ~= -32768 then
      ia.nr5g_rsrp = resp.nr5g_sig_info.rsrp
    else
      ia.nr5g_rsrp = ""
    end

    if resp.nr5g_sig_info.snr ~= -32768 then
        ia.nr5g_snr = resp.nr5g_sig_info.snr*0.1
    else
      ia.nr5g_snr=""
    end
  end

  -- get NR5G RSRQ
  if self.luaq.is_c_member_and_true(resp, "nr5g_rsrq_valid") then
    if resp.nr5g_rsrq ~= -32768 then
      ia.nr5g_rsrq = resp.nr5g_rsrq
    else
      ia.nr5g_rsrq = ""
    end
  end

  return ia
end

-- TODO Current implementation implies there is only one
-- RF band info exists but there could be more according to
-- QMI NAS document. But we only uses first active channel information
-- so leave it as it is at the moment.
-- Note : The channel number here is SSB ARFCN in SA mode.
function QmiNas:build_rf_info_arg(rf_band_info, ext_rf_band_info, rf_bandwidth_info)
  local ia = {}
  local band_no = tonumber(rf_band_info.active_band)

  ia.current_band=self.band_names[band_no]

  -- Check if extended format exists for LTE/5G which supports
  -- 32 bits channel number then update the channel info.
  if ext_rf_band_info then
    ia.active_channel = tonumber(ext_rf_band_info.active_channel)
  else
    ia.active_channel = tonumber(rf_band_info.active_channel)
  end

  if rf_bandwidth_info then
    ia.rf_bandwidth = self.rf_bandwidth_name[tonumber(rf_bandwidth_info.bandwidth)] or ""
  else
    ia.rf_bandwidth = ""
  end

  -- clear old LTE/NSA mode channel value in SA mode
  local sys_so = luardb.get("wwan.0.system_network_status.current_system_so")
  if sys_so and string.find(sys_so, "5G SA") then
    ia.active_channel = ""
  end

  return ia
end

function QmiNas:build_lte_cphy_ca_ind_arg(resp)
  local ia={}

    -- provide data as (freq<<12 | pci<<2 | state)
    ia.scell_freq_pci_state=tonumber(resp.cphy_ca.freq)*4096 + tonumber(resp.cphy_ca.pci)*4 + tonumber(resp.cphy_ca.scell_state)

  local ca_bandwidth={ [0]="1.4MHz", [1]="3MHz", [2]="5MHz", [3]="10MHz", [4]="15MHz", [5]="20MHz" }
  if self.luaq.is_c_member_and_true(resp, "pcell_info_valid") then
    local info = resp.pcell_info
    local freq = info.freq
    if freq == 0 and self.luaq.is_c_member_and_true(resp, "pcell_freq_valid") then
      freq = resp.pcell_freq
    end
    ia.pcell_info_pci = info.pci
    ia.pcell_info_freq = info.freq
    ia.pcell_info_cphy_ca_dl_bandwidth = ca_bandwidth[tonumber(info.cphy_ca_dl_bandwidth)]
    ia.pcell_info_band = self.band_names[tonumber(info.band)]
  end

  local scell_list = {}
  if self.luaq.is_c_member_and_true(resp, "scell_info_valid")
     and self.luaq.is_c_member_and_true(resp, "scell_idx_valid")
     and self.luaq.is_c_member_and_true(resp, "ul_configured_valid") then
    local info = resp.scell_info
    local freq = info.freq
    if freq == 0 and self.luaq.is_c_member_and_true(resp, "scell_freq_valid") then
      freq = resp.scell_freq
    end

    ia.scell_info_pci = info.pci
    ia.scell_info_freq = info.freq
    ia.scell_info_cphy_ca_dl_bandwidth = ca_bandwidth[tonumber(info.cphy_ca_dl_bandwidth)]
    ia.scell_info_band = self.band_names[tonumber(info.band)]

    table.insert(scell_list, { pci = info.pci,
                               freq = info.freq,
                               cphy_ca_dl_bandwidth = ca_bandwidth[tonumber(info.cphy_ca_dl_bandwidth)],
                               band = self.band_names[tonumber(info.band)],
                               scell_state = info.scell_state,
                               scell_idx = resp.scell_idx,
                               ul_configured = resp.ul_configured,
                             })
  end

  if self.luaq.is_c_member_and_true(resp, "unchanged_scell_info_list_ext_valid") then
     local list_len = tonumber(resp.unchanged_scell_info_list_ext_len) or 0
     for i=0, (list_len - 1) do
       local info = resp.unchanged_scell_info_list_ext[i]
       table.insert(scell_list, { pci = info.pci,
                                  freq = info.freq,
                                  cphy_ca_dl_bandwidth = ca_bandwidth[tonumber(info.cphy_ca_dl_bandwidth)],
                                  band = self.band_names[tonumber(info.band)],
                                  scell_state = info.scell_state,
                                  scell_idx = info.scell_idx,
                                  ul_configured = info.ul_configured,
                                })
     end
  end

  if #scell_list > 0 then
    table.sort(scell_list, function(a, b) return(a.scell_idx < b.scell_idx) end)
    ia.scell_list = scell_list
  end

  return ia
end

function QmiNas:build_lte_cphy_ca_info_arg(resp)
  local ia={}

  local cphy_ca_valid = not self.luaq.is_c_member(resp,"cphy_ca_valid") or self.luaq.is_c_true(resp.cphy_ca_valid)

  -- nas_lte_cphy_ca_ind_msg doesn't have this field but
  -- nas_get_lte_cphy_ca_info_resp_msg has. Do nothing for this field.
  if cphy_ca_valid then
  end

  local ca_dl_bandwidth_names={
    [0x00] = "1.4",
    [0x01] = "3",
    [0x02] = "5",
    [0x03] = "10",
    [0x04] = "15",
    [0x05] = "20",
  }
  local bw=nil

  local ca_dl_band_names={
    [120] = "BAND 1",
    [121] = "BAND 2",
    [122] = "BAND 3",
    [123] = "BAND 4",
    [124] = "BAND 5",
    [125] = "BAND 6",
    [126] = "BAND 7",
    [127] = "BAND 8",
    [128] = "BAND 9",
    [129] = "BAND 10",
    [130] = "BAND 11",
    [131] = "BAND 12",
    [132] = "BAND 13",
    [133] = "BAND 14",
    [134] = "BAND 17",
    [135] = "BAND 33",
    [136] = "BAND 34",
    [137] = "BAND 35",
    [138] = "BAND 36",
    [139] = "BAND 37",
    [140] = "BAND 38",
    [141] = "BAND 39",
    [142] = "BAND 40",
    [143] = "BAND 18",
    [144] = "BAND 19",
    [145] = "BAND 20",
    [146] = "BAND 21",
    [147] = "BAND 24",
    [148] = "BAND 25",
    [149] = "BAND 41",
    [150] = "BAND 42",
    [151] = "BAND 43",
    [152] = "BAND 23",
    [153] = "BAND 26",
    [154] = "BAND 32",
    [155] = "BAND 125",
    [156] = "BAND 126",
    [157] = "BAND 127",
    [158] = "BAND 28",
    [159] = "BAND 29",
    [160] = "BAND 30",
    [161] = "BAND 66",
    [162] = "BAND 250",
    [163] = "BAND 46",
    [164] = "BAND 27",
    [165] = "BAND 31",
    [166] = "BAND 71",
    [167] = "BAND 47",
    [168] = "BAND 48",
  }
  local band=nil

  if self.luaq.is_c_true(resp.cphy_ca_dl_bandwidth_valid) then
    bw=tonumber(resp.cphy_ca_dl_bandwidth)
    if bw >= 0 and bw <= 5 then
      ia.dl_bandwidth=ca_dl_bandwidth_names[bw]
    else
      self.l.log("LOG_DEBUG",string.format("Invalid bandwidth index %d",bw))
      return nil
    end
  end

  if self.luaq.is_c_true(resp.cphy_scell_info_list_valid) then
    ia.scell_info_list_len=tonumber(resp.cphy_scell_info_list_len)
    self.l.log("LOG_DEBUG","Scell info array exist, len = "..resp.cphy_scell_info_list_len)
    ia.scell_info = {}
    for i=0,ia.scell_info_list_len-1 do
      ia.scell_info[i]={}
      ia.scell_info[i].scell_idx=tonumber(resp.cphy_scell_info_list[i].scell_idx)
      ia.scell_info[i].pci=tonumber(resp.cphy_scell_info_list[i].scell_info.pci)
      ia.scell_info[i].freq=tonumber(resp.cphy_scell_info_list[i].scell_info.freq)
      bw=tonumber(resp.cphy_scell_info_list[i].scell_info.cphy_ca_dl_bandwidth)
      if bw >= 0 and bw <= 5 then
        ia.scell_info[i].cphy_ca_dl_bandwidth=ca_dl_bandwidth_names[bw]
      else
        self.l.log("LOG_ERR",string.format("Invalid scell bandwidth index %d",bw))
        return nil
      end
      band=tonumber(resp.cphy_scell_info_list[i].scell_info.band)
      ia.scell_info[i].band=ca_dl_band_names[band]
      if not ia.scell_info[i].band then
        self.l.log("LOG_ERR",string.format("Invalid scell band index %d",band))
        return nil
      end
      ia.scell_info[i].scell_state=tonumber(resp.cphy_scell_info_list[i].scell_info.scell_state)
      self.l.log("LOG_INFO",string.format("Scell[%d].scell_idx %d", i, ia.scell_info[i].scell_idx))
      self.l.log("LOG_INFO",string.format("Scell[%d].pci %d", i, ia.scell_info[i].pci))
      self.l.log("LOG_INFO",string.format("Scell[%d].freq %d", i, ia.scell_info[i].freq))
      self.l.log("LOG_INFO",string.format("Scell[%d].cphy_ca_dl_bandwidth %s", i, ia.scell_info[i].cphy_ca_dl_bandwidth))
      self.l.log("LOG_INFO",string.format("Scell[%d].band %s", i, ia.scell_info[i].band))
      self.l.log("LOG_INFO",string.format("Scell[%d].scell_state %d", i, ia.scell_info[i].scell_state))
    end
  end

  if self.luaq.is_c_true(resp.scell_info_valid) then
    ia.scell_pci=tonumber(resp.scell_info.pci)
    ia.scell_freq=tonumber(resp.scell_info.freq)
    bw=tonumber(resp.scell_info.cphy_ca_dl_bandwidth)
    if bw >= 0 and bw <= 5 then
      ia.scell_dl_bandwidth=ca_dl_bandwidth_names[bw]
    else
      self.l.log("LOG_ERR",string.format("Invalid scell bandwidth index %d",bw))
      return nil
    end
    band=tonumber(resp.scell_info.band)
    ia.scell_dl_band=ca_dl_band_names[band]
    if not ia.scell_dl_band then
      self.l.log("LOG_ERR",string.format("Invalid scell band index %d",band))
      return nil
    end
  end

  if self.luaq.is_c_true(resp.scell_idx_valid) then
    ia.scell_idx=resp.scell_idx
  end

  return ia
end

-- check if the given band(rat) is hidden
-- @params band A band or rat name to check
-- ex) CDMA2000, GSM, LTE, NR5G...
function QmiNas:is_hidden_band(band)
  local hidden_bands = self.dconfig.hidden_bands
  if not hidden_bands then return false end
  for _,v in ipairs(hidden_bands:split(",")) do
    if v == band then
      return true
    end
  end
  return false
end

function QmiNas:build_plmn_list_arg(resp)
  local ia={}
  local plmnid_with_pcs_digit = {}
  -- Firstly need to work out MNC digits if they are available
  if self.luaq.is_c_true(resp.mnc_includes_pcs_digit_valid) and resp.mnc_includes_pcs_digit_len then
    for i=0,resp.mnc_includes_pcs_digit_len-1 do
      -- this plmnid is not for display. it is used to indicate whether this plmn has three 3 digits MNC or not.
      local plmnid=string.format("%d%d", resp.mnc_includes_pcs_digit[i].mcc, resp.mnc_includes_pcs_digit[i].mnc)
      plmnid_with_pcs_digit[plmnid]=resp.mnc_includes_pcs_digit[i].mnc_includes_pcs_digit
    end
  end
  if self.luaq.is_c_true(resp.nas_3gpp_network_info_valid) and resp.nas_3gpp_network_info_len then
    local plmn_list=resp.nas_3gpp_network_info
    for i=0,resp.nas_3gpp_network_info_len-1 do
      local network={}
      local mcc, mnc
      local cns_stat -- status for backward compatibility
      local pid=string.format("%d%d", plmn_list[i].mobile_country_code, plmn_list[i].mobile_network_code)
      mcc=string.format("%0.3d",plmn_list[i].mobile_country_code)
      if plmnid_with_pcs_digit[pid] ~= 0 then
        mnc=string.format("%0.3d", plmn_list[i].mobile_network_code)
      else
        mnc=string.format("%0.2d", plmn_list[i].mobile_network_code)
      end
      network["mcc"] = mcc
      network["mnc"] = mnc
      local stats = tonumber(plmn_list[i].network_status)
      local inuse_idx = self.bit.band(stats, 0x3)
      stats = self.bit.rshift(stats, 2)
      local roaming_idx = self.bit.band(stats, 0x3)
      stats = self.bit.rshift(stats, 2)
      local forbidden_idx = self.bit.band(stats, 0x3)
      stats = self.bit.rshift(stats, 2)
      local preferred_idx = self.bit.band(stats, 0x3)
      if forbidden_idx == 0x01 then
        cns_stat = 2 -- forbidden
      elseif inuse_idx == 0x01 then
        cns_stat = 4 -- current
      elseif inuse_idx == 0x02 then
        cns_stat = 1 -- available
      else
        cns_stat = 0 -- unknown
      end
      network["cns_stat"] = cns_stat
      network["status"] = string.format("%s,%s,%s,%s",
        self.network_inuse_status[inuse_idx], self.network_roaming_status[roaming_idx],
        self.network_forbidden_status[forbidden_idx], self.network_preferred_status[preferred_idx])
      network["description"] = self.ffi.string(plmn_list[i].network_description)
      -- add extra index to separate network info with same PLMN but different RAT
      ia[mcc..mnc..i]=network
    end

    if self.luaq.is_c_true(resp.nas_network_radio_access_technology_valid) and resp.nas_network_radio_access_technology_len then
      local rats = resp.nas_network_radio_access_technology
      for i=0,resp.nas_network_radio_access_technology_len-1 do
        local pid = string.format("%d%d",rats[i].mcc,rats[i].mnc)
        local plmnid
        if plmnid_with_pcs_digit[pid] ~= 0 then
          plmnid = string.format("%0.3d%0.3d",rats[i].mcc,rats[i].mnc)
        else
          plmnid = string.format("%0.3d%0.2d",rats[i].mcc,rats[i].mnc)
        end
        if ia[plmnid..i] then
          local rat = tonumber(rats[i].rat)
          if rat == 0x04 then
            rat = 3 -- GERAN
          elseif rat == 0x05 then
            rat = 7 -- UMTS
          elseif rat == 0x08 then
            rat = 9 -- LTE
          else
            rat = 0 -- unknown
          end
          -- add extra index to separate network info with same PLMN but different RAT
          ia[plmnid..i]["rat"] = rat
        end
      end
    end
  end

  return ia
end

-- build the pci earfcn list
-- resp: input (result of pci scan)
-- return: pci scan values as a table
--         empty table on errors/no result
function QmiNas:build_pci_list_arg(resp)
  if not self.luaq.is_c_true(resp.pci_plmn_info_valid) then
    self.l.log("LOG_ERR",'pci plmn info is invalid ')
    return nil
  end

  local cell_info = resp.pci_plmn_info.pci_cell_info
  local signal_info = resp.pci_plmn_info.signal_info
  local ia = {
    rsrp     = signal_info.rsrp,
    rsrp_rx0 = signal_info.rsrp_rx0,
    rsrp_rx0 = signal_info.rsrp_rx1,
    rsrq     = signal_info.rsrq,
    rsrq_rx0 = signal_info.rsrq_rx0,
    rsrq_rx1 = signal_info.rsrq_rx1
  }

  for i=0,resp.pci_plmn_info.pci_cell_info_len-1 do
    local cell = cell_info[i]
    local ia_single = {
      plmn_info      = {},
      pci            = cell.cell_id,
      earfcn         = cell.freq,
      global_cell_id = cell.global_cell_id,
      plmn_len       = cell.plmn_len
    }
    for j=0,cell.plmn_len-1 do
      local plmn = cell.plmn[j]
      ia_single.plmn_info[j+1] = {
        mcc = plmn.mcc,
        mnc = plmn.mnc,
        mnc_includes_pcs_digit = plmn.mnc_includes_pcs_digit
      }
    end
    ia[i+1] = ia_single
  end

  return ia
end

-- This function accumulates partial network scan information
-- into global variable.
function QmiNas:build_network_scan_arg(resp)
  self.l.log("LOG_DEBUG","---build_network_scan_arg---")
  self.l.log("LOG_DEBUG",string.format("nas_network_scan_info_len = %d", resp.nas_network_scan_info_len))
  local network_list=resp.nas_network_scan_info
  for i=0,resp.nas_network_scan_info_len-1 do
    local network={}
    local mcc, mnc
    local cns_stat -- status for backward compatibility
    local pid=string.format("%d%d", network_list[i].mobile_country_code, network_list[i].mobile_network_code)
    mcc=string.format("%0.3d",network_list[i].mobile_country_code)
    if network_list[i].mnc_includes_pcs_digit ~= 0 then
      mnc=string.format("%0.3d", network_list[i].mobile_network_code)
    else
      mnc=string.format("%0.2d", network_list[i].mobile_network_code)
    end
    network["mcc"] = mcc
    network["mnc"] = mnc
    local stats = tonumber(network_list[i].network_status)
    local inuse_idx = self.bit.band(stats, 0x3)
    stats = self.bit.rshift(stats, 2)
    local roaming_idx = self.bit.band(stats, 0x3)
    stats = self.bit.rshift(stats, 2)
    local forbidden_idx = self.bit.band(stats, 0x3)
    stats = self.bit.rshift(stats, 2)
    local preferred_idx = self.bit.band(stats, 0x3)
    if forbidden_idx == 0x01 then
      cns_stat = 2 -- forbidden
    elseif inuse_idx == 0x01 then
      cns_stat = 4 -- current
    elseif inuse_idx == 0x02 then
      cns_stat = 1 -- available
    else
      cns_stat = 0 -- unknown
    end
    network["cns_stat"] = cns_stat
    network["status"] = string.format("%s,%s,%s,%s",
      self.network_inuse_status[inuse_idx], self.network_roaming_status[roaming_idx],
      self.network_forbidden_status[forbidden_idx], self.network_preferred_status[preferred_idx])
    network["description"] = self.ffi.string(network_list[i].network_description)
    local rat = tonumber(network_list[i].rat)
    local ratTbl = {
      [0x04] = 3, -- GERAN
      [0x05] = 7, -- UMTS
      [0x08] = 9, -- LTE
      [0x0C] = 12 -- NR5G
    }
    rat = ratTbl[rat] or 0
    network["rat"] = rat
    self.network_scan_info[mcc..mnc..rat]=network
  end
end

-- QMI_NAS_PERFORM_NETWORK_SCAN is not supporting NR5G scan so
-- QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN should be used.
-- Scan with QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN for NR5G.
-- QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN request responds immediately
-- then receives QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN_IND with partial information
-- until receving completely.
function QmiNas:send_incremental_plmn_scan_msg()
  self.l.log('LOG_DEBUG', "send QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN")
  local qm = self.luaq.new_msg(self.m.QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN)
  -- Scan all network types except for hidden bands
  local mask = 0x00
  for nt, bit in pairs(self.network_type_mask_bit) do
    if not self:is_hidden_band(nt) then
      mask = self.bit.bor(mask, bit)
    end
  end
  qm.req.network_type = mask
  self.l.log('LOG_DEBUG', string.format("qm.req.network_type = 0x%x", qm.req.network_type))
  qm.req.scan_type = "NAS_SCAN_TYPE_PLMN_V01"

  -- initialize network scan info list
  self.network_scan_info = {}

  self.l.log("LOG_DEBUG", "perform async manual_plmn scan, sending QMI QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN")
  if self.luaq.send_msg(qm) then
    self.l.log('LOG_DEBUG', "sent QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN successfully")
    return true
  end

  self.l.log("LOG_DEBUG", "failed to post QMI QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN")
  return false
end

-- QMI_NAS_PERFORM_NETWORK_SCAN is not supporting NR5G scan so
-- QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN should be used first.
-- Scan with QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN first for NR5G.
-- If failed then send QMI_NAS_PERFORM_NETWORK_SCAN for LTE scan
-- QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN request responds immediately
-- then receives QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN_IND with partial information
-- until receving completely.
function QmiNas:perform_async_bplmn_scan()
  self.l.log('LOG_ERR', "perform async bplmn scan, send QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN first")
  if self:send_incremental_plmn_scan_msg() then
    return true
  end

  self.l.log("LOG_DEBUG", "failed to post QMI QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN")
  self.l.log("LOG_DEBUG", "trying QMI_NAS_PERFORM_NETWORK_SCAN")
  local qm = self.luaq.new_msg(self.m.QMI_NAS_PERFORM_NETWORK_SCAN)
  -- network type mask (4 : LTE)
  -- Bit 0 – GSM
  -- Bit 1 – UMTS
  -- Bit 2 – LTE
  -- Bit 3 – TD-SCDMA
  qm.req.network_type = 4
  qm.req.scan_type = "NAS_SCAN_TYPE_PLMN_V01"

  self.l.log("LOG_DEBUG", "perform async bplmn scan, sending QMI QMI_NAS_PERFORM_NETWORK_SCAN")
  if not self.luaq.send_msg_async(qm, self.config.modem_bplmn_scan_timeout) then
    self.l.log("LOG_ERR", "failed to post QMI QMI_NAS_PERFORM_NETWORK_SCAN")
    return false
  end
  return true
end

function QmiNas:perform_async_manual_plmn_scan()
  local qm = self.luaq.new_msg(self.m.QMI_NAS_PERFORM_NETWORK_SCAN)
  -- network type mask (4 : LTE)
  -- Bit 0 – GSM
  -- Bit 1 – UMTS
  -- Bit 2 – LTE
  -- Bit 3 – TD-SCDMA
  qm.req.network_type = 4
  qm.req.scan_type = "NAS_SCAN_TYPE_PLMN_V01"

  -- canceler closure function
  local timeout_func = function(qm)
    self.l.log('LOG_ERR', "[qmi-async] manual plmn scan timeout")
    self.wrdb:setp("PLMN.cmd.status", "[error]")
    self.manual_plmn_scan_mode = false
  end

  self.l.log("LOG_NOTICE", "perform async manual_plmn scan, sending QMI QMI_NAS_PERFORM_NETWORK_SCAN")
  if not self.luaq.send_msg_async(qm, self.config.modem_bplmn_scan_timeout, timeout_func) then
    self.l.log("LOG_ERR", "failed to post QMI QMI_NAS_PERFORM_NETWORK_SCAN")
    return false
  end
  -- set manual scan mode flag
  self.l.log('LOG_DEBUG', "[qmi-async] manual scan mode set")
  self.manual_plmn_scan_mode = true
  return true
end

-- QMI_NAS_PERFORM_NETWORK_SCAN is not supporting NR5G scan so
-- QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN should be used.
-- Scan with QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN for NR5G.
-- QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN request responds immediately
-- then receives QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN_IND with partial information
-- until receving completely.
function QmiNas:perform_async_incremental_manual_plmn_scan()
  if self:send_incremental_plmn_scan_msg() then
    -- set manual scan mode flag
    self.l.log('LOG_DEBUG', "[qmi-async] manual scan mode set")
    self.manual_plmn_scan_mode = true
    return true
  end
  return false
end

function QmiNas:pci_scan(type,event,a)
  local qm = self.luaq.new_msg(self.m.QMI_NAS_PERFORM_NETWORK_SCAN)
  qm.req.network_type_valid = 1
  qm.req.network_type = 4
  qm.req.scan_type_valid = 1
  qm.req.scan_type = 3

  if not self.luaq.send_msg_async(qm,self.config.modem_pci_scan_timeout) then
    self.l.log("LOG_ERR",string.format("pci_scan: send_msg failed"))
    return false, -1, qm.resp, qm
  end

  return true
end

function QmiNas:poll_network_scan_mode()
  local succ,qerr,resp
  succ,qerr,resp = self.luaq.req(self.m.QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE)
  if not succ then
    self.l.log("LOG_ERR", "QMI_NAS_GET_SYSTEM_SELECTON_PREFERENCE request failed")
    return false
  end

  local netSelPref
  if self.luaq.is_c_true(resp.net_sel_pref_valid) then
    if tonumber(resp.net_sel_pref) == 0 then
      netSelPref = "Automatic"
    else
      netSelPref = "Manual"
    end
    if luardb.get("wwan.0.PLMN_selectionMode") ~= netSelPref then
      luardb.set("wwan.0.PLMN_selectionMode", netSelPref)
    end
    return true
  else
    self.l.log("LOG_ERR", "net_sel_pref is not valid")
    return false
  end
end


-- PLMN_select: "0" for auto selection or "rat,mcc,mnc" for manual selection
function QmiNas:perform_plmn_select(plmn_sel)
  if not plmn_sel then
    self.l.log("LOG_ERR", "plmn_sel is nil")
    return false
  end
  self.l.log("LOG_DEBUG", "[peform_plmn_select] plmn_sel="..plmn_sel)
  local selArray = plmn_sel:split(',')
  local mode = tonumber(selArray[1])
  if #selArray ~= 1 and #selArray ~= 3 then
    self.l.log("LOG_ERR", "Bad plmn_sel format " .. plmn_sel)
    return false
  end
  local net_sel_pref
  local mcc, mnc = 0, 0
  local mode_pref
  if #selArray == 1 then
    if mode ~= 0  then
      self.l.log("LOG_ERR", "Bad plmn_sel format " .. plmn_sel)
      return false
    end
    -- set automatic net_sel_pref
    net_sel_pref = "NAS_NET_SEL_PREF_AUTOMATIC_V01"
  else
    -- manual net_sel_pref
    net_sel_pref = "NAS_NET_SEL_PREF_MANUAL_V01"
    mcc = tonumber(selArray[2])
    mnc = tonumber(selArray[3])
    local modePrefTbl = {
      [0] = nil,  -- auto
      [0x05] = 7, -- UMTS,
      [0x08] = 9, -- LTE,
      [0x10] = 12,-- NR5G,
    }
    if not mode or mode == 0 then -- default to auto
      mode_pref = nil
    elseif mode == 9 then -- LTE
      mode_pref = 0x10
    elseif mode == 12 then -- NR5G
      mode_pref = 0x40
    elseif mode >= 1 and mode <= 3 then -- GSM
      mode_pref = 0x04
    elseif mode >= 4 and mode <=8 then -- UMTS
      mode_pref = 0x08
    else
      self.l.log("LOG_ERR", "Bad plmn_sel mode " .. mode)
      return false
    end
    if not mcc or mcc < 0 or mcc > 999 then
      self.l.log("LOG_ERR", "Bad mcc " .. tostring(mcc))
      return false
    end
    if not mnc or mnc < 0 or mnc > 999 then
      self.l.log("LOG_ERR", "Bad mnc " .. tostring(mnc))
      return false
    end
  end
  self.l.log("LOG_DEBUG", string.format("plmn_sel mode_pref=%s, net_sel_pref=%s, mcc=%d, mnc=%d",mode_pref,net_sel_pref,mcc,mnc))
  local qm = self.luaq.new_msg(self.m.QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE)
  if mode_pref then
    qm.req.mode_pref_valid = true
    qm.req.mode_pref = mode_pref
  end
  qm.req.net_sel_pref_valid = true
  qm.req.net_sel_pref.net_sel_pref = net_sel_pref
  qm.req.net_sel_pref.mcc = mcc
  qm.req.net_sel_pref.mnc = mnc
  if not self.luaq.send_msg(qm) then
    self.l.log("LOG_ERR", "QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE send_msg failed")
    return false
  end
  local succ,qerr,resp=self.luaq.ret_qm_resp(qm)
  if not succ then
    self.l.log("LOG_ERR", string.format("QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE request failed, error 0x%04x", qerr))
  end
  return succ
end

-- convert a uint64_t mask to a set (table) of bit positions
function QmiNas:mask_to_bits(mask)
  local bits={}
  local lo=tonumber(self.ffi.cast("uint32_t",mask)) -- low 32 bits
  local hi=tonumber(self.ffi.cast("uint32_t",mask/2^32)) -- high 32 bits
  local i
  for i=0,31 do
    if self.bit.band(2^i, lo) ~= 0 then
      bits[i]=true
    end
    if self.bit.band(2^i, hi) ~= 0 then
      bits[i+32]=true
    end
  end
  return bits
end

-- convert hexadecimal mask(should start with 0x) in string to a set (table) of bit positions
function QmiNas:hexmask_to_bits(mask)
  local bits = {}
  local width = 8 -- 8 hexadecimal digits for uint32_t
  local lmask = string.match(mask, "%s*0[xX](%x+)")
  if not lmask then return bits end
  if (#lmask % width) ~= 0 then
    lmask =  string.format("%s%s", string.rep("0", width - (#lmask % width)), lmask)
  end
  local uint32Tbl = {}
  for k, _ in string.gmatch(lmask, string.rep("%x", width)) do
    table.insert(uint32Tbl, k)
  end

  local uint32Tblsize = #uint32Tbl
  for i, v in ipairs(uint32Tbl) do
    local hex = tonumber('0x' .. v)
    for j=0, (4*width) -1 do
      if self.bit.band(2^j, hex) ~= 0 then
        bits[j + (uint32Tblsize-i)*4*width]=true
      end
    end
  end

  return bits
end

-- convert a set of bit positions to a uint64_t mask
function QmiNas:bits_to_mask(bits)
  local lo,hi = 0,0
  local i
  for i,_ in pairs(bits) do
    if i<32 then
      lo = lo + 2^i
    else
      hi = hi + 2^(i-32)
    end
  end
  return self.ffi.new("uint64_t",lo) + self.ffi.new("uint64_t",hi)*2^32
end

-- convert a set of bit positions to a mask with hex format
function QmiNas:bits_to_hex_mask(bits, minDigits)
  local form = "0x%08x%08x"
  local lo,hi = 0,0
  local i
  for i,_ in pairs(bits) do
    if i<32 then
      lo = lo + 2^i
    else
      hi = hi + 2^(i-32)
    end
  end
  if minDigits and minDigits > 16 then
      form = string.format("0x%%0%dx%%08x", minDigits - 8)
  end
  return string.format(form, hi, lo)
end

-- convert a set of bit positions to a comma separated string
function QmiNas:bits_to_string(bits)
  local i, res
  for i,_ in pairs(bits) do
    res = res and (res .. "," .. i) or tostring(i)
  end
  return res or ""
end

-- bit or over two sets of bit positions (set union)
function QmiNas:bits_or(x, y)
  local res={}
  local i
  for i,_ in pairs(x) do
    res[i]=true
  end
  for i,_ in pairs(y) do
    res[i]=true
  end
  return res
end

-- Parse and save to RDB all of nr5g, nr5g-nsa and nr5g-sa band for backward compatibility
function QmiNas:bandPrefGet()
  local succ,qerr,resp
  local gsmMask, wcdmaMask, lteMask, nr5gMask, nr5gMask_NSA, nr5gMask_SA = {},{},{},{},{},{}
  local countGSM, countWCDMA, countLTE, countNR5G, countNR5G_NSA, countNR5G_SA = 0,0,0,0,0,0
  succ,qerr,resp = self.luaq.req(self.m.QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE)
  if not succ then
    self.l.log("LOG_ERR", "QMI_NAS_GET_SYSTEM_SELECTON_PREFERENCE request failed")
    return false
  end

  -- Get Mode Preference with band get command
#ifdef V_RAT_SEL_y
  if self.luaq.is_c_true(resp.mode_pref_valid) then
    local mode_pref = self:mask_to_bits(resp.mode_pref)
    local mode_pref_str = ""
    self.l.log("LOG_DEBUG", string.format("bandPrefGet mode_pref=[%s]",self:bits_to_string(mode_pref)))
    for i=0,6 do
      if mode_pref[i] and not self:is_hidden_band(self.rat_bit_to_string[i] ) then
        mode_pref_str = mode_pref_str .. self.rat_bit_to_string[i] .. ";"
      end
    end
    mode_pref_str = mode_pref_str:sub(1, -2)
    self.l.log("LOG_DEBUG", string.format("bandPrefGet mode_pref_str=[%s]",mode_pref_str))
    luardb.set("wwan.0.currentband.current_selrat", string.format("%s", mode_pref_str))
  end
#endif

  local bandArray={}
  local i, hex
  if self.luaq.is_c_true(resp.band_pref_valid) then
    local band_pref=self:mask_to_bits(resp.band_pref)
    self.l.log("LOG_DEBUG", string.format("bandPrefGet band_pref=[%s]",self:bits_to_string(band_pref)))
    for i=0,63 do
      if band_pref[i] then
        hex = self.band_bit_to_hex[i]
        if hex and self.band_names[hex] then
          bandArray[#bandArray+1]=self.band_names[hex]
          if self:hex_in_gsm_band(hex) then
            countGSM=countGSM+1
            gsmMask[i] = true
          elseif self:hex_in_wcdma_band(hex) then
            countWCDMA=countWCDMA+1
            wcdmaMask[i] = true
          end
        end
      end
    end
  end

  if self.luaq.is_c_true(resp.lte_band_pref_ext_valid) then
    local lte_band_pref_1_64=self:mask_to_bits(resp.lte_band_pref_ext.bits_1_64)
    local lte_band_pref_65_128=self:mask_to_bits(resp.lte_band_pref_ext.bits_65_128)
    local lte_band_pref_129_192=self:mask_to_bits(resp.lte_band_pref_ext.bits_129_192)
    local lte_band_pref_193_256=self:mask_to_bits(resp.lte_band_pref_ext.bits_193_256)
    self.l.log("LOG_DEBUG", string.format("bandPrefGet lte_band_pref_1_64=[%s]",self:bits_to_string(lte_band_pref_1_64)))
    self.l.log("LOG_DEBUG", string.format("bandPrefGet lte_band_pref_65_128=[%s]",self:bits_to_string(lte_band_pref_65_128)))
    self.l.log("LOG_DEBUG", string.format("bandPrefGet lte_band_pref_129_192=[%s]",self:bits_to_string(lte_band_pref_129_192)))
    self.l.log("LOG_DEBUG", string.format("bandPrefGet lte_band_pref_193_256=[%s]",self:bits_to_string(lte_band_pref_193_256)))
    local function add_lte_band_array(hex, maskBit)
      if hex and self.band_names[hex] then
        self.l.log("LOG_DEBUG", string.format("bandPrefGet hex=%x, band=[%s]",hex, self.band_names[hex]))
        bandArray[#bandArray+1]=self.band_names[hex]
        countLTE=countLTE+1
        lteMask[maskBit] = true
      end
    end
    for i=0,63 do
      hex = nil
      if lte_band_pref_1_64[i] then
        add_lte_band_array(self.lte_band_bit_to_hex[i], i)
      end
      if lte_band_pref_65_128[i] then
        add_lte_band_array(self.lte_band_bit_to_hex[i+64], i+64)
      end
      if lte_band_pref_129_192[i] then
        add_lte_band_array(self.lte_band_bit_to_hex[i+128], i+128)
      end
      if lte_band_pref_193_256[i] then
        add_lte_band_array(self.lte_band_bit_to_hex[i+192], i+192)
      end
    end
  end

  if not self.luaq.is_c_true(resp.lte_band_pref_ext_valid) and self.luaq.is_c_true(resp.band_pref_ext_valid) then
    local band_pref_ext=self:mask_to_bits(resp.band_pref_ext)
    self.l.log("LOG_DEBUG", string.format("bandPrefGet band_pref_ext=[%s]",self:bits_to_string(band_pref_ext)))
    for i=0,63 do
      if band_pref_ext[i] then
        hex = self.lte_band_bit_to_hex[i]
        if hex and self.band_names[hex] then
          bandArray[#bandArray+1]=self.band_names[hex]
          countLTE=countLTE+1
          lteMask[i] = true
        end
      end
    end
  end

  -- < NR5G band preference setting >
  -- Deprecated in v1.268 but keep the logic to support the products with old modem
  if self.luaq.is_c_member_and_true(resp, "nr5g_band_pref_valid") then
    local nr5g_band_pref_tbl = {
      bits_1_64    = {offset = 0,   bits = nil},
      bits_65_128  = {offset = 64,  bits = nil},
      bits_129_192 = {offset = 128, bits = nil},
      bits_193_256 = {offset = 192, bits = nil},
      bits_257_320 = {offset = 256, bits = nil},
      bits_321_384 = {offset = 320, bits = nil},
      bits_385_448 = {offset = 384, bits = nil},
      bits_449_512 = {offset = 448, bits = nil},
    }
    for k, v in pairs(nr5g_band_pref_tbl) do
      v.bits = self:mask_to_bits(resp.nr5g_band_pref[k])
      self.l.log("LOG_DEBUG", string.format("bandPrefGet nr5g_band_pref_%s=[%s]", k, self:bits_to_string(v.bits)))
    end
    for k, v in pairs(nr5g_band_pref_tbl) do
      for bit, _ in pairs(v.bits) do
        local hex = self.nr5g_band_bit_to_hex[bit + v.offset]
        if hex and self.band_names[hex] then
          self.l.log("LOG_DEBUG", string.format("bandPrefGet hex=%x, band=[%s]",hex, self.band_names[hex]))
          bandArray[#bandArray+1]=self.band_names[hex]
          countNR5G=countNR5G+1
          nr5gMask[i] = true
        end
      end
    end
  end

  -- NR5G SA band preference setting
  if self.luaq.is_c_member_and_true(resp, "nr5g_sa_band_pref_valid") then
    local nr5g_sa_band_pref_tbl = {
      bits_1_64    = {offset = 0,   bits = nil},
      bits_65_128  = {offset = 64,  bits = nil},
      bits_129_192 = {offset = 128, bits = nil},
      bits_193_256 = {offset = 192, bits = nil},
      bits_257_320 = {offset = 256, bits = nil},
      bits_321_384 = {offset = 320, bits = nil},
      bits_385_448 = {offset = 384, bits = nil},
      bits_449_512 = {offset = 448, bits = nil},
    }
    for k, v in pairs(nr5g_sa_band_pref_tbl) do
      v.bits = self:mask_to_bits(resp.nr5g_sa_band_pref[k])
      self.l.log("LOG_DEBUG", string.format("bandPrefGet nr5g_sa_band_pref_%s=[%s]", k, self:bits_to_string(v.bits)))
    end
    for k, v in pairs(nr5g_sa_band_pref_tbl) do
      for bit, _ in pairs(v.bits) do
        -- nr5g_sa_band_to_hex and band_names table
        local hex = self.nr5g_sa_band_to_hex[bit + v.offset + 1]
        if hex and self.band_names[hex] then
          self.l.log("LOG_DEBUG", string.format("bandPrefGet hex=%x, band=[%s]",hex, self.band_names[hex]))
          bandArray[#bandArray+1]=self.band_names[hex]
          countNR5G_SA=countNR5G_SA+1
          nr5gMask_SA[bit + v.offset] = true
        end
      end
    end
  end

  -- NR5G NSA band preference setting
  if self.luaq.is_c_member_and_true(resp, "nr5g_nsa_band_pref_valid") then
    local nr5g_nsa_band_pref_tbl = {
      bits_1_64    = {offset = 0,   bits = nil},
      bits_65_128  = {offset = 64,  bits = nil},
      bits_129_192 = {offset = 128, bits = nil},
      bits_193_256 = {offset = 192, bits = nil},
      bits_257_320 = {offset = 256, bits = nil},
      bits_321_384 = {offset = 320, bits = nil},
      bits_385_448 = {offset = 384, bits = nil},
      bits_449_512 = {offset = 448, bits = nil},
    }
    for k, v in pairs(nr5g_nsa_band_pref_tbl) do
      v.bits = self:mask_to_bits(resp.nr5g_nsa_band_pref[k])
      self.l.log("LOG_DEBUG", string.format("bandPrefGet nr5g_nsa_band_pref_%s=[%s]", k, self:bits_to_string(v.bits)))
    end
    for k, v in pairs(nr5g_nsa_band_pref_tbl) do
      for bit, _ in pairs(v.bits) do
        -- nr5g_nsa_band_to_hex and band_names table
        local hex = self.nr5g_nsa_band_to_hex[bit + v.offset + 1]
        if hex and self.band_names[hex] then
          self.l.log("LOG_DEBUG", string.format("bandPrefGet hex=%x, band=[%s]",hex, self.band_names[hex]))
          bandArray[#bandArray+1]=self.band_names[hex]
          countNR5G_NSA=countNR5G_NSA+1
          nr5gMask_NSA[bit + v.offset] = true
        end
      end
    end
  end

  self.l.log("LOG_DEBUG", string.format("bandPrefGet count: GSM=%d, WCDMA=%d, LTE=%d, NR5G=%d, NR5GNSA=%d, NR5GSA=%d",
                                        countGSM, countWCDMA, countLTE, countNR5G, countNR5G_NSA, countNR5G_SA))
  self.l.log("LOG_DEBUG", string.format("bandPrefGet mask: GSM=[%s], WCDMA=[%s], LTE=[%s], NR5G=[%s], NR5GNSA=[%s], NR5GSA=[%s]",
                                        self:bits_to_string(gsmMask), self:bits_to_string(wcdmaMask), self:bits_to_string(lteMask),
                                        self:bits_to_string(nr5gMask), self:bits_to_string(nr5gMask_NSA), self:bits_to_string(nr5gMask_SA)))
  luardb.set("wwan.0.currentband.current_selband.hexmask",
                          string.format("GSM:%s,WCDMA:%s,LTE:%s,NR5G:%s,NR5GNSA:%s,NR5GSA:%s",
                                        self:bits_to_hex_mask(gsmMask, 32), self:bits_to_hex_mask(wcdmaMask, 32), self:bits_to_hex_mask(lteMask, 32),
                                        self:bits_to_hex_mask(nr5gMask, 64),self:bits_to_hex_mask(nr5gMask_NSA, 64), self:bits_to_hex_mask(nr5gMask_SA, 64)))

  if not self.dconfig.enable_multi_band_sel and countGSM+countWCDMA+countLTE+countNR5G+countNR5G_NSA+countNR5G_SA>1 then
    --[[
      By default (apart from Titan), multiple band selection is NOT enabled.
      Only a single band or a band group can be selected.
      If more than one band is selected, work out the smallest band group that
      covers all the selected bands.
      For Titan, this step is skipped. No band groups exist.
    --]]

    hex = 0
    if countGSM > 0 then
        hex = self.bit.bor(hex, self.HEX_GROUP_GSM)
    end
    if countWCDMA > 0 then
        hex = self.bit.bor(hex, self.HEX_GROUP_WCDMA)
    end
    if countLTE > 0 then
        hex = self.bit.bor(hex, self.HEX_GROUP_LTE)
    end
    if countNR5G > 0 or countNR5G_NSA > 0 then
        hex = self.bit.bor(hex, self.HEX_GROUP_NR5G_NSA)
    end
    if countNR5G_SA > 0 then
      hex = self.bit.bor(hex, self.HEX_GROUP_NR5G_SA)
    end

    -- overwrite individual bands by a single band group
    bandArray={self.band_names[hex]}
  end

  return table.concat(bandArray,';')
end

function QmiNas:init_nr5g_band_pref_tbl()
  for _, elem in pairs(self.nr5g_band_pref_tbl) do
    elem.bits = {}
  end
end

-- Bands should be a semicolon-separated string with band names or hex codes as components
-- Ignore nr5g bands and only accept nr5g-nsa and nr5g-sa bands
-- Old nr5g band is siliently ignored in module even if set.
function QmiNas:bandPrefSet(bands)
  local succ,qerr,resp
  if not bands then
    return false
  end
  self.l.log("LOG_DEBUG", "bandPrefSet bands="..bands)
  local mask, hex
  local mode_pref, band_pref, lte_band_pref, nr5g_band_pref, nr5g_nsa_band_pref, nr5g_sa_band_pref = 0,{},{},{},{},{}
  local bandArray=bands:split(';')
  for _,band in ipairs(bandArray) do
    hex = tonumber(band,16)
    if hex then
      -- band is a hex code, convert to name
      band = self.band_names[hex]
    end
    mask = band and self.band_name_to_mask[band]
    hex = band and self.band_name_to_hex[band]
    if hex then
      self.l.log("LOG_DEBUG", string.format("bandPrefSet hex=%d",hex))
      self.l.log("LOG_DEBUG", string.format("bandPrefSet mask=%s",table.tostring(mask)))

      -- Setting radio technology mode preference
      if self:hex_in_gsm_band(hex) then
        mode_pref = self.bit.bor(mode_pref, 0x04)
      end
      if self:hex_in_wcdma_band(hex) then
        mode_pref = self.bit.bor(mode_pref, 0x08)
      end
      if self:hex_in_lte_band(hex) then
        mode_pref = self.bit.bor(mode_pref, 0x10)
      end
      if self:hex_in_nr5g_nsa_band(hex) or self:hex_in_nr5g_sa_band(hex) then
        mode_pref = self.bit.bor(mode_pref, 0x40)
      end
      -- Setting radio band masks
      if mask.mask then -- GSM & WCDMA
        band_pref = self:bits_or(band_pref, mask.mask)
      end
      if mask.lte_mask then -- LTE
        lte_band_pref = self:bits_or(lte_band_pref, mask.lte_mask)
      end
      if mask.nr5g_nsa_mask then -- NR5GNSA
        nr5g_nsa_band_pref = self:bits_or(nr5g_nsa_band_pref, mask.nr5g_nsa_mask)
      end
      if mask.nr5g_sa_mask then -- NR5GSA
        nr5g_sa_band_pref = self:bits_or(nr5g_sa_band_pref, mask.nr5g_sa_mask)
      end
    end
  end

  -- Set bits for extended LTE band preference
  local lte_band_pref_1_64, lte_band_pref_65_128 = {},{}
  local lte_band_pref_129_192, lte_band_pref_193_256 = {},{}
  for i,_ in pairs(lte_band_pref) do
    if i >= 0 and i < 64 then
      lte_band_pref_1_64[i] = true
    elseif i >= 64 and i < 128 then
      lte_band_pref_65_128[i-64] = true
    elseif i >= 128 and i < 192 then
      lte_band_pref_129_192[i-128] = true
    else
      lte_band_pref_193_256[i-192] = true
    end
  end

  local lte_band_pref_ext = {}
  lte_band_pref_ext.bits_1_64 = self:bits_to_mask(lte_band_pref_1_64)
  lte_band_pref_ext.bits_65_128 = self:bits_to_mask(lte_band_pref_65_128)
  lte_band_pref_ext.bits_129_192 = self:bits_to_mask(lte_band_pref_129_192)
  lte_band_pref_ext.bits_193_256 = self:bits_to_mask(lte_band_pref_193_256)

  self.l.log("LOG_DEBUG", string.format("bandPrefSet mode_pref=%x, band_pref=[%s], lte_band_pref=[%s]",
                          mode_pref, self:bits_to_string(band_pref), self:bits_to_string(lte_band_pref)))

  -- NR5G (deprecated v1.268)
  self:init_nr5g_band_pref_tbl()
  for i,_ in pairs(nr5g_band_pref) do
    local idx = math.floor(i/64)
    local elem = self.nr5g_band_pref_tbl[idx]
    if elem then
      elem.bits[i - (64*idx)] = true
    end
  end

  local nr5g_band_pref_arg = {}
  for i, elem in pairs(self.nr5g_band_pref_tbl) do
    self.l.log("LOG_DEBUG", string.format("bandPrefSet nr5g_band_pref_%s=[%s]", i, self:bits_to_string(elem.bits)))
    nr5g_band_pref_arg[elem.name] = self:bits_to_mask(elem.bits)
  end

  -- NR5G NSA
  self:init_nr5g_band_pref_tbl()
  for i,_ in pairs(nr5g_nsa_band_pref) do
    local idx = math.floor(i/64)
    local elem = self.nr5g_band_pref_tbl[idx]
    if elem then
      elem.bits[i - (64*idx)] = true
    end
  end

  local nr5g_nsa_band_pref_arg = {}
  for i, elem in pairs(self.nr5g_band_pref_tbl) do
    self.l.log("LOG_DEBUG", string.format("bandPrefSet nr5g_nsa_band_pref_%s=[%s]", i, self:bits_to_string(elem.bits)))
    nr5g_nsa_band_pref_arg[elem.name] = self:bits_to_mask(elem.bits)
  end

  -- NR5G SA
  self:init_nr5g_band_pref_tbl()
  for i,_ in pairs(nr5g_sa_band_pref) do
    local idx = math.floor(i/64)
    local elem = self.nr5g_band_pref_tbl[idx]
    if elem then
      elem.bits[i - (64*idx)] = true
    end
  end

  local nr5g_sa_band_pref_arg = {}
  for i, elem in pairs(self.nr5g_band_pref_tbl) do
    self.l.log("LOG_DEBUG", string.format("bandPrefSet nr5g_sa_band_pref_%s=[%s]", i, self:bits_to_string(elem.bits)))
    nr5g_sa_band_pref_arg[elem.name] = self:bits_to_mask(elem.bits)
  end

  -- Set preference
  local settings = {
    mode_pref = mode_pref,
    band_pref = self:bits_to_mask(band_pref),
    lte_band_pref_ext = self.radio_capability.is_lte_supported and lte_band_pref_ext or nil,
    -- Though NR5G Band Preference was deprecated v1.268, keep nr5g_band_pref for backward compatibility
    -- Do not include nr5g_band_pref TLV when NR5G NSA/SA TLV is not included
    nr5g_band_pref = self.radio_capability.is_nr5g_supported and not (nr5g_nsa_band_pref_arg or nr5g_sa_band_pref_arg)
                     and nr5g_band_pref_arg or nil,
    nr5g_nsa_band_pref = self.radio_capability.is_nr5g_supported and nr5g_nsa_band_pref_arg or nil,
    nr5g_sa_band_pref = self.radio_capability.is_nr5g_supported and nr5g_sa_band_pref_arg or nil,
  }

  -- Do not include mode_pref TLV if V_RAT_SEL == y in order to
  -- separate RAT control from band selection
#ifdef V_RAT_SEL_y
  settings.mode_pref = nil
#endif

  -- Try with extended LTE band preference TLV first. If it fails then try with duplicated LTE band preference TLV for old modems.
  succ,qerr,resp=self.luaq.req(self.m.QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE, settings)
  if succ then
    self.l.log("LOG_DEBUG", "QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE request with extended LTE band preference succeeded")
    return succ
  end

  -- Try LTE non-extended version
  self.l.log("LOG_ERR", "QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE request with extended LTE band preference failed")
  settings.lte_band_pref_ext = nil
  settings.lte_band_pref = self.radio_capability.is_lte_supported and self:bits_to_mask(lte_band_pref) or nil
  succ,qerr,resp=self.luaq.req(self.m.QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE, settings)
  if not succ then
    self.l.log("LOG_ERR", string.format("QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE request failed, error 0x%04x", qerr))
  else
    self.l.log("LOG_DEBUG", "QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE request succeeded")
  end

  return succ
end

-- Parse all of nr5g, nr5g-nsa and nr5g-sa band for backward compatibility but nr5g band TLV is not
-- sent if nr5g-nsa or nr5g-sa band is required to set.
function QmiNas:bandPrefSetHexMask(bands)
  local succ,qerr,resp
  if not bands then
    return false
  end

  -- Example: NR5G is for backward compatibility with old modem
  -- GSM:0x0000000000000000,WCDMA:0x100600000fc00000,LTE:0x420000a3e23b0f38df,,NR5G:0x00000000010f38df,NR5GNSA:0x00000000010f38df,NR5GSA:0x00000000010f38df
  self.l.log("LOG_DEBUG", string.format("bandPrefSetHexMask bands=[%s]", bands))

  local mode_pref, band_pref, lte_band_pref, lte_band_pref_ext, nr5g_band_pref_arg, nr5g_nsa_band_pref_arg, nr5g_sa_band_pref_arg = 0,{},{},{},{},{},{}
  local bandArray = bands:split(',')
  local bandPrefBitsTbl = {
          gsm     = {mode=0x04, bits={}},
          wcdma   = {mode=0x08, bits={}},
          lte     = {mode=0x10, bits={}},
          nr5g    = {mode=0x40, bits={}},
          nr5gnsa = {mode=0x40, bits={}},
          nr5gsa  = {mode=0x40, bits={}}
        }

  for _, band in ipairs(bandArray) do
    local name, mask = string.match(band, "^%s*(%w+)%s*:%s*(0[xX]%x+)")
    if name then
      name = string.lower(name)
      if bandPrefBitsTbl[name] then
        bandPrefBitsTbl[name].bits = self:hexmask_to_bits(mask)
        mode_pref = self.bit.bor(mode_pref, bandPrefBitsTbl[name].mode)
      end
    end
  end
  if bandPrefBitsTbl['gsm'] ~= nil then
    band_pref = self:bits_or(band_pref, bandPrefBitsTbl['gsm'].bits)
  end
  if bandPrefBitsTbl['wcdma'] ~= nil then
    band_pref = self:bits_or(band_pref, bandPrefBitsTbl['wcdma'].bits)
  end
  if bandPrefBitsTbl['lte'] ~= nil then
    lte_band_pref = self:bits_or(lte_band_pref, bandPrefBitsTbl['lte'].bits)
  end

  local lte_band_pref_tbl = {
    [0] = {name = "bits_1_64", bits = {}},
    [1] = {name = "bits_65_128", bits = {}},
    [2] = {name = "bits_129_192", bits = {}},
    [3] = {name = "bits_193_256", bits = {}},
  }

  if bandPrefBitsTbl['lte'] ~= nil then
    for i,_ in pairs(bandPrefBitsTbl['lte'].bits) do
      local idx = math.floor(i/64)
      local elem = lte_band_pref_tbl[idx]
      if elem then
        elem.bits[i - (64*idx)] = true
      end
    end

    for i, elem in pairs(lte_band_pref_tbl) do
      lte_band_pref_ext[elem.name] = self:bits_to_mask(elem.bits)
    end

    self.l.log("LOG_DEBUG", string.format("bandPrefSetHexMask mode_pref=%x, band_pref=[%s], lte_band_pref=[%s]",
                            mode_pref, self:bits_to_string(band_pref), self:bits_to_string(lte_band_pref)))
  end

  -- NR5G
  -- Deprecated in v1.268 but keep the logic to support the products with old modem
  if bandPrefBitsTbl['nr5g'] ~= nil then
    self:init_nr5g_band_pref_tbl()
    for i, elem in pairs(self.nr5g_band_pref_tbl) do
      nr5g_band_pref_arg[elem.name] = self:bits_to_mask(elem.bits)
    end
    for i,_ in pairs(bandPrefBitsTbl['nr5g'].bits) do
      local idx = math.floor(i/64)
      local elem = self.nr5g_band_pref_tbl[idx]
      if elem then
        elem.bits[i - (64*idx)] = true
      end
    end

    for i, elem in pairs(self.nr5g_band_pref_tbl) do
      self.l.log("LOG_DEBUG", string.format("bandPrefSetHexMask nr5g_band_pref_%s=[%s]", i, self:bits_to_string(elem.bits)))
      nr5g_band_pref_arg[elem.name] = self:bits_to_mask(elem.bits)
    end
  end

  -- NR5G NSA
  if bandPrefBitsTbl['nr5gnsa'] ~= nil then
    self:init_nr5g_band_pref_tbl()
    for i,_ in pairs(bandPrefBitsTbl['nr5gnsa'].bits) do
      local idx = math.floor(i/64)
      local elem = self.nr5g_band_pref_tbl[idx]
      if elem then
        elem.bits[i - (64*idx)] = true
      end
    end

    for i, elem in pairs(self.nr5g_band_pref_tbl) do
      self.l.log("LOG_DEBUG", string.format("bandPrefSetHexMask nr5g_nsa_band_pref_%s=[%s]", i, self:bits_to_string(elem.bits)))
      nr5g_nsa_band_pref_arg[elem.name] = self:bits_to_mask(elem.bits)
    end
  end

  -- NR5G SA
  if bandPrefBitsTbl['nr5gsa'] ~= nil then
    self:init_nr5g_band_pref_tbl()
    for i,_ in pairs(bandPrefBitsTbl['nr5gsa'].bits) do
      local idx = math.floor(i/64)
      local elem = self.nr5g_band_pref_tbl[idx]
      if elem then
        elem.bits[i - (64*idx)] = true
      end
    end

    for i, elem in pairs(self.nr5g_band_pref_tbl) do
      self.l.log("LOG_DEBUG", string.format("bandPrefSetHexMask nr5g_sa_band_pref_%s=[%s]", i, self:bits_to_string(elem.bits)))
      nr5g_sa_band_pref_arg[elem.name] = self:bits_to_mask(elem.bits)
    end
  end

  -- Set preference
  local settings = {
    mode_pref=mode_pref,
    band_pref=self:bits_to_mask(band_pref),
    lte_band_pref_ext=self.radio_capability.is_lte_supported and lte_band_pref_ext or nil,
    -- Do not include nr5g_band_pref TLV when NR5G NSA/SA TLV is not included
    nr5g_band_pref = self.radio_capability.is_nr5g_supported and not (nr5g_nsa_band_pref_arg or nr5g_sa_band_pref_arg)
                     and nr5g_band_pref_arg or nil,
    nr5g_nsa_band_pref = self.radio_capability.is_nr5g_supported and nr5g_nsa_band_pref_arg or nil,
    nr5g_sa_band_pref=self.radio_capability.is_nr5g_supported and nr5g_sa_band_pref_arg or nil,
  }

  -- Do not include mode_pref TLV if V_RAT_SEL == y in order to
  -- separate RAT control from band selection
#ifdef V_RAT_SEL_y
  settings.mode_pref = nil
#endif

  -- Try with extended LTE band preference TLV first. if fails then try with deplicated LTE band preference TLV for old modems.
   succ,qerr,resp=self.luaq.req(self.m.QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE, settings)
  if succ then
    self.l.log("LOG_DEBUG", "QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE request with extended LTE band preference succeeded")
    return succ
  end

  -- Try LTE non-extended version
  self.l.log("LOG_ERR", "QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE request with extended LTE band preference failed")
  settings.lte_band_pref_ext=nil
  settings.lte_band_pref = self.radio_capability.is_lte_supported and self:bits_to_mask(lte_band_pref) or nil
  succ,qerr,resp=self.luaq.req(self.m.QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE, settings)
  if not succ then
    self.l.log("LOG_ERR", "QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE request failed")
  else
    self.l.log("LOG_DEBUG", "QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE request succeeded")
  end
  return succ
end

-- Get Service domain preference
function QmiNas:serviceDomainPrefGet()
  local modem_operating_mode = self.wrdb:getp("operating_mode")
  if modem_operating_mode ~= "online" then
    return false
  end

  local succ,qerr,resp
  succ,qerr,resp = self.luaq.req(self.m.QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE)
  if not succ then
    self.l.log("LOG_ERR", "QMI_NAS_GET_SYSTEM_SELECTON_PREFERENCE request failed")
    return false
  end

  if self.luaq.is_c_true(resp.srv_domain_pref_valid) then
    local value = tonumber(resp.srv_domain_pref)
    if value then
      self.l.log("LOG_NOTICE", string.format("Current service domian preference: %s:%x",
        self.service_domain_names[value], value))
    end
    return value
  end

  return false
end

-- Set Service domain preference
function QmiNas:serviceDomainPrefSet(rdbKey, rdbVal)
  local srv_domain_pref = self.service_domain[rdbVal]
  if srv_domain_pref == nil then
    self.l.log("LOG_ERR", string.format("Invalid service domian preference given: %s", rdbVal))
    return false
  end
  self.l.log("LOG_NOTICE", string.format("Setting service domian preference to %s", rdbVal))

  local curr_srv_domain_pref = self:serviceDomainPrefGet()
  if curr_srv_domain_pref == srv_domain_pref then
    self.l.log("LOG_NOTICE", string.format("service domian preference already set to %s", rdbVal))
    return true
  end

  local succ,qerr,resp
  self.l.log("LOG_NOTICE", string.format("serviceDomainPrefSet(): service domain: %x", srv_domain_pref))
  succ,qerr,resp=self.luaq.req(self.m.QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE, {srv_domain_pref = srv_domain_pref})
  if succ then
    self.l.log("LOG_DEBUG", "QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE request service domain prefrence")
  else
    self.l.log("LOG_ERR", "QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE request with service domain preference failed")
  end

  return succ
end

-- Mode preference should be a semicolon-separated string with RAT names
-- ex) GSM;WCDMA;LTE;NR5G;
function QmiNas:modePrefSet(modes)
  local succ,qerr,resp
  if not modes then
    return false
  end
  self.l.log("LOG_DEBUG", "modePrefSet modes="..modes)
  local mode_pref = 0
  local modeArray=modes:split(';')
  for _,mode in ipairs(modeArray) do
    self.l.log("LOG_DEBUG", string.format("mode=%s", mode))
    if self.rat_to_mask[mode] and not self:is_hidden_band(mode) then
      mode_pref = self.bit.bor(mode_pref, self.rat_to_mask[mode])
    end
  end
  self.l.log("LOG_DEBUG", string.format("modePrefSet mode_pref=[0x%x]", mode_pref))

  -- Set preference
  local settings = {
    mode_pref = mode_pref
  }

  succ,qerr,resp=self.luaq.req(self.m.QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE, settings)
  if not succ then
    self.l.log("LOG_ERR", string.format("QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE request with mode pref failed, error 0x%04x", qerr))
  else
    self.l.log("LOG_DEBUG", "QMI_NAS_SET_SYSTEM_SELECTION_PREFERENCE request with mode pref succeeded")
  end

  return succ
end

function QmiNas:bandOps()
  local band = 0
  local cmd = luardb.get("wwan.0.currentband.cmd.command")
  local succ,qerr,resp
  succ = true
  luardb.set("wwan.0.currentband.cmd.status", "")
  if cmd == "set" or cmd == "set_hexmask" then
    -- firstly check whether the original band preference is saved. If it is not, save it before carrying on
    if luardb.get("wwan.0.module_bands_preference") == nil then
      local bands = self:bandPrefGet()
      if not bands then
        self.l.log("LOG_ERR", "Current band preference could not be read. Abandoned.")
        succ = false
      else
        luardb.set("wwan.0.module_bands_preference", bands, "p")
        succ = true
      end
    end
    -- Now it is safe to make change
    if succ then
      if cmd == "set_hexmask" then
        local v = luardb.get("wwan.0.currentband.cmd.param.band.hexmask")
        self.l.log("LOG_INFO", string.format("selected band mask = %s", v))
        succ = self:bandPrefSetHexMask(v)
        -- After setting bands with "set_hexmask" command,
        -- "wwan.0.currentband.current_selband.hexmask" should be explicitly updated
        -- via "get" command, as original "wwan.0.currentband.current_selband" did.
        -- Only this can update both of "wwan.0.currentband.current_selband"
        -- and "wwan.0.currentband.current_selband.hexmask" properly.
      else
        local v = luardb.get("wwan.0.currentband.cmd.param.band") -- expects a string of semicolon-separated band names or hex codes
        self.l.log("LOG_INFO", string.format("selected band = %s", v))
        succ = self:bandPrefSet(v)
      end
    end
  elseif cmd == "get" then
    local bandStr = self:bandPrefGet()
    if bandStr then
      self.l.log("LOG_INFO", "band = "..bandStr)
      luardb.set("wwan.0.currentband.current_selband", bandStr)
    else
      succ = false
    end
  end

  -- Set Mode Preference with band set command
#ifdef V_RAT_SEL_y
  if cmd == "set_rat" then
     -- expects a string of semicolon-separated RAT names
     -- ex) GSM;WCDMA;LTE;NR5G;
    local v = luardb.get("wwan.0.currentband.cmd.param.rat")
    self.l.log("LOG_INFO", string.format("selected RAT = %s", v))
    succ = self:modePrefSet(v)
  end
#endif

  local result = succ and "[done]" or "[error]"
  self.l.log("LOG_INFO", "bandops: result " ..result)
  luardb.set("wwan.0.currentband.cmd.status", result)
end

-- Use standard command structure, wwan.0.plmn.cmd.command
-- Keep old command for backward compatibility
function QmiNas:plmnOps(cmd_rdb, cmd)
  self.l.log("LOG_DEBUG",string.format("PLMN command '%s' triggered",cmd))
  local succ

  -- [scan] result stored in RDB PLMN_list
  -- "carrier,mcc,mnc,status,rat&..."
  -- status = 1 available, 4 current, 2 forbidden, 0 unknown
  -- rat = 0 unknown, 1 GSM, 2 GSM Compact, 3 GSM/EGPRS, 4 UMTS/HSDPA, 5 UMTS/HSUPA, 6 HSDPA+HSUPA, 7 UMTS, 8 DC-HSPA+, 9 LTE, 12 NR5G

  -- Old command set uses numeric RDB command for historical reason
  if cmd == "1" then
    succ = self:perform_async_manual_plmn_scan()
  -- Use new command for incremental scan
  elseif cmd == "get" then
    succ = self:perform_async_incremental_manual_plmn_scan()

  -- [select] RDB PLMN_select contains parameter
  -- "0" : auto plmn selection
  -- "rat,mnc,mcc" : manual plmn selection where rat=0 means no rat preference, otherwise it has the same meaning as in PLMN_list above.
  elseif cmd == "5" then
    local plmn_sel = self.wrdb:getp("PLMN_select")
    succ = self:perform_plmn_select(plmn_sel)
  elseif cmd == "set" then
    local plmn_sel = self.wrdb:getp("plmn.cmd.param.mode")
    succ = self:perform_plmn_select(plmn_sel)
  else
    self.l.log("LOG_WARNING",string.format("Unknown PLMN command %s",cmd))
    succ = false
  end
  -- Invoke poll_network_scan_mode to update current selection mode
  -- after set command
  if cmd == "set" and succ then
    self.watcher.invoke("sys","poll_network_scan_mode")
  end
  -- Set command result to RDB immediately for PLMN select command or
  -- when manual scan (get) command failed locally
  if cmd == "5" or (cmd == "1" and not succ) then
    self.l.log("LOG_DEBUG",string.format("PLMN command '%s' status %s", cmd,
      succ and "[done]" or "[error]"))
    self.wrdb:setp("PLMN.cmd.status", succ and "[done]" or "[error]")
  elseif cmd == "set" or (cmd == "get" and not succ) then
    self.l.log("LOG_DEBUG",string.format("PLMN command '%s' status %s", cmd,
      succ and "[done]" or "[error]"))
    self.wrdb:setp("plmn.cmd.status", succ and "[done]" or "[error]")
  else
    self.l.log("LOG_DEBUG", "waiting for PLMN scan command complete...")
  end
end

function QmiNas:cellLockOps()
  local lock_rat = luardb.get("wwan.0.cell_lock.cmd.param.rat")
  local cmd = luardb.get("wwan.0.cell_lock.cmd.command")
  local succ,qerr,resp
  local rdbKey = "wwan.0.cell_lock.cmd.param.lock_list"
  luardb.set("wwan.0.cell_lock.cmd.status", "")
  if lock_rat == "lte" then    -- 4G PCI lock
    if cmd == "set" then
      local rdbVal = luardb.get(rdbKey) or ""
      self:set_dynamic_cell_lock(rdbKey, rdbVal)
    elseif cmd == "get" then
      local succ, locksets = self:get_dynamic_cell_lock(nil, nil)
      if succ then
        luardb.set("wwan.0.cell_lock.cmd.status", "[done]")
      else
        luardb.set("wwan.0.cell_lock.cmd.status", "[error]")
      end
    end
#ifdef V_NR5G_CELL_LOCK_y
  elseif lock_rat == "5g" then --5G PCI lock
    if cmd == "set" then
      succ = self:set_dynamic_cell_lock_5G()
    elseif cmd == "get" then
      succ = self:get_dynamic_cell_lock_5G()
    end

    if succ then
      luardb.set("wwan.0.cell_lock.cmd.status", "[done]")
    else
      luardb.set("wwan.0.cell_lock.cmd.status", "[error]")
    end
#endif
  else
    self.l.log("LOG_ERR",string.format("cellLockOps: RAT %s not supported, cellLockOps failed", lock_rat))
    luardb.set("wwan.0.cell_lock.cmd.status", "[error]")
  end

  luardb.set("wwan.0.cell_lock.cmd.param.rat", "")
end

-- check if (eci, pci, earfcn) meets lockmode
-- return true for compliant; false for non-compliant along with locksets
function QmiNas:isCellCompliant(eci, pci, earfcn)
  local locksets = self.util.parse_lockmode(self.wrdb:getp("lockmode"))
  if not locksets or #locksets == 0 then
    return true
  end
  for _,entry in ipairs(locksets) do
    if (not entry.eci or entry.eci==eci) and (not entry.pci or entry.pci==pci) and (not entry.earfcn or entry.earfcn==earfcn) then
      self.l.log("LOG_INFO", string.format("lockmode condition is met: eci=%s, pci=%s, earfcn=%s",entry.eci,entry.pci,entry.earfcn))
      return true
    end
  end
  self.l.log("LOG_NOTICE", "lockmode condition is not met")
  return false, locksets
end


function QmiNas:QMI_NAS_GET_RF_BAND_INFO(type, event, qm)
  self.l.log("LOG_DEBUG","QMI_NAS_GET_RF_BAND_INFO received")

  local resp = qm.resp
  local succ = tonumber(resp.resp.result) == 0

  local ia={}

  if not succ then
    self.l.log("LOG_DEBUG","no band information available")
  elseif resp.rf_band_info_list_len<=0 then
    self.l.log("LOG_DEBUG","no band info list available")
  else
    -- Check if extended format exists for LTE/5G which supports
    -- 32 bits channel number then update the channel info.
    local rf_band_info = resp.rf_band_info_list[0]
    local ext_rf_band_info = nil
    local rf_bandwidth_info = nil
    if self.luaq.is_c_member_and_true(resp, "rf_band_info_list_ext_valid") and resp.rf_band_info_list_ext_len > 0 then
      ext_rf_band_info = resp.rf_band_info_list_ext[0]
    end
    if self.luaq.is_c_member_and_true(resp, "nas_rf_bandwidth_info_valid") and resp.nas_rf_bandwidth_info_len > 0 then
      rf_bandwidth_info = resp.nas_rf_bandwidth_info[0]
    end
    ia=self:build_rf_info_arg(rf_band_info, ext_rf_band_info, rf_bandwidth_info)
  end

  return self.watcher.invoke("sys","modem_on_rf_band_info",ia)
end

function QmiNas:QMI_NAS_GET_SIG_INFO(type, event, qm)
  self.l.log("LOG_DEBUG","QMI_NAS_GET_SIG_INFO received")

  local resp = qm.resp

  -- check qmi result
  if tonumber(resp.resp.result) ~= 0 then
    self.l.log("LOG_DEBUG","qmi result failure found in QMI_NAS_GET_SIG_INFO")
    return
  end

  local ia=self:build_sig_info_arg(resp)

  return self.watcher.invoke("sys","modem_on_signal_info",ia)
end

-- record manual scan result to RDB
-- @params newCmd A indicator for new plmn command set
function QmiNas:set_plmn_scan_result(newCmd, succ)
  if self.manual_plmn_scan_mode then
    self.l.log("LOG_DEBUG", string.format("record manual scan result %s to RDB", succ))
    self.wrdb:setp(string.format("%s.cmd.status", newCmd and "plmn" or "PLMN"), succ and "[done]" or "[error]")
    self.manual_plmn_scan_mode = false
  end
end

function QmiNas:QMI_NAS_PERFORM_NETWORK_SCAN(type, event, qm)
  self.l.log("LOG_DEBUG","QMI_NAS_PERFORM_NETWORK_SCAN received")

  local resp = qm.resp

  -- check qmi result
  if tonumber(resp.resp.result) ~= 0 then
    self.l.log("LOG_DEBUG","qmi result failure found in QMI_NAS_PERFORM_NETWORK_SCAN")
    self:set_plmn_scan_result(false, false)
    return
  end

  if self.luaq.is_c_true(resp.pci_plmn_info_valid) then
    -- for pci scan we do not check validation of scan result as
    -- resp.scan_result_valid is not set
    local ia=self:build_pci_list_arg(qm.resp)
    return self.watcher.invoke("sys","modem_on_pci_list",ia)
  else
    -- check validation of scan result
    if not self.luaq.is_c_true(resp.scan_result_valid) then
      self.l.log("LOG_DEBUG","no scan result code found in QMI_NAS_PERFORM_NETWORK_SCAN ")
      self:set_plmn_scan_result(false, false)
      return
    end

    -- check scan result
    local scan_result = tonumber(resp.scan_result)
    if  scan_result ~= 0 then
      self.l.log("LOG_DEBUG", string.format("scan failure result code found in QMI QMI_NAS_PERFORM_NETWORK_SCAN ",scan_result))
      self:set_plmn_scan_result(false, false)
      return
    end

    -- build ia
    self.l.log("LOG_DEBUG","got success scan result in QMI_NAS_PERFORM_NETWORK_SCAN ")
    local ia=self:build_plmn_list_arg(qm.resp)
    self.watcher.invoke("sys", "modem_on_plmn_list", ia)
    self:set_plmn_scan_result(false, true)
  end
  return true
end

-- If the Network Scan Status TLV in the indication returns NAS_SCAN_STATUS_COMPLETE,
-- it is treated as if the scan completed successfully, and no subsequent indications
-- follow. If the Network Scan Status TLV in the indication returns NAS_SCAN_STATUS_PARTIAL,
-- subsequent indications follow. If the Network Scan Status TLV in the indication
-- returns NAS_SCAN_STATUS_ABORT, it indicates that the scan is terminated and no
-- available network is found.
function QmiNas:QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN_IND(type, event, qm)
  self.l.log("LOG_DEBUG","QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN_IND received")
  local resp = qm.resp

  -- check scan result
  local scan_status = tonumber(resp.scan_status)
  if  scan_status ~= 0x00 and  -- NAS_SCAN_STATUS_COMPLETE_V01
      scan_status ~= 0x01 and  -- NAS_SCAN_STATUS_PARTIAL_V01
      scan_status ~= 0x05 then -- NAS_SCAN_STATUS_PARTIAL_PERIODIC_V01
    self.l.log("LOG_DEBUG", string.format("scan failure result code %d found in QMI QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN_IND",scan_status))
    self:set_plmn_scan_result(true, false)
    return
  end

  if self.luaq.is_c_true(resp.nas_network_scan_info_valid) then
    self:build_network_scan_arg(qm.resp)
  end

  -- Update PLMN lists and set result if got complete information
  if scan_status == 0x00 then
    self.l.log("LOG_DEBUG","got complete scan information in QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN ")
    self.watcher.invoke("sys", "modem_on_plmn_list", self.network_scan_info)
    self:set_plmn_scan_result(true, true)
  else
    self.l.log("LOG_DEBUG","got partial scan information in QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN, keep waiting next indication")
  end
  return true
end

function QmiNas:QMI_NAS_OPERATOR_NAME_DATA_IND(type, event, qm)
  self.l.log("LOG_DEBUG","QMI_NAS_OPERATOR_NAME_DATA_IND received")

  return true
end

function QmiNas:QMI_NAS_CURRENT_PLMN_NAME_IND(type, event, qm)
  self.l.log("LOG_INFO","QMI_NAS_CURRENT_PLMN_NAME_IND received")
  local ia = {}
  local resp = qm.resp
  local mccmnc
  if self.luaq.is_c_true(resp.plmn_id_valid) then
    mccmnc = string.format(self.luaq.is_c_true(resp.plmn_id.mnc_includes_pcs_digit) and "%03d%03d" or "%03d%02d", resp.plmn_id.mcc, resp.plmn_id.mnc)
  end
  --[[
      TLV spn is marked 'deprecated' since 1.117 in favour of spn_ext.
      TODO: Add support of spn_ext if spn_valid is false.
  --]]
  if self.luaq.is_c_true(resp.spn_valid) then
    if resp.spn.spn_enc ~= "NAS_CODING_SCHEME_CELL_BROADCAST_GSM_V01" then
      self.l.log("LOG_WARNING", string.format("current SPN: unknown encoding: %d", tonumber(resp.spn.spn_enc)))
      ia.spn = ""
    else
      ia.spn = self.ffi.string(resp.spn.spn, resp.spn.spn_len)
      self.l.log("LOG_INFO", string.format("current SPN: %s", ia.spn))
    end
  end
  if self.luaq.is_c_true(resp.short_name_valid) then
    if resp.short_name.plmn_name_enc ~= "NAS_CODING_SCHEME_CELL_BROADCAST_GSM_V01" then
      self.l.log("LOG_WARNING", string.format("current plmn short name: unknown encoding: %d", tonumber(resp.short_name.plmn_name_enc)))
      ia.short_name = ""
    else
      ia.short_name = self.ffi.string(resp.short_name.plmn_name, resp.short_name.plmn_name_len)
      if resp.short_name.plmn_name_ci == "NAS_COUNTRY_INITIALS_ADD_V01" then
        ia.short_name = self:get_country_initial(mccmnc) .. ia.short_name
      end
      self.l.log("LOG_INFO", string.format("current plmn short name: %s", ia.short_name))
    end
  end
  if self.luaq.is_c_true(resp.long_name_valid) then
    if resp.long_name.plmn_name_enc ~= "NAS_CODING_SCHEME_CELL_BROADCAST_GSM_V01" then
      self.l.log("LOG_WARNING", string.format("current plmn long name: unknown encoding: %d", tonumber(resp.long_name.plmn_name_enc)))
      ia.long_name = ""
    else
      ia.long_name = self.ffi.string(resp.long_name.plmn_name, resp.long_name.plmn_name_len)
      if resp.long_name.plmn_name_ci == "NAS_COUNTRY_INITIALS_ADD_V01" then
        ia.long_name = self:get_country_initial(mccmnc) .. ia.long_name
      end
      self.l.log("LOG_INFO", string.format("current plmn long name: %s", ia.long_name))
    end
  end
  if self.luaq.is_c_true(resp.nw_name_source_valid) then
    ia.name_source = self.nw_name_source_names[tonumber(resp.nw_name_source)]
    self.l.log("LOG_INFO", string.format("nw name source: %s", ia.name_source))
  end
  return self.watcher.invoke("sys", "modem_on_operator_name", ia)
end

function QmiNas:QMI_NAS_NETWORK_TIME_IND(type, event, qm)

  local resp=qm.resp
  local time_zone = self.luaq.is_c_true(resp.time_zone_valid) and resp.time_zone or nil
  local daylt_sav_adj = self.luaq.is_c_true(resp.daylt_sav_adj_valid) and resp.daylt_sav_adj or 0

  local ia = self:build_network_time_arg(resp.universal_time,time_zone,daylt_sav_adj)
  return self.watcher.invoke("sys","modem_on_network_time",ia)
end

function QmiNas:QMI_NAS_SIG_INFO_IND(type, event, qm)
  self.l.log("LOG_DEBUG","QMI_NAS_SIG_INFO_IND received")
  local ia=self:build_sig_info_arg(qm.resp)
  return self.watcher.invoke("sys","modem_on_signal_info",ia)
end

function QmiNas:QMI_NAS_SERVING_SYSTEM_IND(type, event, qm)

  local ia = self:build_modem_network_status_arg(qm.resp)

  self.l.log("LOG_INFO",string.format("QMI_NAS_SERVING_SYSTEM_IND received (reg=%s,ps=%s)",ia.reg_state,ia.ps_attach_state))

  return self.watcher.invoke("sys","modem_on_network_state",ia)
end

function QmiNas:QMI_NAS_SYS_INFO_IND(type, event, qm)
  self.l.log("LOG_DEBUG","QMI_NAS_SYS_INFO_IND received")

  local ia = self:build_ext_modem_network_status_arg(qm.resp)

  return self.watcher.invoke("sys","modem_on_ext_network_state",ia)
end

function QmiNas:QMI_NAS_RF_BAND_INFO_IND(type,event,qm)
  self.l.log("LOG_DEBUG","QMI_NAS_RF_BAND_INFO_IND received")
  local ia={}

  -- Check if extended format exists for LTE/5G which supports
  -- 32 bits channel number then update the channel info.
  local rf_band_info = qm.resp.rf_band_info
  local ext_rf_band_info = nil
  local rf_bandwidth_info = nil
  if self.luaq.is_c_member_and_true(qm.resp, "rf_band_info_list_ext_valid") then
    ext_rf_band_info = qm.resp.rf_band_info_list_ext
  end
  if self.luaq.is_c_member_and_true(qm.resp, "nas_rf_bandwidth_info_valid") then
    rf_bandwidth_info = qm.resp.nas_rf_bandwidth_info
  end
  ia=self:build_rf_info_arg(rf_band_info, ext_rf_band_info, rf_bandwidth_info)

  return self.watcher.invoke("sys","modem_on_rf_band_info",ia)
end

function QmiNas:QMI_NAS_LTE_CPHY_CA_IND(type,event,qm)
  local ia=self:build_lte_cphy_ca_ind_arg(qm.resp)

  return self.watcher.invoke("sys","modem_on_lte_cphy_ca_ind",ia)
end

-- if the cell measurement was updated, increase the cnt
local cell_measurement_cnt = 0

local neighborCellData =
{
  startTime  = 0;  -- start time of 15 minute cycle
  finishTime = 0;  -- finish time
  i          = 0;  -- index points to next cell
  isActive   = {}; -- flag, indicates cell is active, per cell
  samples    = {}; -- sample counter, per cell
  earfcn     = {}; -- Frequency, per cell
  bw         = {}; -- Bandwidth, per cell
  pci        = {}; -- Physical Cell ID, per cell
  rsrp       = {}; -- last received signal power, per cell
  rsrq       = {}; -- last received signal quality, per cell
  rsrp_sum   = {}; -- accumulated received signal power, per cell
  rsrq_sum   = {}; -- accumulated received signal quality, per cell
}

-- A string that maps frequencies to bandwidth.
local freqBandwidthTable


--[[
********************************************************************************
* getBandwidth - determines the bandwidth for a given frequency. The input
*                string "table" contains the mappings of frequencies to
*                bandwidths.
*
*                A sample of the string is:
*
*                9820:50,2150:15,3350:75,  (the trailing comma is intentional)
******************************************************************************** ]]
function QmiNas:getBandwidth(freq,table)
  local bw = "0"

  if table then
    bw = string.match( table, tostring(freq) .. ":(%d+),") or "0"
  end

  return bw
end




--[[
****************************************************************************************************************
* record_neighbor_cell_data
* updated per MAG-1345:
* - All cells should be tracked within the measurement period (server and neighbor alike)
* - Only readings where RSRQ is >-20 shall be considered
* - When a new measurement for a PCI is received, and is Valid, then (in addition to other processing),
*   a counter shall be incremented for that PCI/EARFCN tuple.
* - At the end of the measurement period, the Neighbor History List shall be populated
*   with cells that meet the following criteria:
*   - EARFCN/PCI is not the serving cell (at the end of the measurement period)
*   - EARFCN/PCI tuple has 2 or more samples.
* - The most recent reading (RSRP, RSRQ, et al) shall be what is entered into the History List for that interval
*
* After the Neighbor History report is generated, the counters shall be cleared for the next measurement interval.
*
* See RDB DOC wwan.rdbd for definition of RDB vars used by this function.
**************************************************************************************************************** ]]
function QmiNas:record_neighbor_cell_data( isUpdateComplete, time, earfcn, pci, rsrp, rsrq )
  -- to allow debug log message to turn on/off dynamically just for this function
  local logdb = function()
    local debug = self.wrdb:getp("cell_measurement.ncell.debug")
    return debug == '1' and "LOG_NOTICE" or "LOG_DEBUG"
  end

  ---------------------------------------------------------------------------
  -- Accumulate data (for one cell)
  ---------------------------------------------------------------------------
  local accumulate = function(earfcn, pci, rsrp, rsrq)
    if neighborCellData.startTime == 0 then
      -- Compute averaging cycle start & finish times
      neighborCellData.startTime = time
      local duration = tonumber(self.wrdb:getp("cell_measurement.ncell.duration")) or 15
      neighborCellData.finishTime = neighborCellData.startTime + duration * 60;
      self.l.log(logdb(), string.format("RNCD: AVERAGING CYCLE STARTS!, duration = %s minutes", duration ))
    end

    if( neighborCellData.i == 0 ) then
      self.l.log(logdb(), string.format("RNCD: ZERO ACTIVE ARRAY..." ))

      for k,v in pairs(neighborCellData.isActive) do
        neighborCellData.isActive[k] = false
      end

      freqBandwidthTable = luardb.get("wwan.0.cell_measurement.freq_to_bandwidth")

      if freqBandwidthTable then
        -- include serving cell bandwidth so samples will store serving cell data too,
        -- They will be used if serving cell changes during 15min averaging cycle.
        local servBw = luardb.get("wwan.0.radio_stack.e_utra_measurement_report.dl_bandwidth")
        local servEarfcn = tonumber(luardb.get("wwan.0.system_network_status.channel"))
        freqBandwidthTable = freqBandwidthTable .. string.format("%s:%s,", servBw, servEarfcn)
      end
    end

    local key = tostring(earfcn) .. ":" .. tostring(pci)  -- Use this key to index the arrays

    -----------------------------------------------------------------------
    -- Validate cell data, not to include invalid cell data
    -----------------------------------------------------------------------
    local bw = self:getBandwidth(earfcn, freqBandwidthTable)
    self.l.log(logdb(), string.format("RNCD: [%s] %d %d %.1f %.1f %s", key, earfcn, pci, rsrp, rsrq, bw))
    local min_rsrq = tonumber(self.wrdb:getp("cell_measurement.ncell.min_rsrq")) or -20
    if rsrp == 0 or rsrq < min_rsrq or bw == "0" then
      self.l.log(logdb(), string.format("RNCD: ignore invalid cell data earfcn:%s, pci:%s, rsrp:%s, rsrq:%s, bw:%s",
        earfcn, pci, rsrp, rsrq, bw))
      return
    end

    -----------------------------------------------------------------------
    -- Update cell data
    -----------------------------------------------------------------------
    neighborCellData.isActive[key] = true
    neighborCellData.earfcn[key]   = earfcn
    neighborCellData.bw[key]       = bw
    neighborCellData.pci[key]      = pci

    -----------------------------------------------------------------------
    -- This block sums rsrp, rsrq, and sample count. It also handles the
    -- special case where a new cell appears, by initializing the rsrp,rsrq,
    -- and sample count for the first sample.
    -----------------------------------------------------------------------
    if( neighborCellData.samples[key] ~= nil ) then
      neighborCellData.samples[key] = neighborCellData.samples[key] + 1
      neighborCellData.rsrp_sum[key] = neighborCellData.rsrp_sum[key] + rsrp
      neighborCellData.rsrq_sum[key] = neighborCellData.rsrq_sum[key] + rsrq
    else
      neighborCellData.samples[key]= 1
      neighborCellData.rsrp_sum[key] = rsrp
      neighborCellData.rsrq_sum[key] = rsrq
    end
    neighborCellData.rsrp[key] = rsrp
    neighborCellData.rsrq[key] = rsrq
    neighborCellData.i = neighborCellData.i + 1
  end

  if not isUpdateComplete then accumulate(earfcn, pci, rsrp, rsrq) return end

  ---------------------------------------------------------------------------
  -- End of an accumulation cycle (10 secs)
  ---------------------------------------------------------------------------
  neighborCellData.i = 0

  -------------------------------------------------------------------
  -- serialize Neighbor Cell data
  -------------------------------------------------------------------
  local serialize = function(dataset)
    local isCurrent = dataset == "current"
    local cellCount = 0
    local sampleCount = 0
    local measurement = ""
    local servEarfcn = tonumber(luardb.get("wwan.0.system_network_status.channel"))
    local servPci = tonumber(luardb.get("wwan.0.system_network_status.PCID"))
    local min_sample = tonumber(self.wrdb:getp("cell_measurement.ncell.min_sample")) or 2
    for k,v in pairs(neighborCellData.isActive) do
      local samples  = neighborCellData.samples[k]
      local earfcn   = neighborCellData.earfcn[k]
      local rsrp     = isCurrent and neighborCellData.rsrp[k] or neighborCellData.rsrp_sum[k]
      local rsrq     = isCurrent and neighborCellData.rsrq[k] or neighborCellData.rsrq_sum[k]
      local pci      = neighborCellData.pci[k]
      local bw       = neighborCellData.bw[k]
      local n        = isCurrent and 1 or samples
      self.l.log(logdb(), string.format("RNCD: [%s] isActive:%s samples:%s", k, v, samples))
      -----------------------------------------------------------------------
      -- Validate each cell data before serializing
      -----------------------------------------------------------------------
      if bw == "0" or rsrp == 0 or rsrq == 0 or samples < min_sample then
        self.l.log(logdb(),
          string.format("RNCD: skip invalid cell earfcn:%s, pci:%s, rsrp:%s, rsrq:%s, bw:%s, samples:%s",
            earfcn, pci, rsrp, rsrq, bw, samples))
      elseif earfcn == servEarfcn and pci == servPci then
        self.l.log(logdb(), string.format("RNCD: skip serving cell earfcn:%s pci:%s bw:%s rsrp:%.1f rsrq:%.1f",
          earfcn, pci, bw, rsrp / n, rsrq / n))
      else
        cellCount = cellCount + 1
        sampleCount = sampleCount + samples
        local sep = measurement ~= "" and "|" or ""
        measurement = measurement .. string.format("%s%d,%d,%d,%.1f,%.1f", sep, earfcn, pci, bw, rsrp / n, rsrq / n)
      end
    end
    return measurement, cellCount, sampleCount
  end

  local current,_,__ = serialize("current")
  if self.wrdb:getp("cell_measurement.ncell.current") ~= current then
    self.wrdb:setp("cell_measurement.ncell.current", current)
  end

  if time < neighborCellData.finishTime then return end
  -----------------------------------------------------------------------
  -- End of averaging cycle
  -----------------------------------------------------------------------
  local average, cellCount, sampleCount = serialize("average")
  self.l.log(logdb(), string.format("RNCD: END OF CYCLE PROCESSING!!" ))
  self.l.log(logdb(), string.format("RNCD:"))
  self.l.log(logdb(), string.format("RNCD: cellCount=%s, sampleCount=%s", cellCount, sampleCount))
  self.l.log(logdb(), string.format("RNCD: cell_measurement.ncell.data       = %s", current))
  self.l.log(logdb(), string.format("RNCD: cell_measurement.ncell.average    = %s", average))
  self.l.log(logdb(), string.format("RNCD: cell_measurement.ncell.start_time = %s", os.date( "!%H:%M:%S", neighborCellData.startTime ) ))
  self.l.log(logdb(), string.format("RNCD:"))
  -------------------------------------------------------------------
  -- Update RDB variables
  -------------------------------------------------------------------
  self.wrdb:setp( "cell_measurement.ncell.data", current)
  self.wrdb:setp( "cell_measurement.ncell.average", average)
  self.wrdb:setp( "cell_measurement.ncell.start_time", os.date( "!%Y-%m-%dT%H:%M:%SZ", neighborCellData.startTime ))

  neighborCellData.startTime = 0

  -- prune inactive cells
  for k,v in pairs(neighborCellData.isActive) do
    neighborCellData.samples[k] = nil
    if( not neighborCellData.isActive[k] ) then
      self.l.log(logdb(), string.format("RNCD: PRUNING [%s]", k ))
      neighborCellData.isActive[k] = nil
      neighborCellData.earfcn[k]   = nil
      neighborCellData.bw[k]       = nil
      neighborCellData.pci[k]      = nil
      neighborCellData.rsrp[k]     = nil
      neighborCellData.rsrq[k]     = nil
    end
  end

  -- the last active sample becomes the first sample of next averaging cycle
  for k,v in pairs(neighborCellData.isActive) do
    accumulate(neighborCellData.earfcn[k], neighborCellData.pci[k], neighborCellData.rsrp[k], neighborCellData.rsrq[k])
  end
end

function QmiNas:QMI_NAS_GET_CELL_LOCATION_INFO(type,event,qm)
  self.l.log("LOG_DEBUG","QMI_NAS_GET_CELL_LOCATION_INFO received")

  -- current list of cell measurement information
  local cell_measurements={}
  local cell_list={}

  local resp = qm.resp

  -- check qmi result
  local succ = tonumber(resp.resp.result) == 0

  local n =0

  if succ then

    local function post_cell_info_to_rdb(type,channel,cell_id,ss,sq)
      local rec, cell_elem
      rec = string.format("%s,%d,%d", type, tonumber(channel), cell_id)
      self.wrdb:setp(string.format("cell_measurement.%d",n),string.format("%s,%d,%d,%0.1f,%0.1f", type, tonumber(channel), cell_id, ss, sq))

      table.insert(cell_measurements,rec)
      cell_elem = string.format("%s:%d:%d", type, tonumber(channel), cell_id)
      table.insert(cell_list, cell_elem)
      n=n+1

      self.l.log("LOG_DEBUG",string.format("cell_info#%d: %s,%d,%d,%0.1f,%0.1f", #cell_measurements, type, tonumber(channel), cell_id, ss, sq))
    end

    local time            = os.time()

    if self.luaq.is_c_true(resp.lte_intra_valid) then
      local lte_intra = resp.lte_intra
      -- TLV 0x27 (LTE Info Extended - Intrafrequency EARFCN)can be included
      -- for 32 bits EARFCN for LTE/5G then should use this instead of 16 bits EARFCN.
      local lte_intra_earfcn = lte_intra.earfcn
      if self.luaq.is_c_true(resp.lte_intra_earfcn_valid) then
        lte_intra_earfcn = resp.lte_intra_earfcn
      end
      for i=0,lte_intra.cells_len-1 do
        self:record_neighbor_cell_data( false,
                        time,
                        lte_intra_earfcn,
                        lte_intra.cells[i].pci,
                        lte_intra.cells[i].rsrp/10,
                        lte_intra.cells[i].rsrq/10 )
        post_cell_info_to_rdb(
          'E',
          lte_intra_earfcn,
          lte_intra.cells[i].pci,
          lte_intra.cells[i].rsrp/10,
          lte_intra.cells[i].rsrq/10
        )
      end

      self.wrdb:setp("system_network_status.PCID", tonumber(lte_intra.serving_cell_id));
      self.wrdb:setp("cell_measurement.serving_system", "LTE");
      self.wrdb:setp("system_network_status.eci_pci_earfcn", string.format("%d,%d,%d",lte_intra.global_cell_id,lte_intra.serving_cell_id,lte_intra_earfcn))

      -- check if cell locking condition is met
      local compliant, locksets = self:isCellCompliant(lte_intra.global_cell_id,lte_intra.serving_cell_id,lte_intra_earfcn)
      if not compliant then
        self.l.log("LOG_NOTICE", "lock mode condition is violated. We are going to detach")
        self.watcher.invoke("sys", "update_cell_lock", locksets)
      end
    end

    if self.luaq.is_c_true(resp.lte_inter_valid) then
      local lte_inter = resp.lte_inter

      -- TLV 0x28 (LTE Info Extended - Interrequency EARFCN)can be included
      -- for 32 bits EARFCN for LTE/5G then should use this instead of 16 bits EARFCN.
      for i=0,lte_inter.freqs_len-1 do
        local inter_earfcn = lte_inter.freqs[i].earfcn
        if self.luaq.is_c_true(resp.lte_inter_earfcn_valid) and i <= resp.lte_inter_earfcn_len-1 then
          inter_earfcn = resp.lte_inter_earfcn[i]
        end
        for j=0,lte_inter.freqs[i].cells_len-1 do
          self:record_neighbor_cell_data( false,
                          time,
                          inter_earfcn,
                          lte_inter.freqs[i].cells[j].pci,
                          lte_inter.freqs[i].cells[j].rsrp/10,
                          lte_inter.freqs[i].cells[j].rsrq/10 )
          post_cell_info_to_rdb(
            'E',
            inter_earfcn,
            lte_inter.freqs[i].cells[j].pci,
            lte_inter.freqs[i].cells[j].rsrp/10,
            lte_inter.freqs[i].cells[j].rsrq/10
          );
        end
      end
    end

    self:record_neighbor_cell_data( true, time)  -- End Neighbor Cell accumulation cycle


    if self.luaq.is_c_true(resp.lte_wcdma_valid) then
      local lte_wcdma = resp.lte_wcdma
      for i=0,lte_wcdma.freqs_len-1 do
        for j=0,lte_wcdma.freqs[i].cells_len-1 do
          post_cell_info_to_rdb(
            'U',
            lte_wcdma.freqs[i].uarfcn,
            lte_wcdma.freqs[i].cells[j].psc,
            lte_wcdma.freqs[i].cells[j].cpich_rscp/10,
            lte_wcdma.freqs[i].cells[j].cpich_ecno/10);
        end
      end
    end

    if self.luaq.is_c_true(resp.lte_gsm_valid) then
      local lte_gsm = resp.lte_gsm
      for i=0,lte_gsm.freqs_len-1 do
        for j=0,lte_gsm.freqs[i].cells_len-1 do
          post_cell_info_to_rdb(
            'G',
            lte_gsm.freqs[i].cells[j].arfcn,
            lte_gsm.freqs[i].cells[j].bsic_id,
            lte_gsm.freqs[i].cells[j].rssi/10,
            0);
        end
      end
    end

    if self.luaq.is_c_true(resp.umts_info_valid) then
      local umts_info = resp.umts_info
      self.wrdb:setp("cell_measurement.rscp", umts_info.rscp);
      for i=0,umts_info.umts_monitored_cell_len-1 do
        post_cell_info_to_rdb(
          'U',
          umts_info.umts_monitored_cell[i].umts_uarfcn,
          umts_info.umts_monitored_cell[i].umts_psc,
          umts_info.umts_monitored_cell[i].umts_rscp,
          umts_info.umts_monitored_cell[i].umts_ecio
        )
      end

      for i=0,umts_info.umts_geran_nbr_cell_len-1 do
        post_cell_info_to_rdb(
          'G',
          umts_info.umts_geran_nbr_cell[i].geran_arfcn,
          0,
          umts_info.umts_geran_nbr_cell[i].geran_rssi,
          0
        );
      end

      self.wrdb:setp("cell_measurement.serving_system", "UMTS");
    end

    if self.luaq.is_c_true(resp.wcdma_lte_valid) then
      local wcdma_lte = resp.wcdma_lte
      local wcdma_rrc_state_names={
        [0x00] = "DISCONNECTED",
        [0x01] = "CELL_PCH",
        [0x02] = "URA_PCH",
        [0x03] = "CELL_FACH",
        [0x04] = "CELL_DCH",
      }
      local wcdma_rrc_state = tonumber(wcdma_lte.wcdma_rrc_state)
      self.wrdb:setp("cell_measurement.wcdma_rrc_state",
        wcdma_rrc_state_names[wcdma_rrc_state]);
      for i=0,wcdma_lte.umts_lte_nbr_cell_len-1 do
        post_cell_info_to_rdb(
          'E',
          wcdma_lte.umts_lte_nbr_cell[i].earfcn,
          wcdma_lte.umts_lte_nbr_cell[i].pci,
          wcdma_lte.umts_lte_nbr_cell[i].rsrp,
          wcdma_lte.umts_lte_nbr_cell[i].rsrq)
      end
    end

    local nr5g_arfcn
    -- Note : The channel number here is SSB ARFCN and represents NR-ARFCN.
    if self.luaq.is_c_member_and_true(resp, "nr5g_arfcn_valid") then
      nr5g_arfcn = resp.nr5g_arfcn
      self.wrdb:setp("radio_stack.nr5g.arfcn",nr5g_arfcn)
      self.wrdb:setp("radio_stack.nr5g.dl_arfcn",nr5g_arfcn)
      self.wrdb:setp("radio_stack.nr5g.ul_arfcn",nr5g_arfcn)
    end

    local nr5g_scell = nil
    if self.luaq.is_c_member_and_true(resp, "nr5g_serving_cell_info_valid") then
      nr5g_scell = resp.nr5g_serving_cell_info
      local global_cell_id = tostring(tonumber(nr5g_scell.global_cell_id))
      self.wrdb:setp("radio_stack.nr5g.CellID",global_cell_id)
      local gnb_id = tostring(tonumber(self.bit.rshift(nr5g_scell.global_cell_id,8)))
      self.wrdb:setp("radio_stack.nr5g.gNB_ID",gnb_id)
    else
      self.wrdb:setp("radio_stack.nr5g.CellID","")
      self.wrdb:setp("radio_stack.nr5g.gNB_ID","")
    end
    if nr5g_arfcn and nr5g_scell then
      post_cell_info_to_rdb(
        'N',
        nr5g_arfcn,
        nr5g_scell.pci,
        nr5g_scell.rsrp/10,
        nr5g_scell.rsrq/10
      );
    end

  end

  -- remove leftover
  local i=n
  local cm
  local cm_val
  while true do
    cm=string.format("cell_measurement.%d",i)
    cm_val=self.wrdb:getp(cm)

    if not cm_val or cm_val == "" then
      break
    end

    self.wrdb:unsetp(cm)
    i=i+1
  end

  self.wrdb:setp("cell_measurement.qty",n)
  self.wrdb:setp("cell_measurement.cell_list", table.concat(cell_list, ","))
  -- update the cnt
  cell_measurement_cnt = cell_measurement_cnt + 1
  if cell_measurement_cnt >= 65535 then
    cell_measurement_cnt = 0
  end
  self.wrdb:setp("cell_measurement.cnt", cell_measurement_cnt)
  self.watcher.invoke("sys","poll_sig_info")

  local chg

  --[[

      This additional procedure is to minimize the period of mismatching between
      [wwan.0.cell_measurement.x] RDBs and [rrc_info.cell.x] SIB1 scan result.

      While neighboring cell measurement performs every second, PLMN background scan
      procedure performs parsing SIB1 messages of all neighboring cells every 10 seconds.
      To avoid unnecessary integrity broken period, we immediately perform PLMN background
      scan whenever a new measurement cell appears or disappears.

]]--

  -- check count
  chg = not self.last_cell_measurements or (#cell_measurements ~= #self.last_cell_measurements)

  -- check indivisual elements
  if not chg then
    for i=1,#self.last_cell_measurements do
      chg = chg or (self.last_cell_measurements[i] ~= cell_measurements[i])
    end
  end

  -- trigger background PLMN scan if any part of meausrement information is changed
  if chg then

    self.l.log("LOG_DEBUG",string.format("[bplmn-scan] cell measurement changed, perform BPLMN scan (old=%d,cur=%d)",#self.last_cell_measurements,#cell_measurements))
    self.watcher.invoke("sys","poll_plmn_list",{action="aperiodic"})

    self.last_cell_measurements = cell_measurements
  end

  return true
end

function QmiNas:QMI_NAS_GET_SERV_CELL_SIB_IND(type,event,qm)
  self.l.log("LOG_NOTICE", string.format("sequence :%s, sib_pkt_len :%d", qm.resp.sequence, qm.resp.sib_pkt_len))
  local s = ""
  for i=0,qm.resp.sib_pkt_len-1 do
    s = s..string.format("%X", qm.resp.sib_pkt[i])
  end
  self.l.log("LOG_NOTICE", string.format("%s", s))
end

QmiNas.cbs={
  "QMI_NAS_GET_RF_BAND_INFO",
  "QMI_NAS_GET_SIG_INFO",
  "QMI_NAS_PERFORM_NETWORK_SCAN",
  "QMI_NAS_PERFORM_INCREMENTAL_NETWORK_SCAN_IND",
  "QMI_NAS_OPERATOR_NAME_DATA_IND",
  "QMI_NAS_CURRENT_PLMN_NAME_IND",
  "QMI_NAS_NETWORK_TIME_IND",
  "QMI_NAS_SIG_INFO_IND",
  "QMI_NAS_SERVING_SYSTEM_IND",
  "QMI_NAS_SYS_INFO_IND",
  "QMI_NAS_RF_BAND_INFO_IND",
  "QMI_NAS_LTE_CPHY_CA_IND",
  "QMI_NAS_GET_CELL_LOCATION_INFO",
  "QMI_NAS_GET_SERV_CELL_SIB_IND",
}

function QmiNas:poll_lte_cphy_ca_info(type,event,a)
  local succ,qerr,resp=self.luaq.req(self.m.QMI_NAS_GET_LTE_CPHY_CA_INFO)
  if not succ then
    return self.watcher.invoke("sys","modem_on_lte_cphy_ca_info",nil)
  end

  local ia=self:build_lte_cphy_ca_info_arg(resp)

  return self.watcher.invoke("sys","modem_on_lte_cphy_ca_info",ia)
end

function QmiNas:poll_network_time(type,event,a)
  local succ,qerr,resp=self.luaq.req(self.m.QMI_NAS_GET_NETWORK_TIME)

  local network_time

  if self.luaq.is_c_true(resp.nas_3gpp2_time_valid) then
    network_time = resp.nas_3gpp2_time
  elseif self.luaq.is_c_true(resp.nas_3gpp_time_valid) then
    network_time = resp.nas_3gpp_time
  end

  if not network_time then
    self.l.log("LOG_ERR","no network time available")
    return
  end

  local time_zone
  if network_time.radio_if ~= "NAS_RADIO_IF_CDMA_1XEVDO_V01" then
    time_zone=network_time.time_zone -- in increments of 15 min
  end

  local ia = self:build_network_time_arg(network_time.universal_time,time_zone,network_time.daylt_sav_adj)
  return self.watcher.invoke("sys","modem_on_network_time",ia)
end

function QmiNas:poll_modem_network_status(type, event,a)
  local succ,qerr,resp=self.luaq.req(self.m.QMI_NAS_GET_SERVING_SYSTEM)

  local ia = self:build_modem_network_status_arg(resp)

  return self.watcher.invoke("sys","modem_on_network_state",ia)
end

function QmiNas:poll_ext_modem_network_status(type, event,a)
  local succ,qerr,resp=self.luaq.req(self.m.QMI_NAS_GET_SYS_INFO)

  local ia = self:build_ext_modem_network_status_arg(resp)

  return self.watcher.invoke("sys","modem_on_ext_network_state",ia)
end

function QmiNas:poll_sig_info(type,event,a)
  local qm = self.luaq.new_msg(self.m.QMI_NAS_GET_SIG_INFO)

  self.l.log("LOG_DEBUG", "send QMI_NAS_GET_SIG_INFO")
  self.luaq.send_msg_async(qm,self.config.modem_generic_poll_timeout)

  return true
end

function QmiNas:poll_rf_band_info(type,event,a)
  local qm = self.luaq.new_msg(self.m.QMI_NAS_GET_RF_BAND_INFO)

  self.l.log("LOG_DEBUG", "send QMI_NAS_GET_RF_BAND_INFO")
  self.luaq.send_msg_async(qm,self.config.modem_generic_poll_timeout)

  return true
end

function QmiNas:poll_quick(type, event, a)
  self.l.log("LOG_DEBUG","qmi nas quick poll")

  local succ,err,resp

  local qm = self.luaq.new_msg(self.m.QMI_NAS_GET_CELL_LOCATION_INFO)

  self.l.log("LOG_DEBUG", "send QMI_NAS_GET_CELL_LOCATION_INFO")
  self.luaq.send_msg_async(qm,self.config.modem_generic_poll_quick_timeout)

  return true

end

function QmiNas:poll_plmn_list(type, event, a)

  local nrb200_attached = self.wrdb:get("service.nrb200.attached") == "1"

  local now = self.t.util.gettimemonotonic()

  -- set initial scan time stamp
  if not self.initial_bplmn_scan_attempt_time then
    self.initial_bplmn_scan_attempt_time = now + self.config.modem_poll_interval
  end

  -- check to see if we need to ignore bplmn scan attempt
  local ignore_bplmn_scan = now < self.initial_bplmn_scan_attempt_time

  -- ignore if BPLMN is disabled
  if not self.dconfig.enable_poll_bplmn then
    self.l.log("LOG_DEBUG","[bplmn-scan] BPLMN scan disabled")
    -- perform_async_bplmn_scan()
    -- ignore if ignore BPLMN flag set
  elseif ignore_bplmn_scan then
    self.l.log("LOG_DEBUG","[bplmn-scan] ignore BPLMN scan for the initial polling period")
    -- do not scan without nrb200 attached, do not scan without ps attached. Additionally, aperiodic scan is only for NRB-200.
  elseif not nrb200_attached then
    if not self.ps_attached then
      self.l.log("LOG_DEBUG",string.format("[bplmn-scan] ignore BPLMN scan - no BPLMN scan condition (nrb200_attached=%s,ps_attached=%s,action=%s)",nrb200_attached,self.ps_attached,a.action))
    else
      local rrc_stat = self.wrdb:getp("radio_stack.rrc_stat.rrc_stat")
      local reg_stat = self.wrdb:getp("system_network_status.reg_stat")

      --
      -- Do not perform scan when UE is searching network (when UE is recovering RRC or EMM connection).
      -- Performing PLBMN scan could delay Qualcomm UE service request or TAU update. As a result of this
      -- tardiness, UE occasionally fails in AT&T AVL lab test cases like LTE-BTR-1-1850, LTE-BTR-1-1852
      -- or any back-off tests.
      --
      -- ** Detail of conflict **
      -- NAS requests of BPLMN scan, service request or attach request seem to be competing to obtain RRC
      -- layer. Unfortunately, these attempts do not seem to be prioritized in UE. Due to this structure,
      -- maximum delay of each type of requests is unpredictable. AT&T lab test cases allow about a minute
      -- of tardiness.
      --
      if rrc_stat == "idle camped" and (reg_stat == "1" or reg_stat == "5") then
        self.l.log("LOG_DEBUG","[bplmn-scan] post async bplmn scan request")
        return self:perform_async_bplmn_scan()
      else
        self.l.log("LOG_DEBUG",string.format("[bplmn-scan] not rrc idle, skip bplmn scan (rrc_stat=%s,reg_stat=%s)",rrc_stat,reg_stat))
      end
    end
  end

  return true
end

function QmiNas:poll_tx_rx_info(type, event,a)
  local lte_is_in_traffic = 0
  local lte_tx_pwr = ""

  local succ,qerr,resp=self.luaq.req(self.m.QMI_NAS_GET_TX_RX_INFO,{radio_if=0x08}) -- LTE
  if succ and self.luaq.is_c_member_and_true(resp, "tx_valid") then
    lte_is_in_traffic = resp.tx.is_in_traffic
    if lte_is_in_traffic == 1 and resp.tx.tx_pwr > 0 then
      lte_tx_pwr = string.format("%0.1f", resp.tx.tx_pwr / 10) -- Tx power value in 1/10 dbm
    end
  end

  local nr5g_is_in_traffic = 0
  local nr5g_tx_pwr = ""

  succ,qerr,resp=self.luaq.req(self.m.QMI_NAS_GET_TX_RX_INFO,{radio_if=0x0C}) -- NR5G
  if succ and self.luaq.is_c_member_and_true(resp, "tx_valid") then
    nr5g_is_in_traffic = resp.tx.is_in_traffic
    if nr5g_is_in_traffic == 1 and resp.tx.tx_pwr > 0 then
      nr5g_tx_pwr = string.format("%0.1f", resp.tx.tx_pwr / 10) -- Tx power value in 1/10 dbm
    end
  end

  self.wrdb:setp_if_chg("system_network_status.is_in_traffic", lte_is_in_traffic)
  self.wrdb:setp_if_chg("system_network_status.tx_pwr", lte_tx_pwr)
  self.wrdb:setp_if_chg("radio_stack.nr5g.is_in_traffic", nr5g_is_in_traffic)
  self.wrdb:setp_if_chg("radio_stack.nr5g.tx_pwr", nr5g_tx_pwr)

  return true
end

function QmiNas:poll(type, event, a)      -- get PLMN list

  self.l.log("LOG_DEBUG","[bplmn-scan] invoke periodic PLMN list")
  self.watcher.invoke("sys","poll_plmn_list",{action="periodic"})

  --[[
      unfortunately, poll is required for signal information as the indication stops when there is no signal
  ]]--
  self.watcher.invoke("sys","poll_sig_info")
  self.watcher.invoke("sys","poll_rf_band_info")
  self.watcher.invoke("sys","poll_modem_network_status")
  self.watcher.invoke("sys","poll_ext_modem_network_status")
  self.watcher.invoke("sys","poll_network_scan_mode")
  self.watcher.invoke("sys","poll_lte_cphy_ca_info")
  self.watcher.invoke("sys","poll_tx_rx_info")

  return true
end

function QmiNas:detach(type,event)

  local succ, err = self.luaq.req(self.m.QMI_NAS_INITIATE_ATTACH,{ps_attach_action="NAS_PS_ACTION_DETACH_V01"})
  if succ then
    return true
  elseif err == self.luaq.e.QMI_ERR_NO_EFFECT then
    -- if already detached, QMI_ERR_NO_EFFECT will be returned, which is OK, but need to poll to update status
    self.l.log("LOG_INFO","detach has no effect")
    self.watcher.invoke("sys","poll_modem_network_status")
    return true
  end
  return succ
end

function QmiNas:attach(type,event)

  local succ, err = self.luaq.req(self.m.QMI_NAS_INITIATE_ATTACH,{ps_attach_action="NAS_PS_ACTION_ATTACH_V01"})
  if succ then
    return true
  elseif err == self.luaq.e.QMI_ERR_NO_EFFECT then
    -- if already attached, QMI_ERR_NO_EFFECT will be returned, which is OK, but need to poll to update status
    self.l.log("LOG_INFO","attach has no effect")
    self.watcher.invoke("sys","poll_modem_network_status")
    return true
  end
  return succ
end

function QmiNas:band_selection(type,event,a)
  return self:bandPrefSet(a.bands) -- expect a semicolon-separated string with band names or hex codes as components
end

function QmiNas:cell_lock(type,event,a)
  a=a or {} -- expect { {pci=PCIx,earfcn=EARFCNx} }
  self.l.log("LOG_INFO",string.format("cell_lock: locksets=%s",table.tostring(a)))
  local qm=self.luaq.new_msg(self.m.QMI_NAS_SET_CELL_LOCK_CONFIG)
  qm.req.cell_list_len=#a
  for i=1,#a do
    qm.req.cell_list[i-1].pci=a[i].pci
    qm.req.cell_list[i-1].freq=a[i].earfcn
  end

  if not self.luaq.send_msg(qm) then
    self.l.log("LOG_ERR",string.format("cell_lock send_msg failed"))
    return false, -1, qm.resp, qm
  end
  self.l.log("LOG_INFO",string.format("cell_lock send_msg succeeded: %s, %s",tonumber(qm.resp.resp.result),tonumber(qm.resp.resp.error)))
  return self.luaq.ret_qm_resp(qm)
end

-- set the lockmode to qualcomm next generation modem which no need to detach and attach before lock
-- This function sets the pci lock without requiring a reboot.
-- It was implemented for magpie, and can be used by any project as the feature is built into the modem.
function QmiNas:set_dynamic_cell_lock(rdbKey, rdbVal)
  -- compare with the modem current pci lock list
  local succ,curr_lock_cell_list = self:get_dynamic_cell_lock(nil,nil)
  if rdbVal == self.util.pack_lockmode(curr_lock_cell_list) then
    self.l.log("LOG_NOTICE",string.format("set_dynamic_cell_lock: identical lock list, skip it, locksets=%s",rdbVal))
    luardb.set("wwan.0.cell_lock.cmd.status", "[done]")
    return
  end

  local locksets = {}
  if not rdbVal or rdbVal == '' then
     self.l.log("LOG_NOTICE",string.format("clear the dynamic cell lock"))
  else
    locksets = self.util.parse_lockmode(rdbVal)
    if not locksets then
      self.l.log("LOG_ERR",string.format("set_dynamic_cell_lock: locksets parsed failed, %s = %s", rdbKey, rdbVal))
      luardb.set("wwan.0.cell_lock.cmd.status", "[error]")
      return
    end
    self.l.log("LOG_NOTICE",string.format("set_dynamic_cell_lock: locksets=%s",table.tostring(locksets)))
  end

  local qm = self.luaq.new_msg(self.m.QMI_NAS_SET_CELL_LOCK_CONFIG)
  qm.req.enforce_valid = 1
  qm.req.enforce = 1
  qm.req.cell_list_len = #locksets
  for i=1,#locksets do
    qm.req.cell_list[i-1].pci = locksets[i].pci
    qm.req.cell_list[i-1].freq = locksets[i].earfcn
  end
  if not self.luaq.send_msg(qm) then
    self.l.log("LOG_ERR",string.format("set_dynamic_cell_lock: cell_lock send_msg failed,result: %s, error: %s",tonumber(qm.resp.resp.result),tonumber(qm.resp.resp.error)))
    luardb.set("wwan.0.cell_lock.cmd.status", "[error]")
  else
    self.l.log("LOG_NOTICE",string.format("set_dynamic_cell_lock: send_msg succeeded: %s, %s",tonumber(qm.resp.resp.result),tonumber(qm.resp.resp.error)))
    luardb.set("wwan.0.cell_lock.cmd.status", "[done]")
    self:get_dynamic_cell_lock(nil,nil)
  end
end

-- get the pci lock list from the modem
-- Required for magpie, and can be used by any other project as the feature is built into the modem.
function QmiNas:get_dynamic_cell_lock(rdbKey, rdbVal)
  local locksets = {}
  local qm = self.luaq.new_msg(self.m.QMI_NAS_GET_CELL_LOCK_CONFIG)
  if not self.luaq.send_msg(qm) then
    self.l.log("LOG_ERR", "send QMI_NAS_GET_CELL_LOCK_CONFIG failed")
    return false, locksets
  end
  self.l.log("LOG_INFO",string.format("send QMI_NAS_GET_CELL_LOCK_CONFIG succeeded: %s, %s",tonumber(qm.resp.resp.result),tonumber(qm.resp.resp.error)))
  if qm.resp.cell_list_valid == 1 then
    for i=0,qm.resp.cell_list_len-1,1 do
      table.insert(locksets, {pci=qm.resp.cell_list[i].pci, earfcn=qm.resp.cell_list[i].freq})
      self.l.log("LOG_NOTICE", string.format("PCI Lock index:%d, PCI: %s, EARFCN: %s", i, qm.resp.cell_list[i].pci, qm.resp.cell_list[i].freq))
    end
  end
  -- log and write back to the rdb
  local curr_lock_cells = self.util.pack_lockmode(locksets)
  self.wrdb:setp("modem_pci_lock_list", curr_lock_cells)
  return true, locksets
end

function QmiNas:set_dynamic_cell_lock_5G()

  local lock_params = luardb.get("wwan.0.cell_lock.cmd.param.lock_list")

  if not lock_params then
    self.l.log("LOG_ERR",string.format("set_dynamic_cell_lock_5G: parameters for PCI lock not set"))
    return false
  end

  -- compare with the modem current pci lock list
  local succ, curr_lock_cell_list = self:get_dynamic_cell_lock_5G()
  if lock_params == self.util.pack_lockmode_5G(curr_lock_cell_list) then
    self.l.log("LOG_NOTICE",string.format("set_dynamic_cell_lock_5G: identical lock list, skip it, locksets=%s",lock_params))
    return true
  end

  local cell_identity = {}
  local req_type
  if lock_params == '' then
    self.l.log("LOG_NOTICE",string.format("set_dynamic_cell_lock_5G: clear 5G dynamic cell lock"))
    -- The following NV item should be deleted by the QMI to make clearing of PCI lock persistent
    -- But currently the QMI does not do so, hence the below workaround
    -- Refer to https://qualcomm-cdmatech-support.my.salesforce.com/5004V0000193MOb
    -- and https://qualcomm-cdmatech-support.my.salesforce.com/5004V000017zXl4
    os.execute("efs_util -D /nv/item_files/modem/nr5g/RRC/pci_lock_info")
    req_type = 'NAS_NR5G_CELL_CFG_TYPE_UNLOCK_V01'
  else
    local locksets = self.util.parse_lockmode_5G(lock_params)
    if not locksets then
      self.l.log("LOG_ERR",string.format("set_dynamic_cell_lock_5G: locksets parsed failed = %s", lock_params))
      return false
    end
    self.l.log("LOG_NOTICE",string.format("set_dynamic_cell_lock_5G: locksets=%s",table.tostring(locksets)))

    -- locksets{} will hold only 1 set(table) of parameters as QMI_NAS_GET_NR5G_CELL_CONFIG supports only 1 cell
    cell_identity.pci = tonumber(locksets[1].pci)
    cell_identity.arfcn = tonumber(locksets[1].arfcn)
    local scs_val = locksets[1].scs
    local scs_val_sets = {['15']=true, ['30']=true, ['60']=true, ['120']=true, ['240']=true}
    if not scs_val_sets[scs_val] then
      self.l.log("LOG_ERR", string.format("set_dynamic_cell_lock_5G: invalid scs value"))
      return false
    else
      cell_identity.scs = 'NAS_NR5G_SCS_'..scs_val..'_V01'
    end

    -- determine band
    local band = locksets[1].band
    local mask, hex
    local nr5g_sa_band_pref = {}
    hex = tonumber(band,16)
    if hex then
      band = self.band_names[hex]
    end
    mask = band and self.band_name_to_mask[band]
    hex = band and self.band_name_to_hex[band]
    if hex then
      if mask.nr5g_sa_mask then
        nr5g_sa_band_pref = self:bits_or(nr5g_sa_band_pref, mask.nr5g_sa_mask)
      else
        self.l.log("LOG_ERR", string.format("set_dynamic_cell_lock_5G: nr5g_sa_mask not found"))
	return false
      end
    end
    self:init_nr5g_band_pref_tbl()
    for i,_ in pairs(nr5g_sa_band_pref) do
      local idx = math.floor(i/64)
      local elem = self.nr5g_band_pref_tbl[idx]
      if elem then
        elem.bits[i - (64*idx)] = true
      end
    end
    local nr5g_sa_band_pref_arg = {}
    for _, elem in pairs(self.nr5g_band_pref_tbl) do
      nr5g_sa_band_pref_arg[elem.name] = self:bits_to_mask(elem.bits)
    end

    cell_identity.band = nr5g_sa_band_pref_arg

    req_type = 'NAS_NR5G_CELL_CFG_TYPE_PCI_V01'

  end

  local settings = {
    type = req_type,
    cell_identity = cell_identity,
  }

  succ,qerr,resp=self.luaq.req(self.m.QMI_NAS_SET_NR5G_CELL_CONFIG, settings)
  if not succ then
    self.l.log("LOG_ERR", string.format("set_dynamic_cell_lock_5G: QMI_NAS_SET_NR5G_CELL_CONFIG request failed with error = %d", tonumber(resp.resp.error)))
  else
    self.l.log("LOG_INFO", "set_dynamic_cell_lock_5G: QMI_NAS_SET_NR5G_CELL_CONFIG request succeeded")
    -- update the rdb variable wwan.0.modem_pci_lock_list_5g
    self:get_dynamic_cell_lock_5G()
  end
  return succ

end

function QmiNas:get_dynamic_cell_lock_5G()
  local qm=self.luaq.new_msg(self.m.QMI_NAS_GET_NR5G_CELL_CONFIG)

  if not self.luaq.send_msg(qm) then
    self.l.log("LOG_ERR", "get_dynamic_cell_lock_5G: send QMI_NAS_GET_NR5G_CELL_CONFIG failed")
    return false
  end

  self.l.log("LOG_INFO",string.format("get_dynamic_cell_lock_5G: QMI_NAS_GET_NR5G_CELL_CONFIG send_msg succeeded: %s, %s",tonumber(qm.resp.resp.result),tonumber(qm.resp.resp.error)))

  if qm.resp.resp.error == 0 and qm.resp.cell_identity_valid == 1 then
    local locksets = {}
    -- scs value
    scs_values = {
      [0] = "15",
      [1] = "30",
      [2] = "60",
      [3] = "120",
      [4] = "240"
    }
    local scs_value = scs_values[tonumber(qm.resp.cell_identity.scs)]
    -- band value
    local band_value
    local nr5g_sa_band_pref_tbl = {
      bits_1_64    = {offset = 0,   bits = nil},
      bits_65_128  = {offset = 64,  bits = nil},
      bits_129_192 = {offset = 128, bits = nil},
      bits_193_256 = {offset = 192, bits = nil},
      bits_257_320 = {offset = 256, bits = nil},
      bits_321_384 = {offset = 320, bits = nil},
      bits_385_448 = {offset = 384, bits = nil},
      bits_449_512 = {offset = 448, bits = nil},
    }
    for k, v in pairs(nr5g_sa_band_pref_tbl) do
      v.bits = self:mask_to_bits(qm.resp.cell_identity.band[k])
      for bit, _ in pairs(v.bits) do
        local hex = self.nr5g_sa_band_to_hex[bit + v.offset + 1]
        if hex and self.band_names[hex] then
	  band_value = self.band_names[hex]
	else
          self.l.log("LOG_ERR", "get_dynamic_cell_lock_5G: band info incorrect")
	  return false
        end
      end
    end

    table.insert(locksets, {pci=qm.resp.cell_identity.pci, arfcn=qm.resp.cell_identity.arfcn, scs=scs_value, band=band_value})
    self.l.log("LOG_INFO", string.format("get_dynamic_cell_lock_5G: Locksets ->  %s",table.tostring(locksets)))
    local curr_lock_cells = self.util.pack_lockmode_5G(locksets)
    self.wrdb:setp("modem_pci_lock_list_5g", curr_lock_cells)
    return true, locksets
  elseif qm.resp.resp.error == 3 then		-- the QMI gives an internal error when no PCI lock is set
    self.l.log("LOG_NOTICE", "5G PCI lock not set.")
    self.wrdb:setp("modem_pci_lock_list_5g", "")
    return true
  end

  return false
end

QmiNas.cbs_system={
  "poll_lte_cphy_ca_info",
  "poll_network_time",
  "poll_modem_network_status",
  "poll_ext_modem_network_status",
  "poll_sig_info",
  "poll_rf_band_info",
  "poll_quick",
  "poll_plmn_list",
  "poll",
  "detach",
  "attach",
  "band_selection",
  "cell_lock",
  "set_dynamic_cell_lock",
  "get_dynamic_cell_lock",
  "pci_scan",
  "poll_network_scan_mode",
  "poll_tx_rx_info",
}

-- Build module supported band list & fill band_name_to_mask, band_name_to_hex
-- TODO : QMI_DMS_GET_BAND_CAPABILITY (0x0045) is deprecated and replaced with
--        QMI_DMS_GET_BAND_CAPABILITY_EX  (0x0078) but at the time of writing this,
--        it is not supported in the modem f/w. It should be implemented later.
function QmiNas:get_module_band_list()
  local succ,qerr,resp=self.luaq.req(self.m.QMI_DMS_GET_BAND_CAPABILITY)
  if succ then
    local capsArray = {}
    local hex, i
    local gsmMask, wcdmaMask, lteMask, nr5gMask_NSA, nr5gMask_SA = {},{},{},{},{}
    self.band_name_to_mask = {}
    self.band_name_to_hex = {}
    local hexForm = "%04x"

    -- GSM/WCDMA bands. resp.band_capability is mandatory
    if not self:is_hidden_band("GSM") or not self:is_hidden_band("WCDMA") then
      local caps = self:mask_to_bits(resp.band_capability)
      -- run up to 255 for extended LTE band
      for i=0, table.maxn(caps) do
        if caps[i] then
          hex = self.band_bit_to_hex[i]
          if hex and self.band_names[hex] then
            self.band_name_to_mask[self.band_names[hex]] = {mask={[i]=true}}
            self.band_name_to_hex[self.band_names[hex]] = hex
            if self:hex_in_gsm_band(hex) and not self:is_hidden_band("GSM") then
              capsArray[#capsArray+1]=string.format(hexForm,hex)..','..self.band_names[hex]
              gsmMask[i] = true
            elseif self:hex_in_wcdma_band(hex) and not self:is_hidden_band("WCDMA") then
              capsArray[#capsArray+1]=string.format(hexForm,hex)..','..self.band_names[hex]
              wcdmaMask[i] = true
            end
          end
        end
      end
    end

    -- LTE bands :
    --    0X10 LTE Band Capability : deplicated
    --    0X12 Supported LTE Bands : preferred
    if not self:is_hidden_band("LTE") then
      if self.luaq.is_c_true(resp.lte_band_capability_valid) or
        self.luaq.is_c_true(resp.supported_lte_bands_valid) then
        -- enable LTE capability for this radio
        self.radio_capability.is_lte_supported=true;
        local caps = {}
        if self.luaq.is_c_true(resp.supported_lte_bands_valid) then
          band_len = resp.supported_lte_bands_len
          self.l.log("LOG_DEBUG",string.format("supported_lte_bands_len = [%d]", band_len))
          caps_filter = {}
          caps_filter_count = 0
          band_filter = luardb.get("wwan.0.module_band_list_filter")
          if band_filter then
            filter_list = band_filter:split(',')
            for _ , value in pairs(filter_list) do
              caps_filter[tonumber(value)] = true
              caps_filter_count = caps_filter_count + 1
            end
          end
          for i=0, band_len-1 do
            if caps_filter_count == 0 or caps_filter[resp.supported_lte_bands[i]] then
              caps[resp.supported_lte_bands[i]-1] = true
            end
          end
        elseif self.luaq.is_c_true(resp.lte_band_capability_valid) then
          caps = self:mask_to_bits(resp.lte_band_capability)
        end
        self.l.log("LOG_DEBUG",string.format("lte band capa = [%s]", self:bits_to_string(caps)))

        for i=0, table.maxn(caps) do
          if caps[i] then
            hex = self.lte_band_bit_to_hex[i]
            if hex and self.band_names[hex] then
              self.band_name_to_mask[self.band_names[hex]] = {lte_mask={[i]=true}}
              self.band_name_to_hex[self.band_names[hex]] = hex
              capsArray[#capsArray+1]=string.format(hexForm,hex)..',' .. self.band_names[hex]
              lteMask[i] = true
            end
          end
        end
      end
    end

    -- NR5G (both NSA and SA)
    if not self:is_hidden_band("NR5G") then
      if self.luaq.is_c_member_and_true(resp, "supported_nr5g_bands_valid") then
        -- enable NR5G capability for this radio
        self.radio_capability.is_nr5g_supported=true;
        local caps = {}
        band_len = resp.supported_nr5g_bands_len
        self.l.log("LOG_DEBUG",string.format("supported_nr5g_bands_len = [%d]", band_len))
        for i=0, band_len-1 do
          self.l.log("LOG_DEBUG",string.format("resp.supported_nr5g_bands[i] = [%d]", resp.supported_nr5g_bands[i]))
          caps[resp.supported_nr5g_bands[i]-1] = true
        end
        self.l.log("LOG_DEBUG",string.format("nr5g band capa = [%s]", self:bits_to_string(caps)))

        for i=0, table.maxn(caps) do
          if caps[i] then
            hex = self.nr5g_nsa_band_to_hex[i+1]
            if hex and self.band_names[hex] then
              self.band_name_to_mask[self.band_names[hex]] = {nr5g_nsa_mask={[i]=true}}
              self.band_name_to_hex[self.band_names[hex]] = hex
              capsArray[#capsArray+1]=string.format(hexForm,hex)..',' .. self.band_names[hex]
              nr5gMask_NSA[i] = true
            end
            hex = self.nr5g_sa_band_to_hex[i+1]
            if hex and self.band_names[hex] then
              self.band_name_to_mask[self.band_names[hex]] = {nr5g_sa_mask={[i]=true}}
              self.band_name_to_hex[self.band_names[hex]] = hex
              capsArray[#capsArray+1]=string.format(hexForm,hex)..',' .. self.band_names[hex]
              nr5gMask_SA[i] = true
            end
          end
        end
      end
    end

    -- build band groups
    self.l.log("LOG_DEBUG",string.format("[get_module_band_list] mask: GSM=[%s], WCDMA=[%s], LTE=[%s], NR5GNSA=[%s], NR5GSA=[%s]",
                                          self:bits_to_string(gsmMask), self:bits_to_string(wcdmaMask), self:bits_to_string(lteMask),
                                          self:bits_to_string(nr5gMask_NSA), self:bits_to_string(nr5gMask_SA)))

    gsmTuple       = {0, (next(gsmMask) ~= nil and self.HEX_GROUP_GSM or nil)}
    wcdmaTuple     = {0, (next(wcdmaMask) ~= nil and self.HEX_GROUP_WCDMA or nil)}
    lteTuple       = {0, (next(lteMask) ~= nil and self.HEX_GROUP_LTE or nil)}
    nr5gTuple_NSA  = {0, (next(nr5gMask_NSA) ~= nil and self.HEX_GROUP_NR5G_NSA or nil)}
    nr5gTuple_SA   = {0, (next(nr5gMask_SA) ~= nil and self.HEX_GROUP_NR5G_SA or nil)}

    for _, v_nr5g_sa in pairs(nr5gTuple_SA) do
      for _, v_nr5g_nsa in pairs(nr5gTuple_NSA) do
        for _,v_lte in pairs(lteTuple) do
          for _, v_wcdma in pairs(wcdmaTuple) do
            for _, v_gsm in pairs(gsmTuple) do
              local hex = self.bit.bor(v_gsm, v_wcdma, v_lte, v_nr5g_nsa, v_nr5g_sa)
              if hex ~= 0 then
                local gwMask = {}  -- combined mask for GSM and WCDMA
                if v_gsm ~= 0 then
                  gwMask = self:bits_or(gwMask, gsmMask)
                end
                if v_wcdma ~= 0 then
                  gwMask = self:bits_or(gwMask, wcdmaMask)
                end

                local bandMask = {
                  mask = (next(gwMask) ~= nil and gwMask or nil),
                  lte_mask = (v_lte ~= 0 and lteMask or nil),
                  nr5g_nsa_mask = (v_nr5g_nsa ~= 0 and nr5gMask_NSA or nil),
                  nr5g_sa_mask = (v_nr5g_sa ~= 0 and nr5gMask_SA or nil),
                }

                if hex and self.band_names[hex] then
                  self.band_name_to_mask[self.band_names[hex]] = bandMask
                  self.band_name_to_hex[self.band_names[hex]] = hex
                  capsArray[#capsArray+1]=string.format(hexForm,hex)..','..self.band_names[hex]
                else
                  self.l.log("LOG_DEBUG",string.format("band_name for hex=%s is missing!", hex))
                end
              end
            end
          end
        end
      end
    end

    self.l.log("LOG_DEBUG","band_name_to_mask="..table.tostring(self.band_name_to_mask))
    self.l.log("LOG_DEBUG","band_name_to_hex="..table.tostring(self.band_name_to_hex))
    luardb.set("wwan.0.module_band_list", table.concat(capsArray,'&'))

    luardb.set("wwan.0.module_band_list.hexmask",
                string.format("GSM:%s,WCDMA:%s,LTE:%s,NR5GNSA:%s,NR5GSA:%s",
                              self:bits_to_hex_mask(gsmMask, 32),
                              self:bits_to_hex_mask(wcdmaMask, 32),
                              self:bits_to_hex_mask(lteMask, 32),
                              self:bits_to_hex_mask(nr5gMask_NSA, 64),
                              self:bits_to_hex_mask(nr5gMask_SA, 64)))


    -- Set available Mode Preference from HW supported band list because
    -- there is no QMI command for that.
#ifdef V_RAT_SEL_y
    local rat_list = ""
    if next(gsmMask) ~= nil and not self:is_hidden_band("GSM") then
      rat_list = rat_list .. "GSM;"
    end
    if next(wcdmaMask) ~= nil and not self:is_hidden_band("WCDMA") then
      rat_list = rat_list .. "WCDMA;"
    end
    if next(lteMask) ~= nil and not self:is_hidden_band("LTE") then
      rat_list = rat_list .. "LTE;"
    end
    if (next(nr5gMask_NSA) ~= nil or next(nr5gMask_SA) ~= nil) and
        not self:is_hidden_band("NR5G") then
      rat_list = rat_list .. "NR5G;"
    end
    rat_list = rat_list:sub(1, -2)
    self.l.log("LOG_DEBUG","RAT list="..rat_list)
    luardb.set("wwan.0.module_rat_list", rat_list)
#endif
  end
  return succ
end

function QmiNas:get_network_name()
  local reqId = self.m.QMI_NAS_GET_PLMN_NAME
  if not reqId then
    self.l.log("LOG_ERR", string.format("QMI_NAS_GET_PLMN_NAME request is not vaild"))
    return
  end

  local ia = {}
  local qm = self.luaq.new_msg(reqId)

  qm.req.send_all_information = true
  self.luaq.send_msg(qm)

  local succ,qerr,resp=self.luaq.ret_qm_resp(qm)
  if not succ then
    self.l.log("LOG_ERR", string.format("QMI_NAS_GET_PLMN_NAME response failed, error 0x%04x", qerr))
    return
  end

  if self.luaq.is_c_true(resp.eons_plmn_name_3gpp_valid) then
    local plmn_name = resp.eons_plmn_name_3gpp

    if tonumber(plmn_name.spn_len) >0 then
      ia.spn = self.ffi.string(plmn_name.spn, plmn_name.spn_len)
    end

    if tonumber(plmn_name.plmn_short_name_len) > 0 and
      plmn_name.plmn_short_name_enc == "NAS_CODING_SCHEME_CELL_BROADCAST_GSM_V01" then
      local plmn_short_name = self.ffi.string(plmn_name.plmn_short_name, plmn_name.plmn_short_name_len)
      if plmn_short_name and plmn_short_name ~= "000 00" then
        ia.short_name = plmn_short_name
      end
    end

    if tonumber(plmn_name.plmn_long_name_len) > 0 and
      plmn_name.plmn_long_name_enc == "NAS_CODING_SCHEME_CELL_BROADCAST_GSM_V01" then
      local plmn_long_name = self.ffi.string(plmn_name.plmn_long_name, plmn_name.plmn_long_name_len)
      if plmn_long_name and plmn_long_name ~= "000 00" then
        ia.long_name = plmn_long_name
      end
    end
  end

  if self.luaq.is_c_true(resp.nw_name_source_valid) then
    ia.name_source = self.nw_name_source_names[tonumber(resp.nw_name_source)]
  end

  return self.watcher.invoke("sys", "modem_on_operator_name", ia)
end

-- This function is the trigger handler for the subscribed RDB wwan.0.plmn.scan,
-- which has been present since the very beginning of wmmd,
-- though that RDB may be obsoleted.
-- User code needs to write non-empty value to that RDB.
function QmiNas:plmn_scan_rdb_trigger_handler(rdb_name, rdb_value)
    if rdb_name == "wwan.0.plmn.scan" and rdb_value and rdb_value ~= "" then
        self:perform_async_bplmn_scan()
    end
end

-- Synchronize hidden bands
-- If hidden band is set then synchronize modem band list with the hidden band
function QmiNas:sync_hidden_bands()
    local selBand = luardb.get("wwan.0.currentband.current_selband.hexmask") or ""
    if selBand == "" then
        return
    end
    local hiddenBands = self.dconfig.hidden_bands
    if hiddenBands == "" then
        return
    end

    self.l.log("LOG_NOTICE", "sync hidden bands")

    local B32_mask = "0x00000000000000000000000000000000"
    local B64_mask = "0x0000000000000000000000000000000000000000000000000000000000000000"
    local band_mask_tbl = {
        ["GSM"]     = B32_mask,
        ["WCDMA"]   = B32_mask,
        ["LTE"]     = B32_mask,
        ["NR5G"]    = B64_mask,
        ["NR5GNSA"] = B64_mask,
        ["NR5GSA"]  = B64_mask
    }

    for _, v in ipairs(selBand:split(",")) do
        local n = v:split(":")
        local band, mask = n[1], n[2]
        local key = (band == "NR5GNSA" or band == "NR5GSA") and "NR5G" or band
        -- Copy non-hiden band mask only
        if not string.find(hiddenBands, key) then
            band_mask_tbl[band] = mask
        end
    end

    -- Merge new band mask string and update the modem band list
    local newBandStr = ""
    for _, i in ipairs({ "GSM", "WCDMA", "LTE", "NR5G", "NR5GNSA", "NR5GSA" }) do
        newBandStr = string.format("%s%s%s:%s", newBandStr, newBandStr == "" and "" or ",", i, band_mask_tbl[i])
    end
    self.l.log("LOG_DEBUG","newBandStr = "..newBandStr)
    if selBand ~= newBandStr then
        local succ = self:bandPrefSetHexMask(newBandStr)
        if not succ then
            self.l.log("LOG_ERR","bandPrefSetHexMask() failed, return")
            return
        end

        -- Get preferred band list list
        local bandStr = self:bandPrefGet()
        if bandStr then
            luardb.set("wwan.0.currentband.current_selband", bandStr)
        end
    else
        self.l.log("LOG_NOTICE","newBandStr is same as current band, no change")
    end
end

function QmiNas:init()

  self.l.log("LOG_INFO", "initiate qmi_nas")

  -- add watcher for qmi
  for _,v in pairs(self.cbs) do
    self.watcher.add("qmi", v, self, v)
  end

  -- add watcher for system
  for _,v in pairs(self.cbs_system) do
    self.watcher.add("sys", v, self, v)
  end

  local succ,err,res = self.luaq.req(self.m.QMI_NAS_INDICATION_REGISTER,{
    req_serving_system=1,
    reg_network_time=1,
    sys_info=1,
    sig_info=1,
    reg_current_plmn_name=1,
    reg_rf_band_info=1,
    reg_operator_name_data=1,
    reg_lte_cphy_ca=1
  })


  -- register delta indication
  succ,err,res = self.luaq.req(self.m.QMI_NAS_CONFIG_SIG_INFO2,{
    cdma_rssi_delta=1,
    cdma_ecio_delta=1,
    hdr_rssi_delta=1,
    hdr_ecio_delta=1,
    hdr_sinr_delta=1,
    hdr_io_delta=1,
    gsm_rssi_delta=1,
    wcdma_rssi_delta=1,
    wcdma_ecio_delta=1,
    lte_rssi_delta=1,
    lte_snr_delta=1,
    lte_rsrq_delta=1,
    lte_rsrp_delta=1,
    tdscdma_rscp_delta=1,
    tdscdma_rssi_delta=1,
    tdscdma_ecio_delta=1,
    tdscdma_sinr_delta=1
  })

  -- Get supported band list
  self:get_module_band_list()

  -- Get preferred band list list
  local bandStr = self:bandPrefGet()
  if bandStr then
      luardb.set("wwan.0.currentband.current_selband", bandStr)
  end

  -- Synchronize hidden bands
  self:sync_hidden_bands()

  -- Get network name
  self:get_network_name()

  -- Get current cell lock list
  self:get_dynamic_cell_lock(nil, nil)

#ifdef V_NR5G_CELL_LOCK_y
  self:get_dynamic_cell_lock_5G()
#endif

  -- band selection RDB watch
  self.rdbWatch:addObserver("wwan.0.currentband.cmd.command", "bandOps", self)

  -- plmn list scan RDB watch
  self.rdbWatch:addObserver("wwan.0.plmn.scan", "plmn_scan_rdb_trigger_handler", self)

  -- plmn selection RDB watch
  self.rdbWatch:addObserver(self.config.rdb_g_prefix.."PLMN_command_state", "plmnOps", self)

  -- plmn selection RDB watch for latest NAS version
  self.rdbWatch:addObserver(self.config.rdb_g_prefix.."plmn.cmd.command", "plmnOps", self)

  -- set the dynamic cell lock
  self.rdbWatch:addObserver("wwan.0.neighbour_lock_list", "set_dynamic_cell_lock", self)

  -- get the lock in the modem
  self.rdbWatch:addObserver("wwan.0.modem_pci_lock_query", "get_dynamic_cell_lock", self)

  -- dynamic cell lock RDB watch
  self.rdbWatch:addObserver("wwan.0.cell_lock.cmd.command", "cellLockOps", self)

  -- set the service domain preference in the modem
  self.rdbWatch:addObserver("wmmd.config.modem_service_domain", "serviceDomainPrefSet", self)

  -- get the service domain preference from the modem
  self.rdbWatch:addObserver("wwan.0.get_modem_service_domain", "serviceDomainPrefGet", self)

  --[[
  succ,err,res = luaq.req(m.QMI_NAS_GET_LTE_BAND_PRIORITY_LIST)

  local qm = luaq.new_msg(m.QMI_NAS_SET_LTE_BAND_PRIORITY)
  -- build request
  qm.req.band_priority_list_len=1
  qm.req.band_priority_list[0]="NAS_ACTIVE_BAND_E_UTRA_OPERATING_BAND_2_V01"

  -- send sync
  luaq.send_msg(qm)

  succ,err,res = luaq.req(m.QMI_NAS_GET_LTE_BAND_PRIORITY_LIST)
  ]]--

end

return QmiNas
