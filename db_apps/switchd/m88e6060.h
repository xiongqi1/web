#define ID_M88E6060			0x1410c8

// Phy register definitions

#define M88E6060_PSSR		0x11
#define M88E6060_VCTC		0x1A
#define M88E6060_VCTS		0x1B

#define VCT_ENVCT				0x8000
#define VCT_TST_MSK			0x6000
#define VCT_AMP_MSK			0x1F00
#define VCT_DST_MSK			0x00FF
#define VCT_TST_OFS			13
#define VCT_AMP_OFS			8
#define VCT_DST_OFS			0

#define VCT_TST_OK			0
#define VCT_TST_SC			1
#define VCT_TST_OC			2
#define VCT_TST_FAIL			3

// Port register definitions

#define M88E6060_PORT_OFS	0x08

#define M88E6060_PCR			0x04
#define M88E6060_PAV			0x0B

#define PCR_PS_DISBALED		0x00
#define PCR_PS_BLOCK			0x01
#define PCR_PS_LEARN			0x02
#define PCR_PS_FORWARD		0x03
#define PCR_FORCE_FC			0x8000

#define PSR_SPD100			0x0100
#define PSR_DUPLEX			0x0200
#define PSR_LINK				0x1000
#define PSR_RESOLVED			0x2000
