 /**
  * Main functions to use the SMS sample codes.
  *
  * Copyright (C) Sierra Wireless Inc.
  *
  *
  *
  */

#include "legato.h"
#include "interfaces.h"
#include "smsSample.h"

//--------------------------------------------------------------------------------------------------
/**
 *  App init
 *
 */
//--------------------------------------------------------------------------------------------------
    const char*   destinationPtr = "+33625921232"; ///< [IN] The destination number.
//    const char*   destinationPtr = "+33677291019"; ///< Gregory
//    const char*   destinationPtr = "+33761428366"; ///Cedric Grimault


void sendSystemCommand(const char *aCmd,char *aCmdOutput,int aCmdOutputSize) {
    
    FILE *fp;
    fp = NULL;

    memset(aCmdOutput,0x00,aCmdOutputSize);
    LE_INFO("Start cmd       : %s\n",aCmd);
    
    fp = popen(aCmd,"r");
    
    if (NULL != fp) {
        LE_INFO("Success cmd     : %s\n",aCmd);
        while (fgets(aCmdOutput, aCmdOutputSize-1, fp) != NULL) {
            LE_INFO("Cmd Output      : %s", aCmdOutput);
        }
    }
        else {
        LE_INFO("Failed cmd : %s",aCmd);
    }
    pclose(fp);
}
    
    
    COMPONENT_INIT
{
    LE_INFO("SMS Starting");

    smsmt_Receiver();
//    smsmo_SendMessage(destinationPtr,consoleOutput);
   
    smsmt_MonitorStorage();
}
