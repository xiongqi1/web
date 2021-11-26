#ifndef __QMIIOCTL_H__
#define __QMIIOCTL_H__


// IOCTL CODE
////////////////////////////////////////////////////////////////////////////////

// IOCTL to generate a client ID for this service type
#define IOCTL_QMI_GET_SERVICE_FILE (0x8BE0 + 1)
// IOCTL to get the VIDPID of the device
#define IOCTL_QMI_GET_DEVICE_VIDPID (0x8BE0 + 2)
// IOCTL to get the MEID of the device
#define IOCTL_QMI_GET_DEVICE_MEID (0x8BE0 + 3)



#endif
