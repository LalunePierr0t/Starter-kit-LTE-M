#include "interfaces.h"
#include "legato.h"
#include "camera.h"

#define RETRY_WAIT_SEC 60

int takePhoto(Camera* cam, char* dirPath) {
    int rc = false;
    char fileToSave[MAX_PATH_SIZE];
    snprintf(fileToSave,sizeof(fileToSave),"%s%d.jpg",dirPath,(int)time(0));
    camSetFileToSave(fileToSave);
    
    rc = camOpenSerial(cam);
    LE_INFO("Open CAM       : %s", (rc > 0) ? "OK":"KO" );
    rc = camSendCommand(E_DISABLE_COMPRESSION);
    LE_INFO("Command : %s  : %s", camGetCommandName(E_DISABLE_COMPRESSION), (true == rc) ? "OK":"KO" );
    rc = camSendCommand(E_STOP_CAPTURE);
    LE_INFO("Command : %s  : %s", camGetCommandName(E_STOP_CAPTURE), (true == rc) ? "OK":"KO" );
    rc = camSendCommand(E_CAPTURE_IMAGE);
    LE_INFO("Command : %s  : %s", camGetCommandName(E_CAPTURE_IMAGE), (true == rc) ? "OK":"KO" );
    rc = camSendCommand(E_IMAGE_DATA_LENGTH);
    LE_INFO("Command : %s  : %s", camGetCommandName(E_IMAGE_DATA_LENGTH), (true == rc) ? "OK":"KO" );
    rc = camSendCommand(E_GET_IMAGE);
    LE_INFO("Command : %s  : %s", camGetCommandName(E_GET_IMAGE), (true == rc) ? "OK":"KO" );
    rc = camSendCommand(E_RESET);
    LE_INFO("Command : %s  : %s", camGetCommandName(E_RESET), (true == rc) ? "OK":"KO" );
    camCloseSerial();
    return rc;
}

void photoLoop(Camera* cam, int intervalMintues, char* dirPath) {
  LE_INFO("Taking photos every %d minutes and storing them in %s",
          intervalMintues, dirPath);
  while (true) {
    int success = takePhoto(cam, dirPath);
    int sleepDur = success ? intervalMintues * 60 : RETRY_WAIT_SEC;
    if (success)
      LE_INFO("Taking next photo in %d minutes", intervalMintues);
    else
      LE_INFO("Retrying after %d seconds", RETRY_WAIT_SEC);
    sleep(sleepDur);
  }
}



COMPONENT_INIT {
//    Camera cam = {
//        .devPath="/dev/ttyUSB0", .serialNum = 0x00, .speed = B115200
//    };
//    
//    photoLoop(&cam, 10, "/tmp/");
}