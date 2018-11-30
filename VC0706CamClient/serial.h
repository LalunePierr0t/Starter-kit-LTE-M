#ifndef SERIAL_H
#define SERIAL_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>

#define C_SERIAL_ERROR_TIMEOUT      0


/**
 * @brief       open a serial port
 *
 * @param[in]   aSerialPortName :   Serial Port Name
 */
int serialOpen(char *aSerialPortName, speed_t aSpeed);

/**
 * @brief       close opened serial port
 *
 * @param[in]   aSerialPortName :   Serial Port Name
 *
 */
int serialClose(void);

/**
 * @brief       Send Byte on opened serial pot
 *
 * @param[in]   aByte :         Byte to send
 *
 */
void serialSendByte(unsigned char aByte, size_t aCount);

/**
 * @brief       Receive Byte on opened serial pot
 *
 * @param[out]  aByte :         Byte to receive
 * @param[in]   aMsTimeout:     reading timeout
 *
 */
int serialReadByte(unsigned char *aByte, int aMsTimeout);


#endif //SERIAL_H

