#include "camera.h"
#include "legato.h"
#include "serial.h"


const unsigned char stopCaptureCMD[]        = { 0x56 ,0x00 ,0x36 ,0x01 ,0x03 };
const unsigned char stopCaptureACK[]        = { 0x76 ,0x00 ,0x36 ,0x00 ,0x00 };

const unsigned char captureImageCMD[]       = { 0x56 ,0x00 ,0x36 ,0x01 ,0x00 };
const unsigned char captureImageACK[]       = { 0x76 ,0x00 ,0x36 ,0x00 ,0x00 };

const unsigned char imageDataLengthCMD[]    = { 0x56 ,0x00 ,0x34 ,0x01 ,0x00 };
const unsigned char imageDataLengthACK[]    = { 0x76 ,0x00 ,0x34 ,0x00 ,0x04 ,0x00 ,0x00 };

const unsigned char getImageCMD[]           = { 0x56 ,0x00 ,0x32 ,0x0C ,0x00 ,0x0A ,0x00 ,0x00, 0x00, 0x00, 0x00, 0x00 };
const unsigned char getImageACK[]           = { 0x76 ,0x00 ,0x32 ,0x00 ,0x00 };

const unsigned char resetCMD[]              = { 0x56 ,0x00 ,0x26 ,0x00 };
const unsigned char resetACK[]              = {  };

const unsigned char disableCompressionCMD[] = { 0x56 ,0x00 ,0x31 ,0x05 ,0x01 ,0x01 ,0x12 ,0x04 ,0x00 };
const unsigned char disableCompressionACK[] = { 0x76 ,0x00 ,0x31 ,0x00 ,0x00 };


const camCMD_t camCmdList[E_SIZEOF_CAMCMDLISTNAME] = {
    {.cmdName="Stop Capture         "   ,   .cmdData=stopCaptureCMD,            .cmdDataSize=sizeof(stopCaptureCMD), 
                                            .cmdDataACK=stopCaptureACK,         .cmdDataACKSize=sizeof(stopCaptureACK)          },
    {.cmdName="Capture Image Capture"   ,   .cmdData=captureImageCMD,           .cmdDataSize=sizeof(captureImageCMD), 
                                            .cmdDataACK=captureImageACK,        .cmdDataACKSize=sizeof(captureImageACK)         },
    {.cmdName="Get Image Data Length"   ,   .cmdData=imageDataLengthCMD,        .cmdDataSize=sizeof(imageDataLengthCMD), 
                                            .cmdDataACK=imageDataLengthACK,     .cmdDataACKSize=sizeof(imageDataLengthACK)      },
    {.cmdName="Get Image            "   ,   .cmdData=getImageCMD,               .cmdDataSize=sizeof(getImageCMD), 
                                            .cmdDataACK=getImageACK,            .cmdDataACKSize=sizeof(getImageACK)             },
    {.cmdName="Reset                "   ,   .cmdData=resetCMD,                  .cmdDataSize=sizeof(resetCMD), 
                                            .cmdDataACK=resetACK,               .cmdDataACKSize=sizeof(resetACK)                },
    {.cmdName="Disable Compression  "   ,   .cmdData=disableCompressionCMD,     .cmdDataSize=sizeof(disableCompressionCMD), 
                                            .cmdDataACK=disableCompressionACK,  .cmdDataACKSize=sizeof(disableCompressionACK)   },
};

unsigned char gDataImageLengthHigh = 0;
unsigned char gDataImageLengthLow  = 0;
char gFileToSave[MAX_PATH_SIZE];


int camOpenSerial(Camera *aCamera) {
    int rc = 0;
    rc = serialOpen(aCamera->devPath, aCamera->speed);    
    return rc;
}

int camCloseSerial() {
    int rc = 0;
    rc = serialClose(); 
    return rc;
}

void camSetImageLengthForTransfert(void) {
    unsigned char imageLengthForTransfert[4];
    imageLengthForTransfert[0] = gDataImageLengthHigh;
    imageLengthForTransfert[1] = gDataImageLengthLow;
    imageLengthForTransfert[2] = 0x00;
    imageLengthForTransfert[3] = 0x00;
    
    for (int i = 0; i< sizeof(imageLengthForTransfert); i++) {
        serialSendByte(imageLengthForTransfert[i],1);
    }
}

int camReceiveAndSaveJpg(void) {
    int rc = 0;
  
    FILE* filePtr; 
    unsigned char byte;
    int bufferSize;
    int bufferSizeOrg;
    
    filePtr = fopen(gFileToSave, "w");
    
    bufferSize = gDataImageLengthHigh;
    bufferSize <<= 8;
    bufferSize |= gDataImageLengthLow;
    
    bufferSizeOrg = bufferSize;
    
    LE_DEBUG("Photo size : %d bytes",bufferSize+2);
    
    while (bufferSize > 0) {
        rc = serialReadByte(&byte, TTY_TIMEOUT);
        fwrite (&byte , sizeof(char), sizeof(char), filePtr);
        if (C_SERIAL_ERROR_TIMEOUT == rc) { // If we receivd 95% of the image we consider thats OK
            int receivedPercentage = ((bufferSize*100)/bufferSizeOrg);
            if (  receivedPercentage > C_MIN_PHOTO_RECEIVED_TO_BE_OK_THRESHOLD) {
                LE_WARN("Photo received at %02d %%, OK",receivedPercentage);
                rc = true;
                fclose(filePtr);
                return rc;
            }
            else {
                LE_ERROR("Photo received at %02d %%, KO",receivedPercentage);
                rc = false;
                fclose(filePtr);
                return rc;
            }
        }
        bufferSize--;
    }
    fclose(filePtr);
    rc = true;
    return rc;
}
 
 int camSendCommand(camCmdListName_t aCmd) {
    unsigned char byte  = 0;
    int rc  =  true ;
    // Set 
    for (int i = 0; i< camCmdList[aCmd].cmdDataSize; i++) {
        serialSendByte(camCmdList[aCmd].cmdData[i],1);
    }
    
    // for getting image, we need to send the length
    if (E_GET_IMAGE == aCmd) {
        camSetImageLengthForTransfert();
    }
    
    // Get Resp
    for (int i = 0; i< camCmdList[aCmd].cmdDataACKSize; i++) {
        serialReadByte(&byte, TTY_TIMEOUT);
        if(camCmdList[aCmd].cmdDataACK[i] == byte) {
            rc = true;
        }
        else {
            rc = false;
            return rc;
        }    
    }
    // when getting image length, we need to store it
    if (E_IMAGE_DATA_LENGTH == aCmd) {
        serialReadByte(&byte, TTY_TIMEOUT);
        gDataImageLengthHigh = byte;
        serialReadByte(&byte, TTY_TIMEOUT);
        gDataImageLengthLow  = byte;
        LE_DEBUG("gDataImageLengthHigh    : 0x%02X",gDataImageLengthHigh);
        LE_DEBUG("gDataImageLengthLow     : 0x%02X",gDataImageLengthLow);
    }
    // when getting image, we need to store it
    else if (E_GET_IMAGE == aCmd) {
        rc = camReceiveAndSaveJpg();
    }
    return rc;
 }
 
const char * camGetCommandName(camCmdListName_t aCmd) {
     return camCmdList[aCmd].cmdName;
 }


void camSetFileToSave(char * aFileToSave) {
    snprintf(gFileToSave,sizeof(gFileToSave),"%s",aFileToSave);
}