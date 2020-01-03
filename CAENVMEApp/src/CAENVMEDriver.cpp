#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <exception>
#include <iostream>
#include <map>
#include <list>
#include <vector>
#include <iomanip>
#include <fstream>
#include <sys/timeb.h>

#include <epicsTypes.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsTimer.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <iocsh.h>

#include <macLib.h>
#include <epicsGuard.h>

#include "envDefs.h"
#include "errlog.h"

#include "CAENVMEWrapper.h"

#include "CAENVMEDriver.h"

#include <epicsExport.h>

static const char *driverName="CAENVMEDriver";

CAENVMEDriver::CAENVMEDriver(const char *portName, int crate, int board_id, unsigned base_address, unsigned card_increment, bool simulate)
   : asynPortDriver(portName, 
                    0, /* maxAddr */ 
                    NUM_CAENVME_PARAMS,
                    asynInt32Mask | asynUInt32DigitalMask | asynDrvUserMask, /* Interface mask */
                    asynInt32Mask | asynUInt32DigitalMask,  /* Interrupt mask */
                    ASYN_CANBLOCK, /* asynFlags.  This driver can block but it is not multi-device */
                    1, /* Autoconnect */
                    0, /* Default priority */
                    0),	/* Default stack size*/
					m_baseAddress(base_address), m_cardIncrement(card_increment)
{
	if (card_increment == 0)
	{
		card_increment = 0x10000; // base_address is OK defaulting to 0x0
	}
    createParam(P_crateString, asynParamInt32, &P_crate);
    createParam(P_boardIdString, asynParamInt32, &P_boardId);
	createParam(P_VMEWriteString, asynParamUInt32Digital, &P_VMEWrite);
	setIntegerParam(P_crate, crate);
	setIntegerParam(P_boardId, board_id);
	m_vme = new CAENVMEWrapper(simulate, cvV1718, 0, board_id);
}

struct VMEDetails
{
	unsigned addr;
	VMEDetails(const char* addr_str)
	{
		addr = strtoul(addr_str, NULL, 0);
	}
};
	
asynStatus CAENVMEDriver::writeUInt32Digital(asynUser *pasynUser, epicsUInt32 value, epicsUInt32 mask)
{
	int card;
    int function = pasynUser->reason;
	if (function == P_VMEWrite)
	{
	    getAddress(pasynUser, &card);
		VMEDetails* details = (VMEDetails*)pasynUser->userData;
		m_vme->writeCycle(m_baseAddress + m_cardIncrement * card + details->addr, &value, cvA32_U_DATA, cvD16);
		return asynSuccess;
	}
	else
	{
		return asynPortDriver::writeUInt32Digital(pasynUser, value, mask);
	}
}

asynStatus CAENVMEDriver::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
	return asynPortDriver::writeInt32(pasynUser, value);
}

asynStatus CAENVMEDriver::drvUserCreate(asynUser *pasynUser, const char* drvInfo, const char** pptypeName, size_t* psize)
{
   char *drvInfocpy;				//copy of drvInfo
   char *charstr;				//The current token
   char *tokSave = NULL;			//Remaining tokens

   if (strncmp(drvInfo, "VME", 3) == 0)
     {
     //take a copy of drvInfo and split into tokens
     drvInfocpy = epicsStrDup((const char *)drvInfo);
	 // first token is command
     charstr = epicsStrtok_r((char *)drvInfocpy, "_", &tokSave);
     if (!strcmp(charstr, P_VMEWriteString))
        pasynUser->reason = P_VMEWrite;
     //Second token is address
     charstr = epicsStrtok_r(NULL, "_", &tokSave);
     if (charstr != NULL)
        pasynUser->userData = new VMEDetails(charstr);
     free(drvInfocpy);
     return asynSuccess;
     }
  else
     {
     return asynPortDriver::drvUserCreate(pasynUser, drvInfo, pptypeName, psize);
     }
}

asynStatus CAENVMEDriver::drvUserDestroy(asynUser *pasynUser)
{
   const char *functionName = "drvUserDestroy";
   if ( pasynUser->reason == P_VMEWrite )
      {
      delete pasynUser->userData;
      pasynUser->userData = NULL;
      return(asynSuccess);
      }
  else
      {
      return asynPortDriver::drvUserDestroy(pasynUser);
      }
}
	
extern "C" {

/// \param[in] portName @copydoc initArg0
/// \param[in] fileName @copydoc initArg1
/// \param[in] fileType @copydoc initArg2
int CAENVMEConfigure(const char *portName, int crate, int board_id, unsigned base_address, unsigned card_increment, bool simulate)
{
	try
	{
		CAENVMEDriver* driver = new CAENVMEDriver(portName, crate, board_id, base_address, card_increment, simulate);
		if (driver == NULL)
		{
			errlogSevPrintf(errlogMajor, "CAENVMEConfigure failed (NULL)\n");
			return(asynError);
		}
		else
		{
			return(asynSuccess);
		}
	}
	catch(const std::exception& ex)
	{
		errlogSevPrintf(errlogMajor, "CAENVMEConfigure failed: %s\n", ex.what());
		return(asynError);
	}
}

// EPICS iocsh shell commands 

static const iocshArg initArg0 = { "portName", iocshArgString};			///< The name of the asyn driver port we will create
static const iocshArg initArg1 = { "crate", iocshArgInt};			
static const iocshArg initArg2 = { "board_id", iocshArgInt};			
static const iocshArg initArg3 = { "base_address", iocshArgInt};			
static const iocshArg initArg4 = { "card_increment", iocshArgInt};		
static const iocshArg initArg5 = { "simulate", iocshArgInt};		

static const iocshArg * const initArgs[] = { &initArg0,
                                             &initArg1,
											 &initArg2,
											 &initArg3,
											 &initArg4,
											 &initArg5
};

static const iocshFuncDef initFuncDef = {"CAENVMEConfigure", sizeof(initArgs) / sizeof(iocshArg*), initArgs};

static void initCallFunc(const iocshArgBuf *args)
{
    CAENVMEConfigure(args[0].sval, args[1].ival, args[2].ival, args[3].ival, args[4].ival, (args[5].ival != 0 ? true : false));
}

static void CAENVMERegister(void)
{
    iocshRegister(&initFuncDef, initCallFunc);
}

epicsExportRegistrar(CAENVMERegister);

}
