/** 
 *   @file  bind6if.h
 *
 *   @brief   
 *      Common structures and definitions used by the BIND6 Object.
 *
 *  \par
 *  NOTE:
 *      (C) Copyright 2008, Texas Instruments, Inc.
 *
 *  \par
 */

#ifndef _C_BINDV6IF_INC
#define _C_BINDV6IF_INC  /* #defined if this .h file has been included */

/* BIND6 Flag Definitions: These definitions define the status of the address
 * in the System. 
 *  - TENTATIVE : Address is not assigned and the DAD process is executing on it
 *  - DEPRECATED: Address Lifetime has expired and should not be used. 
 *  - ANYCAST   : Address is an ANYCAST address and DAD should -not- be run on it. */
#define BIND6_TENTATIVE_ADDRESS      0x1
#define BIND6_DEPRECATED_ADDRESS     0x2
#define BIND6_ANYCAST_ADDRESS        0x4

/** 
 * @brief 
 *  The structure describes the BIND6 Object.
 *
 * @details
 *  There exists a BIND6 Object for each IPv6 Address which has been 
 *  assigned to a network interface. If there are multiple IPv6 addresses 
 *  assigned to the same network interface then each address is associated 
 *  with a BIND6 object.  
 */
typedef struct BIND6_ENTRY
{
    /**
     * @brief   Links to other BIND6 Objects.
     */
    LIST_NODE     links;

    /**
     * @brief   Flags associated with the BIND6 Object.
     *  - TENTATIVE : Address is not assigned and the DAD process is executing on it
     *  - DEPRECATED: Address Lifetime has expired and should not be used. 
     *  - ANYCAST   : Address is an ANYCAST address and DAD should -not- be run on it. 
     */
    UINT32        flags;

    /**
     * @brief   Network Interface Device on which the BIND6 object resides.
     */
    NETIF_DEVICE* ptr_device;

    /**
     * @brief   Handle of the networking (CLONING) route which is associated 
     *          with the BIND6 Object.
     */
    HANDLE        hRtNet;

    /**
     * @brief   Handle of the HOST route which is associated with the BIND6 Object.
     */
    HANDLE        hRtHost;

    /**
     * @brief   The IPv6 Address associated with the BIND6 Object.
     */
    IP6N          IPHost;

    /**
     * @brief   The IPv6 Network address associated with the BIND6 Object. 
     */
    IP6N          IPNet;

    /**
     * @brief   The IPv6 Network Mask associated with the BIND6 Object. 
     */
    IP6N          IPMask;

    /**
     * @brief   The Valid Lifetime of the BIND6 Object for which the address
     *          and associated bindings will remain valid. All Link Local Bindings
     *          remain vaid forever. Valid Lifetime is stored as an "absolute" value
     */
    int          ValidLifetime;

    /**
     * @brief   The Preferred Lifetime of the BIND6 Object for which the address
     *          and associated bindings will remain valid. All Link Local Bindings
     *          remain vaid forever. Preferred Lifetime is stored as an "absolute"
     *          value
     */
    int          PreferredLifetime;

    /**
     * @brief   The Number of probes sent out on this address for the DAD state machine.
     */
    UINT16        NumProbes;
}BIND6_ENTRY;

/********************************************************************** 
 * Exported API (KERNEL MODE):
 *  These functions are exported by the BIND6 Module and are available 
 *  for internal NDK core stack usage only.
 ***********************************************************************/
_extern HANDLE Bind6New (NETIF_DEVICE* ptr_device, IP6N Host, IP6N Mask, UINT32 ValidLT, UINT32 PrefLT, UINT8 IsAny);
_extern void   Bind6Free (HANDLE hbind6Obj);
_extern int    Bind6DADCheck (NETIF_DEVICE* ptr_device, IP6N TargetAddress);

_extern HANDLE Bind6FindByHost (NETIF_DEVICE* ptr_device, IP6N IP);
_extern HANDLE Bind6FindByNet (NETIF_DEVICE* ptr_device, IP6N IPNet);
_extern HANDLE Bind6FindByIF (NETIF_DEVICE* ptr_device);
_extern IP6N   Bind6IF2IPHost (NETIF_DEVICE* ptr_device);

_extern UINT32 Bind6GetLifetime (HANDLE hbind6Obj);
_extern void   Bind6SetLifetime (HANDLE hbind6Obj, UINT32 Lifetime);

_extern int    Bind6GetLinkLocalAddress(NETIF_DEVICE* ptr_device, IP6N* IPAddress);
_extern int    Bind6GetGlobalAddress(NETIF_DEVICE* ptr_device, IP6N IPNetworkAddr, IP6N* IPAddress);

_extern NETIF_DEVICE* Bind6GetInterfaceHandle (HANDLE hbind6Obj);

/********************************************************************** 
 * Exported API (KERNEL MODE SAFE):
 *  These functions are exported by the BIND6 Module and are available 
 *  for application developers to be used.
 ***********************************************************************/
_extern BIND6_ENTRY*  Bind6GetTable (void);
_extern void          Bind6CleanTable (BIND6_ENTRY* ptr_list);

#endif /* _C_BINDV6IF_INC */
