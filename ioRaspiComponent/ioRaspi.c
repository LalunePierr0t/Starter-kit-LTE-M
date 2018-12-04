#include "legato.h"
#include "interfaces.h"
#include "ioRaspi.h"

ioRaspExternHandler gIoRaspHandler;

void SetIoRaspExternHandler(ioRaspExternHandler aIoRaspHandler) {
    gIoRaspHandler = aIoRaspHandler;
}

//-------------------------------------------------------------------------------------------------
/**
 * Generic Push button handler -> toggle LED1
 */
//-------------------------------------------------------------------------------------------------
static void GenericPushButtonHandler
(
    bool state, ///< true if the button is pressed
    void *ctx   ///< context pointer - not used
)
{
    LE_DEBUG("Generic Button State change %s", state?"TRUE":"FALSE");
    if (true == state) {
        gIoRaspHandler();
    }

}


//-------------------------------------------------------------------------------------------------
/**
 * Configure GPIOs
 */
//-------------------------------------------------------------------------------------------------
static void ConfigureGpios(void)
{

    // Set the Generic push-button GPIO as input
    LE_FATAL_IF(
        mangoh_button_SetInput(MANGOH_BUTTON_ACTIVE_LOW) != LE_OK,
        "Couldn't configure push button as input");
    mangoh_button_AddChangeEventHandler(MANGOH_BUTTON_EDGE_RISING, GenericPushButtonHandler, NULL, 0);


}


COMPONENT_INIT
{
    ConfigureGpios();

}
