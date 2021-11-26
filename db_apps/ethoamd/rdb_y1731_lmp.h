#pragma once
#ifdef NCI_Y1731

///|Y1731.{i}.Lmp.Cos||Unsigned int||W||P||0||CoS value(0..7). It replaces the priority field in
///LBM/LTM/AIS/LCK/CSF/MCC/EXM/VSM, only for 802.1Q or 802.1AD
///:    0: TC4
///:    5: TC1
#define Y1731_Lmp_Cos	"y1731.lmp.cos"

///|Y1731.{i}.Lmp.Status||String||R|| || ||Dot1ag/Y.1731 LMP operation status
///* alias to "Dot1ag.{i}.Lmp.Status"
///All error message related to operation on LMP level
///*Success: LMP is enabled
///*Success: LMP is disabled
///*Success: MEP is active
///*Success: MEP is inactive
///*Success: CCM is enabled
///*Success: CCM is disabled
///*Success: collect CCM stats
///*Error: Cannot enable MEP
///*Error: Cannot disable MEP
///*Error: cannot setup LMP end point xxx
///*Error: Cannot enable CCM
///*Error: Cannot disable CCM
///*Error: Cannot collect CCM stats
#define A_Y1731_Lmp_Status	LMP_Status


//enable AIS
//$  0 -- success
//$ <0 --- error code
int AIS_enable(Session *pSession);

// disable AIS
int AIS_disable(Session *pSession);


// auto adjust AIS state
//$  0 -- success
//$ <0 --- error code
int AIS_auto_start(Session *pSession);






// collect Y1731 parameter
//$  0 -- success
//$ <0 --- error code
//int Y1731_Collect(Session* pSession);

#endif
