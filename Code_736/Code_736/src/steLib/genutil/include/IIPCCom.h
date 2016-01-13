#ifndef IIPCCOM_H_
#define IIPCCOM_H_
typedef enum LINKS
{
 	LINK_C1_COMMAND_C0 = 0,
	LINK_C2_COMMAND_C0,
	LINK_C3_COMMAND_C0,
	LINK_C4_COMMAND_C0,
	LINK_C5_COMMAND_C0,
	LINK_C6_COMMAND_C0,
	LINK_C7_COMMAND_C0,

	LINK_C1_BURSTINFO_C0, // BurstInfo Request from C1 to DDC RX
	LINK_C2_BURSTINFO_C0, // BurstInfo Request from C2 to DDC RX
	LINK_C3_BURSTINFO_C0,  // and so on..
	LINK_C4_BURSTINFO_C0,
	LINK_C5_BURSTINFO_C0,
	LINK_C6_BURSTINFO_C0,
	LINK_C7_BURSTINFO_C0,

	LINK_C1_L1RECEIVER_C0, // Burst From DDC RX to C1 : L2 Pak C1 to IPU
	LINK_C2_L1RECEIVER_C0, // Burst From DDC RX to C2 : L2 Pak C2 to IPU
	LINK_C3_L1RECEIVER_C0, // and so on...
	LINK_C4_L1RECEIVER_C0,
	LINK_C5_L1RECEIVER_C0,
	LINK_C6_L1RECEIVER_C0,
	LINK_C7_L1RECEIVER_C0,


	//LINK_C1_L1TRANSMITTER_C0, // L2 Packet from IPU to C1 : Burst from C1 to FPGa
//	LINK_C2_L1TRANSMITTER_C0, // L2 Packet from IPU to C2 : Burst from C2 to FPGa
//	LINK_C3_L1TRANSMITTER_C0,// L2 Packet from IPU to C3 : Burst from C3 to FPGa
//	LINK_C4_L1TRANSMITTER_C0,// L2 Packet from IPU to C4 : Burst from C4 to FPGa
//	LINK_C5_L1TRANSMITTER_C0,// L2 Packet from IPU to C5 : Burst from C5 to FPGa
//	LINK_C6_L1TRANSMITTER_C0,// L2 Packet from IPU to C6 : Burst from C6 to FPGa
//	LINK_C7_L1TRANSMITTER_C0,// L2 Packet from IPU to C7 : Burst from C7 to FPGa

	MAX_LINKS
} LINKS;


#endif //IIPCCOM_H_