#include<ti\sysbios\knl\Task.h>

#include "Router.h"
#include "CmdPkt.h"
#include "L1Manager.h"
#include "MemMgr.h"
#include "ste_msgQ.h"
#include "UsrConfig.h"
#include <CmdIf.h>
//below #define moved to UsrConfig.h
//#define MAX_RX_CHANNEL	20 //((6 /*Beacon channel*/ * 2 /*Max 2 channel per beacon channel*/ + 8 /* Max traffic channel*/) )
INT8	nMaxRxChannel = 0;
UINT32	nConfigCount=0;
UINT32	nStopCount=0;
UINT32	nSetUnsetTSCCount=0;
UINT8 first_halfrate = 0;

//#pragma DATA_SECTION (RX_Channel_Num, ".rxChannel")


//UINT8 far RX_Channel_Num;


#define MAX_TX_CHANNEL 5
INT8	nMaxTxChannel = 0;

INT8	nAJTxChannel = 0;
INT8	nCBTxChannel = 0;
INT8	nVBTSTxChannel = 0;

#define MAX_HOPPING_CHANNEL 7*7
BOOL	bFreeID[MAX_HOPPING_CHANNEL];


#define MAX_PROCESSING_CORES CORE_7




ChannelKey	gRxRouter[MAX_DSP_CORE][MAX_RX_MGR];
ChannelKey	gTxRouter[MAX_DSP_CORE][MAX_TX_MGR];
extern struct DDCRx	*gDDCRx;
//static ChRouter	gTxRouter[MAX_DSP_CORE][MAX_TX_MGR];


//##for channel release modification
typedef struct DspToIpuHeaderL2
{
	UINT16	nARFCN;
	UINT8	nBAND;
	UINT8	bScanning;
	UINT8	nTS;
	UINT8	nDirection;
	UINT8	nTSC;
	UINT32	nFrameNum;
	UINT8	nChannelType;
	UINT8	nSubSlotNum;
	UINT16	nSNRinQ8;
	UINT16	nRxLevel;
	UINT8	nTOA;
	UINT8	nCipherMode;
	UINT8	nFHEnabled;
	UINT16	nARFCN_FH;
	UINT16	nReceiverID;
	UINT16	nBeaconRef;
} DspToIpuHeaderL2;

//##end




#if ((defined TESTING_CIPHER_WITH_IPU) || (defined TESTING_HOPPING_WITH_IPU))
#define TESTING_WITH_IPU
UINT16 nHSN = 3;
#endif  

#ifdef TESTING_WITH_IPU
VOID Enable_Freq_Hopping(Packet *pPacket)
{
	if((ParamsConfig_GetChannelComb(pPacket) == 1) || (ParamsConfig_GetChannelComb(pPacket) == 7) )
	{
		ParamsConfig_SetBAND(pPacket, 1);
		ParamsConfig_SetBeaconBAND(pPacket, 1);
		ParamsConfig_SetTSC(pPacket, 7);
	#ifdef TESTING_HOPPING_WITH_IPU
		ParamsConfig_SetHopping(pPacket, 1);
		ParamsConfig_SetHSN(pPacket, nHSN); // was 59
		ParamsConfig_SetMAIO(pPacket, 2);
		ParamsConfig_SetNumHopFreq(pPacket, 3);
		ParamsConfig_SetHopARFCN(pPacket, 0, 48);
		ParamsConfig_SetHopARFCN(pPacket, 1, 50);
		ParamsConfig_SetHopARFCN(pPacket, 2, 52);
		nHSN++;
	#endif
	#ifdef TESTING_CIPHER_WITH_IPU
		ParamsConfig_SetCiphering(pPacket, 1);
	#endif 			
	}
}
#define ENABLE_FREQ_HOPPING(pPacket) Enable_Freq_Hopping(pPacket)
#else
#define ENABLE_FREQ_HOPPING(pPacket)
#endif

//#pragma CODE_SECTION(Router_Init, ".textDDR")
VOID Router_Init( VOID )
{

	memset(&gRxRouter, 0,sizeof(gRxRouter));
	memset(&gTxRouter, 0,sizeof(gTxRouter));
#ifdef HANDLE_HOPPING 
    memset(&bFreeID,1,MAX_HOPPING_CHANNEL);
#endif

}

typedef enum ROUTER_MODE
{
	ROUTER_ASSIGN_CHANNEL,
	ROUTER_SEARCH_CHANNEL,
	ROUTER_SEARCH_ASSIGN_CHANNEL,
	ROUTER_DELETE_CHANNEL

}ROUTER_MODE;
BOOL CmdRouter_GetCoreInfoTx( ChannelKey *pChannelKey, ROUTER_MODE eMode, Packet *pPacket, DSP_CORE *pCore );
BOOL CmdRouter_CheckChannelConfig(ChannelKey *pChannelKey,Packet *pPacket);

#ifdef ENABLE_RESET_TRX

	VOID ResetTRXChannels( UINT16 nBeaconARFCN );
#endif


Packet 	 *CmdRouter_MakeFailureSts( Packet *pPacket, PacketType eReason )
{
	Packet *pResponsePacket;
	CmdPkt	oCmdPkt, oStsPkt;
	// Step2: Send response to last received command
#ifndef OLD_IIPC
	pResponsePacket	=	Alloc( MSGTYPE_PACKET );
#else
	pResponsePacket	=	Alloc( gHash[SEG_RECEIVER_IPU_CMDPKT_ID] );
#endif /* OLD_IIPC */

	CmdPkt_Parse(&oCmdPkt, pPacket);
	
	CmdPkt_Make(
		&oStsPkt, 
		pResponsePacket, 
		(PacketReceiver)CmdPkt_GetSender(&oCmdPkt), 
		0, 
		CmdPkt_GetCommand(&oCmdPkt), 
		eReason, 
		CmdPkt_GetSeqNumber(&oStsPkt)
	);
	
	return pResponsePacket;
}

BOOL CmdRouter_CheckChannelConfig(ChannelKey *pChannelKey,Packet *pPacket)
{
	BOOL bChannelConf = FALSE;
	DSP_CORE	eCore;
	UINT8		nRxMgr;
	UINT8		SameFreq;
	UINT8		nHop_Freq_Count;
	//DspToIpuHeaderL2	*pRealHeader;
	Packet *pRealHeader;
	if(pChannelKey->eChannelComb == II)
					{
		if( (ParamsConfig_GetVoEncoderType(pPacket) != 5) && ( ParamsConfig_GetVoEncoderType(pPacket) != TCH_AHS_AMR) ) // kept 5 for GSM HR (Stack sending 5 but we are hardcoding to 3 in Configure channel in Rxmanager.c)
		{
			bChannelConf = TRUE;
			LOG_MSG_CONF("Wrong Vocoder sent %d",ParamsConfig_GetVoEncoderType(pPacket));
			LOG_MSG_CONF("CONFRECV: ARFCN: %d,Band %d TN %d ChComb %d SubSlot %d",\
								pChannelKey->oFreq.nARFCN,pChannelKey->oFreq.nBand,pChannelKey->nTN,\
								pChannelKey->eChannelComb,pChannelKey->nSubchannel);
							LOG_MSG_CONF("-------------------------------------");
						}

					}
	for(nRxMgr = 0; (bChannelConf== FALSE) && (nRxMgr < MAX_RX_MGR); nRxMgr++)
	{
		for(eCore = CORE_1; eCore <=  MAX_PROCESSING_CORES; eCore++)
		{
			if((gRxRouter[eCore][nRxMgr].oFreq.nBand == pChannelKey->oFreq.nBand)&&
				(gRxRouter[eCore][nRxMgr].oFreq.nARFCN == pChannelKey->oFreq.nARFCN) )
				{
					if( gRxRouter[eCore][nRxMgr].nTN & (1<<pChannelKey->nTN) )
					{
						// Check hopping parameters
						if((gRxRouter[eCore][nRxMgr].nHSN == pChannelKey->nHSN) && \
						(gRxRouter[eCore][nRxMgr].nMAIO == pChannelKey->nMAIO))
						{
							if(gRxRouter[eCore][nRxMgr].nFreqNum == pChannelKey->nFreqNum)
							{

								SameFreq = memcmp(&gRxRouter[eCore][nRxMgr].nHopFreq[0],&pChannelKey->nHopFreq[0],pChannelKey->nFreqNum);
								if(SameFreq == 0)
								 {
										if(pChannelKey->eChannelComb == II)
										{
											if(pChannelKey->nSubchannel == gRxRouter[eCore][nRxMgr].nSubchannel)
											{
												bChannelConf = TRUE;
			#if 1
											/*	Eth_Debug((CHAR *)"CH CONF ALREADY ARFCN %d TN %d ChannelComb %d SubCh %d ", \
												pChannelKey->oFreq.nARFCN,pChannelKey->nTN,pChannelKey->eChannelComb,pChannelKey->nSubchannel);
												MSG_BOX("earlier it was routed to core %d RXMGR %d, RECVID %d",eCore,nRxMgr,gRxRouter[eCore][nRxMgr].nID); */
												LOG_MSG_CONF("CH CONF ALREADY ARFCN %d TN %d ChannelComb %d SubCh %d HSN %d MAIO %d", \
														pChannelKey->oFreq.nARFCN,pChannelKey->nTN,pChannelKey->eChannelComb,pChannelKey->nSubchannel,pChannelKey->nHSN,pChannelKey->nMAIO);
												LOG_MSG_CONF("earlier it was routed to core %d RXMGR %d, RECVID %d",eCore,nRxMgr,gRxRouter[eCore][nRxMgr].nID);
														for( nHop_Freq_Count = 0; nHop_Freq_Count < ParamsConfig_GetNumHopFreq(pPacket); nHop_Freq_Count++ )
														{
															LOG_MSG_CONF("Hopping ARFCN: Stored %d Received %d",gRxRouter[eCore][nRxMgr].nHopFreq[nHop_Freq_Count],pChannelKey->nHopFreq[nHop_Freq_Count]);
														}

			#endif
												break;
											}
										}
//########## condition where signaling configuration came same same parameters of a already configured traffic		 ###########

										else if((pChannelKey->eChannelComb == VII) && (gRxRouter[eCore][nRxMgr].eChannelComb != VII))
										{
													LOG_MSG_CONF("### Stopping traffic for signaling ###");
													// send channel release for traffic.
													pRealHeader = CmdIf_AllocPacket(CmdIf_GetHandler());
													pRealHeader->Header.nIdentity	 = 0x80;
													pRealHeader->Header.nCommand	 = 0x00;
													pRealHeader->Header.nSeqNum	 	 = 0x00;
													pRealHeader->Header.nByteCnt	 = 0x30;
			//############## ###################################
			//please check function "DataPkt_MakeL2DspToIpuHeader "
			//to get more details for the byte and data order given below
			//############################################################
											 if(pRealHeader!= NULL)
											 {
										//###refer "DataPkt_MakeType1_2"	for below 4 bytes
										//paylod information
													pRealHeader->nData[0]= 0x01;  //packet type
													pRealHeader->nData[1]= 0x1E; //channel validity flag
													pRealHeader->nData[2]=0; // reserved
													pRealHeader->nData[3]=0; // reserved
									//##please check function "DataPkt_MakeL2DspToIpuHeader for below bytes
									// ID1 2 bytes
													pRealHeader->nData[5]    = (UINT8) (gRxRouter[eCore][nRxMgr].oFreq.nARFCN);
													pRealHeader->nData[4]    = (UINT8)(((gRxRouter[eCore][nRxMgr].oFreq.nARFCN) & 0x1F00) >> 8);
													pRealHeader->nData[4]   |= (UINT8)(((pChannelKey->oFreq.nBand) & 0x7) << 5);

										// ID2 1 byte
													pRealHeader->nData[6]    = (UINT8)0;
													pRealHeader->nData[6]   |= (UINT8)((pChannelKey->nTN) & 0x7)<<1;
													pRealHeader->nData[6]   |= (UINT8)(((0x0) & 0x1) << 4);
													pRealHeader->nData[6]   |= (UINT8)(0x00);

										// ID3 4 bytes
													pRealHeader->nData[10]    = (UINT8)(0x10); //frame num
													pRealHeader->nData[9]    = (UINT8)(0x00);
													pRealHeader->nData[8]    = (UINT8)(0x00);
													pRealHeader->nData[7]    = (UINT8)(0x7 & 0x1F);  // channel type
													pRealHeader->nData[7]   |= (UINT8)(((gRxRouter[eCore][nRxMgr].nSubchannel) & 0x7) << 5);

										// ID4 2 bytes
													pRealHeader->nData[12]    = (UINT8)0x00; //snr
													pRealHeader->nData[11]    = (UINT8)(0x00);

										// ID5 2 bytes
													pRealHeader->nData[14]    = (UINT8)(0x00);
													pRealHeader->nData[13]     = (UINT8)(0x00);

										// ID6 1 byte
													pRealHeader->nData[15]   = (UINT8)(0x00);

										// ID7 2 bytes
													pRealHeader->nData[17]   = (UINT8)(0x00);
													pRealHeader->nData[17]  |= (UINT8)(0x00);
													pRealHeader->nData[17]  |= (UINT8)(0x00);
													pRealHeader->nData[16]   = (UINT8)(0x00);

										// ID8 2 bytes
													pRealHeader->nData[19]   = (UINT8)(gRxRouter[eCore][nRxMgr].nID);
													pRealHeader->nData[18]   = (UINT8)(((gRxRouter[eCore][nRxMgr].nID) & 0xFF00) >> 8);

										// ID9 2 bytes
													pRealHeader->nData[21]   = (UINT8)((gRxRouter[eCore][nRxMgr].oBeaconFreq.nARFCN | gRxRouter[eCore][nRxMgr].oBeaconFreq.nBand)<< 13);
													pRealHeader->nData[20]   = (UINT8)((((gRxRouter[eCore][nRxMgr].oBeaconFreq.nARFCN | gRxRouter[eCore][nRxMgr].oBeaconFreq.nBand)<< 13) & 0xFF00) >> 8);

										//Insert 06 6D here
													pRealHeader->nData[22]= 0x06;
													pRealHeader->nData[23]= 0x0D;
													pRealHeader->nData[24]= 0x2B;
													pRealHeader->nData[25]= 0x2B;

													if(gRxRouter[eCore][nRxMgr].eChannelComb == I)
													{
														// send to stack
														Transmit_Mesg(CORE_0,DATA,pRealHeader);
														break;
													}
													if(gRxRouter[eCore][nRxMgr].eChannelComb == II)
													{
														// send to stack and continue for next suslot
														Transmit_Mesg(CORE_0,DATA,pRealHeader);
														if(first_halfrate)
														{
															first_halfrate=0;
															break;
														}
														else
														{
															first_halfrate=1;
															continue;
														}
													}
													else
													{
														Free(0,pRealHeader);
													}
											 }

										 }
//#########                 end of modification ##############
										 else
										 {
												LOG_MSG_CONF("CH CONF ALREADY ARFCN %d TN %d ChannelComb %d HSN %d MAIO %d", \
												pChannelKey->oFreq.nARFCN,pChannelKey->nTN,pChannelKey->eChannelComb,pChannelKey->nHSN,pChannelKey->nMAIO);
												LOG_MSG_CONF("earlier it was routed to core %d RXMGR %d, RECVID %d",eCore,nRxMgr,gRxRouter[eCore][nRxMgr].nID);
												for( nHop_Freq_Count = 0; nHop_Freq_Count < ParamsConfig_GetNumHopFreq(pPacket); nHop_Freq_Count++ )
												{
													LOG_MSG_CONF("Hopping ARFCN: Stored %d Received %d",gRxRouter[eCore][nRxMgr].nHopFreq[nHop_Freq_Count],pChannelKey->nHopFreq[nHop_Freq_Count]);
												}
												bChannelConf = TRUE;
												break;
										}
							    } // if(SameFreq == 0)
							}
						}
					}
				}
			}
		}

	    first_halfrate=0;
		return bChannelConf;
}

BOOL CmdRouter_GetCoreInfo( ChannelKey *pChannelKey, ROUTER_MODE eMode, Packet *pPacket, DSP_CORE *pCore )
{
	DSP_CORE	eCore;
	UINT8		nRxMgr;
	BOOL		bDone = FALSE;
	BOOL		Is_Hopped = FALSE;
	UINT8		Sub_Slot = 0;
	UINT8		nHopFreq_Count = 0;
	BOOL		bChannelConf = FALSE;

	if(eMode == ROUTER_ASSIGN_CHANNEL)
	{
		bChannelConf = CmdRouter_CheckChannelConfig(pChannelKey,pPacket);
		if(bChannelConf == TRUE)
		{
//Channel already Configured , so freeing the assigned ID
			if( pChannelKey->nID > 0 )
			{
				bFreeID[pChannelKey->nID]= 1;
			}
			return FALSE;
		}
	}

	if( nMaxRxChannel > MAX_RX_CHANNEL )
	{		
		
		if( ( eMode == ROUTER_ASSIGN_CHANNEL ) || (eMode == ROUTER_SEARCH_ASSIGN_CHANNEL))
		{
			LOG_MSG_CONF( "CmdRouter: Routing for Cmd(%d) failed and nMaxRxChannel: %d", pPacket->Header.nCommand, nMaxRxChannel );

			LOG_MSG_CONF("Routing for Cmd(%d) failed!! nMaxRxChannel: %d", pPacket->Header.nCommand, nMaxRxChannel );

			Params_SetReceiverID(pPacket,0);
			bFreeID[pChannelKey->nID] = 1;
			return FALSE;
		}
		
	}
	
	Is_Hopped = ParamsConfig_GetHopping(pPacket);
	if(eMode == ROUTER_ASSIGN_CHANNEL)
					{
						Sub_Slot = ParamsConfig_GetSubSlot(pPacket);
					}
					else if(eMode == ROUTER_DELETE_CHANNEL)
					{
						Sub_Slot = ParamsStop_GetSubSlot(pPacket);
					}
					else if(eMode == ROUTER_SEARCH_CHANNEL)
					{
						Sub_Slot = ParamsTsc_GetSubSlot(pPacket);
					}


	switch( eMode )
	{
		case ROUTER_ASSIGN_CHANNEL:
			for(nRxMgr = 0; (bDone== FALSE) && (nRxMgr < MAX_RX_MGR); nRxMgr++)
			{
				for(eCore = CORE_1; eCore <=  MAX_PROCESSING_CORES; eCore++)
				{
					if( gRxRouter[eCore][nRxMgr].nTN == 0  )
					{
						LOG_MSG_CONF("CONFRECV: ARFCN: %d,Band %d TN %d SubSlot %d ChComb %d nRxMgr = %d Core %d nRecvId = %d isHopped:%d nMax %d",\
						pChannelKey->oFreq.nARFCN, pChannelKey->oFreq.nBand,pChannelKey->nTN,Sub_Slot,\
						pChannelKey->eChannelComb,nRxMgr,eCore,pPacket->nData[3],Is_Hopped,(nMaxRxChannel) + 1);
						LOG_MSG_CONF("---------------------------------------------");

//						Eth_Debug((CHAR *)"Core_0 ConfigCount %d Configuring ARFCN: %d,Band %d TN %d SubSlot %d ChComb %d nRxMgr = %d Core %d ID = %d",\
//								nConfigCount,pChannelKey->oFreq.nARFCN, pChannelKey->oFreq.nBand,pChannelKey->nTN,pChannelKey->nSubchannel,\
//								pChannelKey->eChannelComb,nRxMgr,eCore,pChannelKey->nID);

						gRxRouter[eCore][nRxMgr].nTN |= (1<<pChannelKey->nTN);
						gRxRouter[eCore][nRxMgr].oFreq	=	pChannelKey->oFreq;
						gRxRouter[eCore][nRxMgr].nSubchannel = pChannelKey->nSubchannel;
						gRxRouter[eCore][nRxMgr].oBeaconFreq	=	pChannelKey->oBeaconFreq;
						gRxRouter[eCore][nRxMgr].nID			=	pChannelKey->nID;
						gRxRouter[eCore][nRxMgr].nHSN			= pChannelKey->nHSN;
						gRxRouter[eCore][nRxMgr].nMAIO			= pChannelKey->nMAIO;
						gRxRouter[eCore][nRxMgr].eChannelComb   = pChannelKey->eChannelComb;
						gRxRouter[eCore][nRxMgr].nFreqNum		= pChannelKey->nFreqNum;

						for( nHopFreq_Count = 0; nHopFreq_Count < ParamsConfig_GetNumHopFreq(pPacket); nHopFreq_Count++ )
						{
							gRxRouter[eCore][nRxMgr].nHopFreq[nHopFreq_Count] = ParamsConfig_GetHopARFCN(pPacket, nHopFreq_Count);
						}

						if( (pChannelKey->eChannelComb == 4) || (pChannelKey->eChannelComb == 5))
							gRxRouter[eCore][nRxMgr].bIsBeacon = TRUE;
						else
							gRxRouter[eCore][nRxMgr].bIsBeacon = FALSE;
						pPacket->nRxMgr = nRxMgr;
						bDone = TRUE;
						nMaxRxChannel++;

						break;
					}
				}
				
			}
			if(bDone == FALSE)
			{
				Eth_Debug((CHAR *)"CmdRouter : No SPU Resources");
			}

		break;

		case ROUTER_SEARCH_ASSIGN_CHANNEL:
		for(nRxMgr = 0; (bDone== FALSE) && (nRxMgr < MAX_RX_MGR); nRxMgr++)
			{
				for(eCore = CORE_1; eCore <= MAX_PROCESSING_CORES; eCore++)
				{
					if(( gRxRouter[eCore][nRxMgr].oFreq.nBand == pChannelKey->oFreq.nBand)&&
						( gRxRouter[eCore][nRxMgr].oFreq.nARFCN == pChannelKey->oFreq.nARFCN))
					{
						if( !(gRxRouter[eCore][nRxMgr].nTN & (1<<pChannelKey->nTN)) )
						{
							gRxRouter[eCore][nRxMgr].nTN |= (1<<pChannelKey->nTN);
							pPacket->nRxMgr = nRxMgr;
							bDone = TRUE;
							nMaxRxChannel++;
					//		LOG_DUMP("nMaxRxChannel %d",nMaxRxChannel);
//							Eth_Debug((CHAR *)"Routing CMD %d to CORE %d",pPacket->Header.nCommand, eCore);
							break;
						}
					
					}
				}
				
			}
		break;
		case ROUTER_SEARCH_CHANNEL:
			for(nRxMgr = 0; (bDone== FALSE) && (nRxMgr < MAX_RX_MGR); nRxMgr++)
			{
				for(eCore = CORE_1; eCore <=  MAX_PROCESSING_CORES; eCore++)
				{
					if(( gRxRouter[eCore][nRxMgr].oFreq.nBand == pChannelKey->oFreq.nBand)&&
						( gRxRouter[eCore][nRxMgr].oFreq.nARFCN == pChannelKey->oFreq.nARFCN) &&
						(gRxRouter[eCore][nRxMgr].eChannelComb == pChannelKey->eChannelComb ) )
					{
						if( gRxRouter[eCore][nRxMgr].nTN & (1<<pChannelKey->nTN) )
						{
							if(gRxRouter[eCore][nRxMgr].nID == pChannelKey->nID )
							{
							pPacket->nRxMgr = nRxMgr;
							bDone = TRUE;
							if(ParamsTsc_IsUnSetTSC(pPacket) == TRUE)
							{
							LOG_MSG_CONF("UNSET TSC: ARFCN: %d,Band %d TN %d SubSlot %d ChComb %d nRxMgr = %d Core %d nRecvId = %d nMax %d",\
													pChannelKey->oFreq.nARFCN, pChannelKey->oFreq.nBand,pChannelKey->nTN,Sub_Slot,\
													pChannelKey->eChannelComb,nRxMgr,eCore,pChannelKey->nID,nMaxRxChannel );
							LOG_MSG_CONF("---------------------------------------------");
							}
							else
							{
								if(pPacket->nCommand == IPU_TO_DSP_SET_TSC_FOR_RECEIVER)
								{
								LOG_MSG_CONF("SET TSC: ARFCN: %d,Band %d TN %d SubSlot %d ChComb %d nRxMgr = %d Core %d nRecvId = %d nMax %d",\
														pChannelKey->oFreq.nARFCN, pChannelKey->oFreq.nBand,pChannelKey->nTN,Sub_Slot,\
														pChannelKey->eChannelComb,nRxMgr,eCore,pChannelKey->nID,nMaxRxChannel );
								LOG_MSG_CONF("---------------------------------------------");
								}
							}
							break;
							}
						}
					
					}
				}
				
			}

				if(bDone == FALSE)
				{
					if(ParamsTsc_IsUnSetTSC(pPacket) == TRUE)
					{
					LOG_MSG_CONF("UNSET TSC FAIL: ARFCN: %d,Band %d TN %d SubSlot %d ChComb %d nRxMgr = %d Core %d nRecvId = %d nMax %d",\
											pChannelKey->oFreq.nARFCN, pChannelKey->oFreq.nBand,pChannelKey->nTN,Sub_Slot,\
											pChannelKey->eChannelComb,nRxMgr,eCore,pChannelKey->nID,nMaxRxChannel );
					LOG_MSG_CONF("---------------------------------------------");
					}
					else
					{
						LOG_MSG_CONF("SET TSC FAIL: ARFCN: %d,Band %d TN %d SubSlot %d ChComb %d nRxMgr = %d Core %d nRecvId = %d nMax %d",\
												pChannelKey->oFreq.nARFCN, pChannelKey->oFreq.nBand,pChannelKey->nTN,Sub_Slot,\
												pChannelKey->eChannelComb,nRxMgr,eCore,pChannelKey->nID,nMaxRxChannel );
						LOG_MSG_CONF("---------------------------------------------");
					}
				}
		break;

		case ROUTER_DELETE_CHANNEL:
			for(nRxMgr = 0; (bDone== FALSE) && (nRxMgr < MAX_RX_MGR); nRxMgr++)
			{
				for(eCore = CORE_1; eCore <=  MAX_PROCESSING_CORES; eCore++)
				{
					if(( gRxRouter[eCore][nRxMgr].oFreq.nBand == pChannelKey->oFreq.nBand)&&
						( gRxRouter[eCore][nRxMgr].oFreq.nARFCN == pChannelKey->oFreq.nARFCN) &&
						(gRxRouter[eCore][nRxMgr].eChannelComb == pChannelKey->eChannelComb )  )
					{

						if( gRxRouter[eCore][nRxMgr].nTN & (1<<pChannelKey->nTN) )
						{

								if(gRxRouter[eCore][nRxMgr].nID == pChannelKey->nID )
								{
									if(pChannelKey->eChannelComb == II)
									{
										if(gRxRouter[eCore][nRxMgr].nSubchannel == pChannelKey->nSubchannel)
										{
//											Eth_Debug((CHAR *)"Core_0 StopCount %d DeConfiguring ARFCN: %d,Band %d TN %d SubSlot %d ChComb %d nRxMgr = %d Core %d ID = %d",\
//													nStopCount,pChannelKey->oFreq.nARFCN, pChannelKey->oFreq.nBand,pChannelKey->nTN,pChannelKey->nSubchannel,\
//													pChannelKey->eChannelComb,nRxMgr,eCore,pChannelKey->nID);
								LOG_MSG_CONF("STOPRECV: ARFCN: %d,Band %d TN %d SubSlot %d ChComb %d nRxMgr = %d Core %d nRecvId = %d nMax %d",\
								pChannelKey->oFreq.nARFCN, pChannelKey->oFreq.nBand,pChannelKey->nTN,Sub_Slot,\
								pChannelKey->eChannelComb,nRxMgr,eCore,pPacket->nData[3],(nMaxRxChannel) - 1);
								LOG_MSG_CONF("---------------------------------------------");

											if( pChannelKey->nID > 0 )
											{
												bFreeID[pChannelKey->nID]= 1;
											}
											gRxRouter[eCore][nRxMgr].nID = 0;
											pPacket->nRxMgr = nRxMgr;
											pPacket->nCoreno = eCore;

											bDone = TRUE;
											if(pPacket->Header.nCommand != IPU_TO_DSP_STOP_SCANING_BAND)
											{
											CmdRouter_DeleteChannel(pPacket);
											}

											LOG_DUMP("STOPRECV: ARFCN: %d,TN %d ChComb %d SubCh %d nRxMgr %d eCore = %d", \
											pChannelKey->oFreq.nARFCN,pChannelKey->nTN, \
											pChannelKey->eChannelComb, pChannelKey->nSubchannel,nRxMgr,eCore);
											break;
										}
									}
									else
									{
//										Eth_Debug((CHAR *)"Core_0 StopCount %d DeConfiguring ARFCN: %d,Band %d TN %d SubSlot %d ChComb %d nRxMgr = %d Core %d ID = %d",\
//												nStopCount,pChannelKey->oFreq.nARFCN, pChannelKey->oFreq.nBand,pChannelKey->nTN,pChannelKey->nSubchannel,\
//												pChannelKey->eChannelComb,nRxMgr,eCore,pChannelKey->nID);
								LOG_MSG_CONF("STOPRECV: ARFCN: %d,Band %d TN %d SubSlot %d ChComb %d nRxMgr = %d Core %d nRecvId = %d nMax %d",\
								pChannelKey->oFreq.nARFCN, pChannelKey->oFreq.nBand,pChannelKey->nTN,Sub_Slot,\
								pChannelKey->eChannelComb,nRxMgr,eCore,pPacket->nData[3],(nMaxRxChannel) - 1);
								LOG_MSG_CONF("---------------------------------------------");

										if( pChannelKey->nID > 0 )
										{
											bFreeID[pChannelKey->nID]= 1;
										}
										gRxRouter[eCore][nRxMgr].nID = 0;

										if( (pChannelKey->eChannelComb == 4) || (pChannelKey->eChannelComb == 5))
										{
											gRxRouter[eCore][nRxMgr].bIsBeacon = FALSE;
										//	Eth_Debug((CHAR *)"Beacon channel %d DeConfigured",gRxRouter[eCore][nRxMgr].oFreq.nARFCN);
										}

										pPacket->nRxMgr = nRxMgr;
										pPacket->nCoreno = eCore;

										bDone = TRUE;

										if(pPacket->Header.nCommand != IPU_TO_DSP_STOP_SCANING_BAND)
										{
										CmdRouter_DeleteChannel(pPacket);
										}


										LOG_DUMP("STOPRECV: ARFCN: %d,TN %d ChComb %d SubCh %d nRxMgr %d eCore = %d", \
										pChannelKey->oFreq.nARFCN,pChannelKey->nTN, \
										pChannelKey->eChannelComb, pChannelKey->nSubchannel,nRxMgr,eCore);

										break;
									}
								}
						}
					}//if((gRxRouter[eCore][nRxMgr].oFreq.nBand == pChannelKey->oFreq.nBand)&&
				}//for(eCore = CORE_1; eCore <=  MAX_PROCESSING_CORES; eCore++)
				
			}
			if(bDone == FALSE)
			{
				LOG_MSG_CONF("STOPRECV FAIL: ARFCN: %d,Band %d TN %d SubSlot %d ChComb %d nRecvId = %d",\
				pChannelKey->oFreq.nARFCN, pChannelKey->oFreq.nBand,pChannelKey->nTN,Sub_Slot,\
				pChannelKey->eChannelComb,pPacket->nData[3]);
				LOG_MSG_CONF("---------------------------------------------");
			}
		break;
	}
	
	if(bDone == TRUE )
	{
		*pCore = eCore;
	}
	else
	{
		if( pChannelKey->eChannelComb == 7 ) // if channel comb 7 is found in beacon channel assign separate channel
		{
			if( eMode == ROUTER_SEARCH_ASSIGN_CHANNEL )
			{
				bDone = CmdRouter_GetCoreInfo(pChannelKey, ROUTER_ASSIGN_CHANNEL, pPacket, pCore);
			}	
		}
	}

#if 0
	if(bDone == FALSE )
	{
		Eth_Debug((CHAR *) "###CmdRouter: Routing cmd %d Failed###", pPacket->Header.nCommand);
		Params_SetReceiverID(pPacket,0);
		bFreeID[pChannelKey->nID] = 1;
	}
#endif

	if( nMaxRxChannel < 0 )
	{
		Eth_Debug((CHAR *) "#### CmdRouter: MaxRxChannel Less than Zero nMaxRxChannel: %d ####", nMaxRxChannel );
		return FALSE;
	}

	return bDone;
}


BOOL CmdRouter_MapRxCmdToCore(Packet	*pPacket, DSP_CORE *pCore )
{
	CmdPkt		oCmdPkt;

	ROUTER_MODE		eRouterMode;
	CommandType		nCmd;
	ChannelKey		oChannelKey;
	BOOL			bHoppingEnabled = FALSE;
	UINT8			nHopFreqCount;

	// parse command
	CmdPkt_Parse(&oCmdPkt, pPacket);

	nCmd = CmdPkt_GetCommand(&oCmdPkt);

	switch( nCmd )
	{
		case IPU_TO_DSP_SCAN_BEACON_FREQ:
		case IPU_TO_DSP_STOP_SCANING_BAND:
			{
				//Packet *pPacket = oCmdPkt.pPacket;
				
				oChannelKey.nTN = 0;
				
				oChannelKey.eChannelComb 	= IV;
				
				oChannelKey.oFreq.nBand 	= ParamsFreq_GetBAND(pPacket);  
				oChannelKey.oFreq.nARFCN 	= 65535;
				oChannelKey.nID		= 0;
				oChannelKey.nSubchannel = 255; 		//Invalid
				
				if( nCmd == IPU_TO_DSP_SCAN_BEACON_FREQ )
				{
					nConfigCount++;
					Params_SetReceiverID(pPacket,0);
					eRouterMode = ROUTER_ASSIGN_CHANNEL;
				}
				else
				{
					nStopCount++;
					eRouterMode = ROUTER_DELETE_CHANNEL;	
				}

				return CmdRouter_GetCoreInfo(&oChannelKey, eRouterMode, oCmdPkt.pPacket, pCore);
			}

		case IPU_TO_DSP_STOP_SCANNING_ARFCN:
			{
				nStopCount++;
				oChannelKey.nTN = 0;
				
				oChannelKey.eChannelComb 	= IV;
				
				oChannelKey.oFreq.nBand 	= ParamsArfcn_GetBAND(pPacket); 
				oChannelKey.oFreq.nARFCN 	= 65535;
				oChannelKey.nID		= 0;
				
				eRouterMode = ROUTER_SEARCH_CHANNEL;
				
				return CmdRouter_GetCoreInfo(&oChannelKey, eRouterMode, oCmdPkt.pPacket, pCore);
				
			}


		case IPU_TO_DSP_CONFIGURE_RECEIVER:
			{
				Packet *pPacket = oCmdPkt.pPacket;
				
				nConfigCount++;
				if(ParamsConfig_GetVoEncoderType(pPacket) == 4)
				{
					if(ParamsConfig_GetChannelComb(pPacket) == 1)
					{

						MSG_BOX("CONFRECV Sent Vocoder Type IV but ChannelComb 1 Changing to II");

						ParamsConfig_SetChannelComb(pPacket,2);
						oChannelKey.eChannelComb = II; // Setting it as ChannelComb 2 
					}
				}
				oChannelKey.eChannelComb 		= (CHANNEL_COMB)ParamsConfig_GetChannelComb(pPacket);
				oChannelKey.nTN 				= ParamsConfig_GetTS(pPacket);
				oChannelKey.oFreq.nBand 		= ParamsConfig_GetBAND(pPacket);
				oChannelKey.oFreq.nARFCN 		= ParamsConfig_GetARFCN(pPacket);
				oChannelKey.oBeaconFreq.nBand	=	ParamsConfig_GetBeaconBAND(pPacket);
				oChannelKey.oBeaconFreq.nARFCN	=	ParamsConfig_GetBeaconARFCN(pPacket);
				oChannelKey.nHSN 				= ParamsConfig_GetHSN(pPacket);
				oChannelKey.nMAIO				= ParamsConfig_GetMAIO(pPacket);
				oChannelKey.nFreqNum			= ParamsConfig_GetNumHopFreq(pPacket);
				
				for( nHopFreqCount = 0; nHopFreqCount < ParamsConfig_GetNumHopFreq(pPacket); nHopFreqCount++ )
				{
					oChannelKey.nHopFreq[nHopFreqCount] = ParamsConfig_GetHopARFCN(pPacket, nHopFreqCount);
				}

				if(oChannelKey.eChannelComb == II)
					oChannelKey.nSubchannel = ParamsConfig_GetSubSlot(pPacket);
				else
					oChannelKey.nSubchannel = 255; //Invalid
				ENABLE_FREQ_HOPPING(pPacket);
				bHoppingEnabled = ParamsConfig_GetHopping(pPacket);
				eRouterMode = ROUTER_ASSIGN_CHANNEL;

#ifdef HANDLE_HOPPING
				Params_SetReceiverID(pPacket,0);  // hopping ID set = 0 by default
				oChannelKey.nID = 0;
				// edited by kausik,25jan for chan comb 1/2,receiver id starts from 4 
					if(bHoppingEnabled == TRUE )
					{
						UINT8 nID;
						for(nID = 1; nID < MAX_HOPPING_CHANNEL ; nID++ )
						{
							if(bFreeID[nID] == 1 )
							{
								
								oChannelKey.nID = nID;
								Params_SetReceiverID(pPacket,nID);
								bFreeID[nID] = 0;
								break;
		
							}
						}
						
					}


#endif
				if( (oChannelKey.eChannelComb == 4 ) || (oChannelKey.eChannelComb == 5 ) )
				{
					oChannelKey.bIsBeacon = TRUE;
				}
				return CmdRouter_GetCoreInfo(&oChannelKey, eRouterMode, oCmdPkt.pPacket, pCore);
				
			}

		case IPU_TO_DSP_SET_TSC_FOR_RECEIVER:
		case IPU_TO_DSP_SET_TSC_FOR_VBTS_MODE:
			{
				Packet *pPacket = oCmdPkt.pPacket;
				nSetUnsetTSCCount++;
				
				oChannelKey.nTN = ParamsTsc_GetTS(pPacket);
				
				oChannelKey.eChannelComb 	= (CHANNEL_COMB)ParamsTsc_GetChannelComb(pPacket);
				
				oChannelKey.oFreq.nBand 	= ParamsTsc_GetBAND(pPacket);
				oChannelKey.oFreq.nARFCN 	= ParamsTsc_GetARFCN(pPacket);
				oChannelKey.nSubchannel     = ParamsTsc_GetSubSlot(pPacket);
#ifdef HANDLE_HOPPING
			oChannelKey.nID = 	Params_GetReceiverID(pPacket);
	
#endif
				eRouterMode = ROUTER_SEARCH_CHANNEL;	
			
				return CmdRouter_GetCoreInfo(&oChannelKey, eRouterMode, oCmdPkt.pPacket, pCore);
				
			}

		case IPU_TO_DSP_CHANNEL_MODIFY:
			{
				Packet *pPacket = oCmdPkt.pPacket;
				
				oChannelKey.nTN = ParamsCHM_GetTS(pPacket);	
				
				oChannelKey.eChannelComb 	= (CHANNEL_COMB)ParamsCHM_GetCHComb(pPacket);
				
				oChannelKey.oFreq.nBand 	= ParamsCHM_GetBAND(pPacket);
				oChannelKey.oFreq.nARFCN 	= ParamsCHM_GetARFCN(pPacket);
#ifdef HANDLE_HOPPING
			oChannelKey.nID = 	Params_GetReceiverID(pPacket);
#endif			
				eRouterMode = ROUTER_SEARCH_CHANNEL;	
			
				return CmdRouter_GetCoreInfo(&oChannelKey, eRouterMode, oCmdPkt.pPacket, pCore);
				
			}

		case IPU_TO_DSP_STOP_RECEIVER:
		case IPU_TO_DSP_STOP_VBTS_MODE:
			{
				Packet *pPacket = oCmdPkt.pPacket;
				nStopCount++;
				
				oChannelKey.nTN = ParamsStop_GetTS(pPacket);
				
				oChannelKey.eChannelComb 	= (CHANNEL_COMB)ParamsStop_GetChannelComb(pPacket);
				
				oChannelKey.oFreq.nBand 	= ParamsStop_GetBAND(pPacket);
				oChannelKey.oFreq.nARFCN 	= ParamsStop_GetARFCN(pPacket);
				if(oChannelKey.eChannelComb == II)
					oChannelKey.nSubchannel = 	ParamsStop_GetSubSlot(pPacket);	
				else
					oChannelKey.nSubchannel = 255; //invalid
				
				eRouterMode = ROUTER_DELETE_CHANNEL;
#ifdef HANDLE_HOPPING
			    oChannelKey.nID = 	Params_GetReceiverID(pPacket);
#endif
				if( (oChannelKey.eChannelComb == 4 ) || (oChannelKey.eChannelComb == 5 ) )
				{
					oChannelKey.bIsBeacon = FALSE;
				}

				if(nCmd == IPU_TO_DSP_STOP_RECEIVER)
				{
#ifdef Driver_modified // ##### bcz of driver errors, have to modify the code
					LOG_MSG_PM("No of Receivers Configured id %d Transmitters configured is %d",nMaxRxChannel,nMaxTxChannel);
#endif
				}
#ifdef ENABLE_RESET_TRX
		if( ParamsStop_GetResetTRX (pPacket) == TRUE )
		{
				ResetTRXChannels( ParamsStop_GetARFCN(pPacket) );
		}
		{
			BOOL bStatus;
			bStatus = CmdRouter_GetCoreInfo(&oChannelKey, eRouterMode, oCmdPkt.pPacket, pCore);
			*pCore = BOTH_COREFLAG;
			return bStatus;
		}
#else
			return CmdRouter_GetCoreInfo(&oChannelKey, eRouterMode, oCmdPkt.pPacket, pCore);
#endif
				
			}
		case DSP_TO_FPGA_RX_TUNE:
			{
				UINT8		nSeqNum;
				nSeqNum	=	CmdPkt_GetSeqNumber(&oCmdPkt);
	 			*pCore	=	(DSP_CORE)RXTUNE_GET_CORE_FROM_SEQNUM(nSeqNum); //((nSeqNum & 0XC0)>>6);
				oCmdPkt.pPacket->nRxMgr = RXTUNE_GET_RXMGRID_FROM_SEQNUM(nSeqNum); //(nSeqNum & 0X38 ) >> 3;
				//LOG_DUMP( "CmdRouter: RxTune sts to RxMgr(%d) to Core(%d)", oCmdPkt.pPacket->nRxMgr, *pCore  );
				return TRUE;
			}

		case DSP_TO_FPGA_CONFIG_SRIO:
			{
				*pCore	= (DSP_CORE)CORE_0;
				return TRUE;
			}

		case DSP_TO_FPGA_SET_DDC:
			{
				*pCore	= (DSP_CORE)INVALID_CORE;
				return TRUE;
			}
			
		case DSP_TO_FPGA_SET_DDC2_CONFIGURATION:
			{
				*pCore	= (DSP_CORE)INVALID_CORE;
				return TRUE;
			}





		default:
			return FALSE;

	}
		
}

BOOL CmdRouter_UpdateRxCmdRouterOnResponse(  Packet	*pPacket )
{

	CmdPkt			oCmdPkt;
	DSP_CORE		eTargetCore;
	ROUTER_MODE		eRouterMode;

	ChannelKey		oChannelKey;

	// parse command
	CmdPkt_Parse(&oCmdPkt, pPacket);
//TODO???????????????????????// need to handle failure exeuction of all the command */
	switch( CmdPkt_GetCommand(&oCmdPkt) )
	{
		case IPU_TO_DSP_SCAN_BEACON_FREQ:
			{
				//Packet *pPacket = oCmdPkt.pPacket;
				
				oChannelKey.nTN = 0;
				
				oChannelKey.eChannelComb 	= IV;
				
				oChannelKey.oFreq.nBand 	= ParamsFreq_GetBAND(pPacket);

				oChannelKey.oFreq.nARFCN 	= 65535;
				oChannelKey.nSubchannel 	= 255;
				eRouterMode = ROUTER_DELETE_CHANNEL;
				
				Params_SetReceiverID(pPacket,0);	
				oChannelKey.nID		= 0;

				return CmdRouter_GetCoreInfo(&oChannelKey, eRouterMode, oCmdPkt.pPacket, &eTargetCore);
			}
/*	@@@mani,	case IPU_TO_DSP_CONFIGURE_TRANSMITTER:
			{
				// TODO: This function is called when the command id is success.
				// but this should be call irrespective of status.
				// distinguish pass/fail case here.
				Packet *pPacket = oCmdPkt.pPacket;
				
				oChannelKey.nTN = ParamsStop_GetTS(pPacket);
				
				oChannelKey.eChannelComb 	= (CHANNEL_COMB)ParamsStop_GetChannelComb(pPacket);
				
				oChannelKey.oFreq.nBand 	= ParamsStop_GetBAND(pPacket);
				oChannelKey.oFreq.nARFCN 	= ParamsStop_GetARFCN(pPacket);
				
				eRouterMode = ROUTER_DELETE_CHANNEL;	
			
				return CmdRouter_GetCoreInfoTx(&oChannelKey, eRouterMode, oCmdPkt.pPacket,  &eTargetCore);
				
			}
*/
		default:
		return FALSE;
	}
}


BOOL CmdRouter_GetCoreInfoTx( ChannelKey *pChannelKey, ROUTER_MODE eMode, Packet *pPacket, DSP_CORE *pCore )
{
	DSP_CORE	eCore;
	UINT8		nTxMgr;
	BOOL		bDone = FALSE;


	if( nMaxTxChannel >= MAX_TX_CHANNEL )
	{		
		
		if( ( eMode == ROUTER_ASSIGN_CHANNEL ) || (eMode == ROUTER_SEARCH_ASSIGN_CHANNEL))
		{
			LOG_DUMP( "CmdRouter: Routing for Cmd(%d) failed and nMaxTxChannel: %d", pPacket->Header.nCommand, nMaxTxChannel );
			return FALSE;
		}
		
	}

	

	
	LOG_DUMP( "---------------------------------------------------------");
	LOG_DUMP( "Router: Requested ARFCN: %d, Band: %d", pChannelKey->oFreq.nARFCN, pChannelKey->oFreq.nBand);
	LOG_DUMP( "Router: and Requested TN: %d, ChannelComb: %d", pChannelKey->nTN, pChannelKey->eChannelComb);



//	LOG_MSG_PM( "---------------------------------------------------------");
//	LOG_MSG_PM( "Router: Requested ARFCN: %d, Band: %d", pChannelKey->oFreq.nARFCN, pChannelKey->oFreq.nBand);
//	LOG_MSG_PM( "Router: and Requested TN: %d, ChannelComb: %d", pChannelKey->nTN, pChannelKey->eChannelComb);

	eCore = CORE_1; // We are making TX Only in Core_1.
	switch( eMode )
	{
		case ROUTER_ASSIGN_CHANNEL:
			for(nTxMgr = 0; (bDone== FALSE) && (nTxMgr < MAX_TX_MGR); nTxMgr++)
			{
				if( gTxRouter[CORE_1][nTxMgr].nTN == 0  )
				{
					gTxRouter[CORE_1][nTxMgr].nTN |= (1<<pChannelKey->nTN);
					gTxRouter[CORE_1][nTxMgr].oFreq	=	pChannelKey->oFreq;
					gTxRouter[CORE_1][nTxMgr].oBeaconFreq	=	pChannelKey->oBeaconFreq;
					pPacket->nTxMgr = nTxMgr;
					nMaxTxChannel++;
					
					gTxRouter[CORE_1][nTxMgr].bIsBeacon = pChannelKey->bIsBeacon;
				
					bDone = TRUE;
					LOG_DUMP( "CmdRouter: Assigning and routing Cmd(%d) to Core(%d)", pPacket->Header.nCommand, CORE_1);
					LOG_DUMP( "CmdRouter: to TxMgr(%d) to TN(%d)", pPacket->nTxMgr, pChannelKey->nTN  );

//					LOG_MSG_PM( "CmdRouter: Assigning and routing Cmd(%d) to Core(%d)", pPacket->Header.nCommand, CORE_1);
//					LOG_MSG_PM( "CmdRouter: to TxMgr(%d) to TN(%d)", pPacket->nTxMgr, pChannelKey->nTN  );

					break;
				}
								
			}

		break;

		case ROUTER_SEARCH_ASSIGN_CHANNEL:
		for(nTxMgr = 0; (bDone== FALSE) && (nTxMgr < MAX_TX_MGR); nTxMgr++)
			{
				if(( gTxRouter[CORE_1][nTxMgr].oFreq.nBand == pChannelKey->oFreq.nBand)&&
					( gTxRouter[CORE_1][nTxMgr].oFreq.nARFCN == pChannelKey->oFreq.nARFCN) )
				{
					if( !(gTxRouter[CORE_1][nTxMgr].nTN & (1<<pChannelKey->nTN)) )
					{
						gTxRouter[CORE_1][nTxMgr].nTN |= (1<<pChannelKey->nTN);
						pPacket->nTxMgr = nTxMgr;
						gTxRouter[CORE_1][nTxMgr].bIsBeacon = pChannelKey->bIsBeacon;
						bDone = TRUE;
						nMaxTxChannel++;
				//		LOG_DUMP( "CmdRouter: Searching, assigning and routing Cmd(%d) to Core(%d)", pPacket->Header.nCommand, eCore);
				//		LOG_DUMP( "CmdRouter: to TxMgr(%d) to TN(%d)", pPacket->nRxMgr, pChannelKey->nTN  );

//						LOG_MSG_PM( "CmdRouter: Searching, assigning and routing Cmd(%d) to Core(%d)", pPacket->Header.nCommand, eCore);
//						LOG_MSG_PM( "CmdRouter: to TxMgr(%d) to TN(%d)", pPacket->nRxMgr, pChannelKey->nTN  );

						break;
					}
				
				}
			}
				

		break;
		case ROUTER_SEARCH_CHANNEL:
			for(nTxMgr = 0; (bDone== FALSE) && (nTxMgr < MAX_TX_MGR); nTxMgr++)
			{
				if(( gTxRouter[CORE_1][nTxMgr].oFreq.nBand == pChannelKey->oFreq.nBand)&&
					( gTxRouter[CORE_1][nTxMgr].oFreq.nARFCN == pChannelKey->oFreq.nARFCN) )
				{
					if( gTxRouter[CORE_1][nTxMgr].nTN & (1<<pChannelKey->nTN) )
					{
						pPacket->nTxMgr = nTxMgr;
						bDone = TRUE;
				//		LOG_DUMP( "CmdRouter: Searching and routing Cmd(%d) to Core(%d)", pPacket->Header.nCommand, CORE_1);
				//		LOG_DUMP( "CmdRouter: to TxMgr(%d) to TN(%d)", pPacket->nTxMgr, pChannelKey->nTN  );

//						LOG_MSG_PM( "CmdRouter: Searching and routing Cmd(%d) to Core(%d)", pPacket->Header.nCommand, CORE_1);
//						LOG_MSG_PM( "CmdRouter: to TxMgr(%d) to TN(%d)", pPacket->nTxMgr, pChannelKey->nTN  );

						break;
					}
				
				}
				
			}
		break;

		case ROUTER_DELETE_CHANNEL:
			for(nTxMgr = 0; (bDone== FALSE) && (nTxMgr < MAX_TX_MGR); nTxMgr++)
			{
				if(( gTxRouter[CORE_1][nTxMgr].oFreq.nBand == pChannelKey->oFreq.nBand)&&
					( gTxRouter[CORE_1][nTxMgr].oFreq.nARFCN == pChannelKey->oFreq.nARFCN) )
				{
					if( gTxRouter[CORE_1][nTxMgr].nTN & (1<<pChannelKey->nTN) )
					{
						gTxRouter[CORE_1][nTxMgr].nTN &= ~(1<<pChannelKey->nTN);
						pPacket->nTxMgr = nTxMgr;
						bDone = TRUE;
						nMaxTxChannel--;
					//	LOG_DUMP( "CmdRouter: Deleting and routing Cmd(%d) to Core(%d)", pPacket->Header.nCommand, CORE_1);
					//	LOG_DUMP( "CmdRouter: to RxMgr(%d) to TN(%d)", pPacket->nRxMgr, pChannelKey->nTN  );

//						LOG_MSG_PM( "CmdRouter: Deleting and routing Cmd(%d) to Core(%d)", pPacket->Header.nCommand, CORE_1);
//						LOG_MSG_PM( "CmdRouter: to RxMgr(%d) to TN(%d)", pPacket->nRxMgr, pChannelKey->nTN  );

						break;
					}
		
				}
				
			}
		break;
	}
	
	if(bDone == TRUE )
	{
		*pCore = eCore;
	}
	else
	{
		if( pChannelKey->eChannelComb == 7 ) // if channel comb 7 is found in beacon channel assign separate channel
		{
			bDone = CmdRouter_GetCoreInfoTx(pChannelKey, ROUTER_ASSIGN_CHANNEL, pPacket, pCore);	
		}
	}
	if(bDone == FALSE )
	{
		LOG_DUMP( "CmdRouter: Routing for Cmd(%d) failed", pPacket->Header.nCommand);

//		LOG_MSG_PM( "CmdRouter: Routing for Cmd(%d) failed", pPacket->Header.nCommand);

	}

	if( nMaxTxChannel < 0 )
	{
		LOG_DUMP( "CmdRouter: Routing for Cmd(%d) failed and nMaxTxChannel: %d", pPacket->Header.nCommand, nMaxTxChannel );

//		LOG_MSG_PM( "CmdRouter: Routing for Cmd(%d) failed and nMaxTxChannel: %d", pPacket->Header.nCommand, nMaxTxChannel );


		return FALSE;
	}
	return bDone;
}

BOOL CmdRouter_MapTxCmdToCore(Packet	*pPacket, DSP_CORE *pCore )
{
	CmdPkt		oCmdPkt;

	ROUTER_MODE		eRouterMode;
	CommandType		nCmd;
	ChannelKey		oChannelKey;

	// parse command
	CmdPkt_Parse(&oCmdPkt, pPacket);

	nCmd = CmdPkt_GetCommand(&oCmdPkt);

	switch( nCmd )
	{
		
		case IPU_TO_DSP_CONFIG_AREA_JAMMING:
//			CmdRouter_OnAreaJamming(pPacket);
			nMaxTxChannel =  MAX_TX_CHANNEL;
			nAJTxChannel =  MAX_TX_CHANNEL;
			nCBTxChannel = 0;
			nVBTSTxChannel  = 0;
			*pCore = (DSP_CORE) BOTH_COREFLAG;
			return TRUE;
		case IPU_TO_DSP_STOP_AREA_JAMMING:
		//	CmdRouter_OnAreaJamming(pPacket);
			*pCore = (DSP_CORE) CORE_1;
			nMaxTxChannel = 0;
			nAJTxChannel = 0;
			return (BOOL) TRUE;
			
		case IPU_TO_DSP_CONFIGURE_TRANSMITTER:
		
			 
			{
				Packet *pPacket = oCmdPkt.pPacket;
				
				oChannelKey.nTN = ParamsConfig_GetTS(pPacket);
				
				oChannelKey.eChannelComb 	= (CHANNEL_COMB)ParamsConfig_GetChannelComb(pPacket);
				
				oChannelKey.oFreq.nBand 	= ParamsConfig_GetBAND(pPacket);
				oChannelKey.oFreq.nARFCN 	= ParamsConfig_GetARFCN(pPacket);
				oChannelKey.oBeaconFreq.nBand	=	ParamsConfig_GetBeaconBAND(pPacket);
				oChannelKey.oBeaconFreq.nARFCN	=	ParamsConfig_GetBeaconARFCN(pPacket);
				oChannelKey.bIsBeacon		= FALSE;
				
				ENABLE_FREQ_HOPPING(pPacket);
				eRouterMode = ROUTER_ASSIGN_CHANNEL;
				nCBTxChannel++;
				return CmdRouter_GetCoreInfoTx(&oChannelKey, eRouterMode, oCmdPkt.pPacket, pCore);
				
			}
			case IPU_TO_DSP_CONFIGURE_VBTS_MODE:
			 
			{
				Packet *pPacket = oCmdPkt.pPacket;
				
				oChannelKey.nTN = ParamsConfig_GetTS(pPacket);
				
				oChannelKey.eChannelComb 	= (CHANNEL_COMB)ParamsConfig_GetChannelComb(pPacket);
				
				oChannelKey.oFreq.nBand 	= ParamsConfig_GetBAND(pPacket);
				oChannelKey.oFreq.nARFCN 	= ParamsConfig_GetARFCN(pPacket);
				oChannelKey.oBeaconFreq.nBand	=	ParamsConfig_GetBeaconBAND(pPacket);
				oChannelKey.oBeaconFreq.nARFCN	=	ParamsConfig_GetBeaconARFCN(pPacket);			
				ENABLE_FREQ_HOPPING(pPacket);
				
				if( (oChannelKey.eChannelComb==4 ) || (oChannelKey.eChannelComb==5))
				{
					eRouterMode = ROUTER_ASSIGN_CHANNEL;
					oChannelKey.bIsBeacon		= TRUE;
					
				}
				else
				{
					eRouterMode = ROUTER_SEARCH_ASSIGN_CHANNEL;
					
				}
				nVBTSTxChannel++;
				return CmdRouter_GetCoreInfoTx(&oChannelKey, eRouterMode, oCmdPkt.pPacket, pCore);
				
			}
		case IPU_TO_DSP_SET_TSC_FOR_TRANSMITTER:
		case IPU_TO_DSP_SET_TSC_FOR_VBTS_MODE:
			{
				Packet *pPacket = oCmdPkt.pPacket;
				
				oChannelKey.nTN = ParamsTsc_GetTS(pPacket);
				
				oChannelKey.eChannelComb 	= (CHANNEL_COMB)ParamsTsc_GetChannelComb(pPacket);
				
				oChannelKey.oFreq.nBand 	= ParamsTsc_GetBAND(pPacket);
				oChannelKey.oFreq.nARFCN 	= ParamsTsc_GetARFCN(pPacket);
				
				eRouterMode = ROUTER_SEARCH_CHANNEL;	
			
				return CmdRouter_GetCoreInfoTx(&oChannelKey, eRouterMode, oCmdPkt.pPacket, pCore);
				
			}

		

		case IPU_TO_DSP_STOP_TRANSMITTER:
		case IPU_TO_DSP_STOP_VBTS_MODE:
			{
				Packet *pPacket = oCmdPkt.pPacket;
				
				oChannelKey.nTN = ParamsStop_GetTS(pPacket);
				
				oChannelKey.eChannelComb 	= (CHANNEL_COMB)ParamsStop_GetChannelComb(pPacket);
				
				oChannelKey.oFreq.nBand 	= ParamsStop_GetBAND(pPacket);
				oChannelKey.oFreq.nARFCN 	= ParamsStop_GetARFCN(pPacket);
				
				eRouterMode = ROUTER_DELETE_CHANNEL;
				
				if( nCmd == IPU_TO_DSP_STOP_TRANSMITTER)
				{
					nCBTxChannel--;	
				}
				else
				{
					nVBTSTxChannel--;
				}
			
				return CmdRouter_GetCoreInfoTx(&oChannelKey, eRouterMode, oCmdPkt.pPacket, pCore);
				
			}
		case DSP_TO_FPGA_AREA_JAMMING:
		case DSP_TO_FPGA_STOP_AREA_JAMMING:

			*pCore	= (DSP_CORE)CORE_0;
			return TRUE;
		case DSP_TO_FPGA_TX_TUNE:
			{
				*pCore	= (DSP_CORE)INVALID_CORE;
				return TRUE;
			}
		case DSP_TO_FPGA_DAC_CONFIG:
			{
				*pCore	= (DSP_CORE)INVALID_CORE;
				return TRUE;
			}
		default:
			return FALSE;

	}
		
}
BOOL CmdRouter_DeleteChannel(Packet *pPacket)
{
	DSP_CORE eCore;
	UINT8	nRxMgr;
	UINT8	nDLDDCNum;
	UINT8	nULDDCNum;


	eCore = (DSP_CORE) pPacket->nCoreno;
	nRxMgr = pPacket->nRxMgr;

	nDLDDCNum = ((eCore - 1) * 7) + nRxMgr + 1;
	nULDDCNum = nDLDDCNum;

	DDCRx_RemoveJobonStopCmd(gDDCRx,nDLDDCNum,eCore);
	

//	Signal_Pend(&(gDDCRx->SigStop_Received));  // Why it is required

	DDCRx_RemoveJobonStopCmd(gDDCRx,nULDDCNum,eCore);

/*	if(	Stopburst ==  TRUE)
	Signal_Pend(&(gDDCRx->SigStop_Received));	// Why it is required
*/
		gRxRouter[eCore][nRxMgr].nTN = 0;
		gRxRouter[eCore][nRxMgr].eChannelComb  = INVALID;
		gRxRouter[eCore][nRxMgr].nMAIO = 0 ;
		gRxRouter[eCore][nRxMgr].nSubchannel = 0;
		nMaxRxChannel--;
//		Eth_Debug((CHAR *)"nMaxRxchannel decreased to %d ",nMaxRxChannel);
		return TRUE;
}

BOOL CmdRouter_MapCmdToCore(Packet	*pPacket, DSP_CORE *pCore )
{
	
	CmdPkt		oCmdPkt;

	CommandType		nCmd;

	// parse command
	CmdPkt_Parse(&oCmdPkt, pPacket);

	nCmd = CmdPkt_GetCommand(&oCmdPkt);
	switch(nCmd)
	{
		case IPU_TO_DSP_SCAN_BEACON_FREQ	:
		case IPU_TO_DSP_CONFIGURE_RECEIVER	:
		case IPU_TO_DSP_SET_TSC_FOR_RECEIVER :
		case IPU_TO_DSP_CHANNEL_MODIFY:
		case IPU_TO_DSP_STOP_RECEIVER :
		case IPU_TO_DSP_STOP_SCANNING_ARFCN:
		case DSP_TO_FPGA_RX_TUNE:
		case IPU_TO_DSP_STOP_SCANING_BAND:
		case DSP_TO_FPGA_SET_DDC:  // it was DSP_TO_FPGA_SET_DDC1_CONFIGURATION
		case DSP_TO_FPGA_SET_DDC2_CONFIGURATION:


			return CmdRouter_MapRxCmdToCore(pPacket, pCore);
		case IPU_TO_DSP_CONFIGURE_TRANSMITTER :
		case IPU_TO_DSP_CONFIGURE_VBTS_MODE :
		case IPU_TO_DSP_CONFIG_AREA_JAMMING :
		case IPU_TO_DSP_SET_TSC_FOR_TRANSMITTER :
		case IPU_TO_DSP_SET_TSC_FOR_VBTS_MODE :
		case IPU_TO_DSP_STOP_TRANSMITTER:
		case IPU_TO_DSP_STOP_VBTS_MODE :
		case IPU_TO_DSP_STOP_AREA_JAMMING:
		case DSP_TO_FPGA_STOP_AREA_JAMMING:
		case DSP_TO_FPGA_AREA_JAMMING:
		case DSP_TO_FPGA_TX_TUNE:
		case DSP_TO_FPGA_DAC_CONFIG:

			return CmdRouter_MapTxCmdToCore(pPacket, pCore);

		default:
			
			Eth_Debug((CHAR *)"CmdRouter:UnExpected Command received");
			LOG_DUMP("FATAL: ###CmdRouter:UnExpected Command received###");
			LOG_DUMP("CmdRouter:UnExpected Command received %d",nCmd);
			return FALSE;
	}

}

#ifdef ENABLE_RESET_TRX

VOID ResetTRXChannels( UINT16 nBeaconARFCN )
{

// Reset all associated trx channels except beacon.. (beacon will be deleted
//as part of command router 

	UINT8 nRxMgr, nTxMgr;
	DSP_CORE	eCore;

	for(nRxMgr = 0; (nRxMgr < MAX_RX_MGR); nRxMgr++)
	{
		for(eCore = CORE_1; eCore <=  MAX_PROCESSING_CORES; eCore++)
		{
			if(gRxRouter[eCore][nRxMgr].oBeaconFreq.nARFCN == nBeaconARFCN )
			{
				UINT8 nTN;
				for(nTN = 0; nTN <8; nTN++)
				{
					
					if( ( gRxRouter[eCore][nRxMgr].nTN >> nTN ) & 0X01 )
					{
						nMaxRxChannel--;
					}
				}	
				
				
				gRxRouter[eCore][nRxMgr].nTN = 0;
				if( gRxRouter[eCore][nRxMgr].bIsBeacon == TRUE )
				{
					gRxRouter[eCore][nRxMgr].nTN	=	1;
					nMaxRxChannel++;
				}
				#ifdef HANDLE_HOPPING
				if( gRxRouter[eCore][nRxMgr].nID != 0 )
				{
					bFreeID[gRxRouter[eCore][nRxMgr].nID]= 1;
					gRxRouter[eCore][nRxMgr].nID = 0;
				}
				#endif		
							
			}
		}
		
	}


	for(nTxMgr = 0; (nTxMgr < MAX_TX_MGR); nTxMgr++)
	{
		
		if(gTxRouter[CORE_1][nTxMgr].oBeaconFreq.nARFCN == nBeaconARFCN )
		{
		
			UINT8 nTN;
			for(nTN = 0; nTN <8; nTN++)
			{
					
				if( ( gTxRouter[CORE_1][nTxMgr].nTN >> nTN ) & 0X01 )
				{
					nMaxTxChannel--;
					
					if( gTxRouter[CORE_1][nTxMgr].bIsBeacon)
					{
						
						// it is beacon channel means vbts
						nVBTSTxChannel--;
					}
					else
					{
						nCBTxChannel--;
					}
				}
			}	
			
			gTxRouter[CORE_1][nTxMgr].nTN = 0;

			if( gTxRouter[CORE_1][nTxMgr].nID != 0 )
			{
				bFreeID[gTxRouter[CORE_1][nTxMgr].nID]= 1;
				gTxRouter[CORE_1][nTxMgr].nID = 0;
			}
		}
	}
}
#endif

