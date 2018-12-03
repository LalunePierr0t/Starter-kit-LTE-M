#include "legato.h"
#include "interfaces.h"

 #ifndef IO_RASP_SAMPLE_INCLUDE_GUARD
 #define IO_RASP_SAMPLE_INCLUDE_GUARD
typedef void (*ioRaspExternHandler)(void);

LE_SHARED void SetIoRaspExternHandler(ioRaspExternHandler aIoRaspHandler);


#endif //IO_RASP_SAMPLE_INCLUDE_GUARD