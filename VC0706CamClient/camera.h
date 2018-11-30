#ifndef CAMERA_H
#define CAMERA_H

#include "legato.h"
#include <termios.h>

#define TTY_TIMEOUT 10
#define MAX_PATH_SIZE 256
#define C_MIN_PHOTO_RECEIVED_TO_BE_OK_THRESHOLD 95

typedef struct {
  char devPath[MAX_PATH_SIZE];
  uint8_t serialNum;            // camera serial number
  speed_t speed;

} Camera;

typedef struct {
    char cmdName[50];
    const unsigned char *cmdData;
    const unsigned char cmdDataSize;
    const unsigned char *cmdDataACK;
    const unsigned char cmdDataACKSize;
} camCMD_t;




typedef enum {
    E_STOP_CAPTURE,
    E_CAPTURE_IMAGE,
    E_IMAGE_DATA_LENGTH,
    E_GET_IMAGE,
    E_RESET,
    E_DISABLE_COMPRESSION,
    E_SIZEOF_CAMCMDLISTNAME
}   camCmdListName_t;

//camCmdListName_t e_cmdList;




// File stream functions for reading photos
LE_SHARED int camOpenSerial(Camera *aCamera) ;

LE_SHARED int camCloseSerial() ;

LE_SHARED int camStopCapture(void);

LE_SHARED int camSendCommand(camCmdListName_t aCmd);

LE_SHARED const char * camGetCommandName(camCmdListName_t aCmd);

LE_SHARED void camSetFileToSave(char * aFileToSave);


#endif
