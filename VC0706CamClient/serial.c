
#include "serial.h"
#include "legato.h"

static int gTtyFD = 0;

int serialOpen(char *aSerialPortName, speed_t aSpeed)
{
    struct termios tio;

    FILE *fp;
    fp = NULL;

    fp = popen("ash && modprobe ftdi_sio","r");
    pclose(fp);
    
    memset(&tio, 0, sizeof(tio));
    tio.c_iflag = 0;
    tio.c_oflag = 0;
    tio.c_cflag = CS8 | CREAD | CLOCAL;         // 8n1, see termios.h for more information
    tio.c_lflag = 0;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 5;

    gTtyFD = open(aSerialPortName, O_RDWR | O_NONBLOCK);
    cfsetospeed(&tio, aSpeed);
    cfsetispeed(&tio, aSpeed);
    tcflush( gTtyFD, TCIFLUSH );
    tcsetattr(gTtyFD, TCSANOW, &tio);
    
    LE_DEBUG("SerialPortName        : %s", aSerialPortName);
    LE_DEBUG("Speed                 : %d", (int)aSpeed);
    LE_DEBUG("File Descriptor       : %d", (int)gTtyFD);

    return gTtyFD;
}

int serialClose(void)
{
    LE_DEBUG("File Descriptor       : %d",(int)gTtyFD);
    close(gTtyFD);
    return 0;
}

void serialSendByte(unsigned char aByte, size_t aCount)
{
    LE_DEBUG("aByte     : 0x%02X",(int)aByte);
    LE_DEBUG("aCounT    : %d",(int)aCount);
    write(gTtyFD, &aByte, aCount);
}

int serialReadByte(unsigned char *aByte, int aMsTimeout)
{
    fd_set set;
    struct timeval timeout;
    int rc;

    
    FD_ZERO(&set);          /* clear the set */
    FD_SET(gTtyFD, &set);   /* add our file descriptor to the set */

    timeout.tv_sec = 0;
    timeout.tv_usec = aMsTimeout * 1000;

    rc = select(gTtyFD+1, &set, NULL, NULL, &timeout);
    if (rc == -1) {
        LE_DEBUG("select error"); /* an error accured */
    }
    else if (rc == C_SERIAL_ERROR_TIMEOUT) {
        LE_DEBUG("reading timeout error"); /* a timeout occured */
    }
    else {
        read( gTtyFD, aByte, sizeof(char));  /* there was data to read */
    }
    LE_DEBUG("aByte      : 0x%02X",*aByte);
    return rc;
}
