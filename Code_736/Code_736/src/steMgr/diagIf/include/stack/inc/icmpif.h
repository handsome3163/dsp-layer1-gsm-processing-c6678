/*
 *  Copyright 2007 by Texas Instruments Incorporated.
 *  All rights reserved. Property of Texas Instruments Incorporated.
 *  Restricted rights to use, duplicate or disclose this code are
 *  granted through contract.
 *
 *  @(#) TCP/IP_Network_Developers_Kit 1.93.00.09 08-16-2007 (ndk-c09)
 */
//--------------------------------------------------------------------------
// Ip Stack
//--------------------------------------------------------------------------
// ICMPIF.H
//
// Author: Michael A. Denio
// Copyright 1999 by Texas Instruments Inc.
//-------------------------------------------------------------------------

#ifndef _C_ICMPIF_INC
#define _C_ICMPIF_INC  /* #defined if this .h file has been included */

//-----------------------------------------------------------------------
//
// Standard ICMP Equates
//

#define ICMP_ECHOREPLY                  0       // echo reply
#define ICMP_UNREACH                    3       // dest unreachable, codes:
#define     ICMP_UNREACH_NET            0       // bad net
#define     ICMP_UNREACH_HOST           1       // bad host
#define     ICMP_UNREACH_PROTOCOL       2       // bad protocol
#define     ICMP_UNREACH_PORT           3       // bad port
#define     ICMP_UNREACH_NEEDFRAG       4       // IP_DF caused drop
#define     ICMP_UNREACH_SRCFAIL        5       // src route failed
#define     ICMP_UNREACH_NET_UNKNOWN    6       // unknown net
#define     ICMP_UNREACH_HOST_UNKNOWN   7       // unknown host
#define     ICMP_UNREACH_ISOLATED       8       // src host isolated
#define     ICMP_UNREACH_NET_PROHIB     9       // prohibited access
#define     ICMP_UNREACH_HOST_PROHIB    10      // ditto
#define     ICMP_UNREACH_TOSNET         11      // bad tos for net
#define     ICMP_UNREACH_TOSHOST        12      // bad tos for host
#define     ICMP_UNREACH_FILTER_PROHIB  13      // admin prohib
#define     ICMP_UNREACH_HOST_PRECEDENCE 14     // host prec vio.
#define     ICMP_UNREACH_PRECEDENCE_CUTOFF 15   // prec cutoff
#define ICMP_SOURCEQUENCH               4       // packet lost, slow down
#define ICMP_REDIRECT                   5       // shorter route, codes:
#define     ICMP_REDIRECT_NET           0       // for network
#define     ICMP_REDIRECT_HOST          1       // for host
#define     ICMP_REDIRECT_TOSNET        2       // for tos and net
#define     ICMP_REDIRECT_TOSHOST       3       // for tos and host
#define ICMP_ECHO                       8       // echo service
#define ICMP_ROUTERADVERT               9       // router advertisement
#define ICMP_ROUTERSOLICIT              10      // router solicitation
#define ICMP_TIMXCEED                   11      // time exceeded, code:
#define     ICMP_TIMXCEED_INTRANS       0       // ttl==0 in transit
#define     ICMP_TIMXCEED_REASS         1       // ttl==0 in reass
#define ICMP_PARAMPROB                  12      // ip header bad
#define     ICMP_PARAMPROB_OPTABSENT    1       // req. opt. absent
#define ICMP_TSTAMP                     13      // timestamp request
#define ICMP_TSTAMPREPLY                14      // timestamp reply
#define ICMP_IREQ                       15      // information request
#define ICMP_IREQREPLY                  16      // information reply
#define ICMP_MASKREQ                    17      // address mask request
#define ICMP_MASKREPLY                  18      // address mask reply

#define ICMP_MAXTYPE                    18

//-------------------------------------------------------------------------
//
// Statisics Data
//

extern UINT32   _ICMPInErrors;
extern UINT32   _ICMPOutErrors;
extern UINT32   _ICMPIn[ ICMP_MAXTYPE+1 ];
extern UINT32   _ICMPOut[ ICMP_MAXTYPE+1 ];

//-----------------------------------------------------------------------
//
// ICMP Functions
//

_extern void   ICMPInput( PBM_Pkt *pPkt );

_extern void   ICMPChecksum( ICMPHDR *pIcHdr, uint Size );

_extern void   ICMPGenPacket( IPHDR *pIpHdr, HANDLE hIFRx,
                              uint Type, uint Code, UINT32 Aux );

_extern void   ICMPSendRtAdv( HANDLE hIF, uint wLife,
                              IPN IPAddr, long Pref );

#endif

