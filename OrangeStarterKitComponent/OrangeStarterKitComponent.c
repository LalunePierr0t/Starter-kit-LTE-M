//--------------------------------------------------------------------------------------------------
/**
 * @file OrangeStarterKitComponent.c
 *
 * This sample for the Orange Starter kit LTE-M makes use of mqttClient API over IPC, to start/stop mqttClient and to send mqtt messages to Orange LiveObjects
 * More info : http://developer.orange.com/starterkit
 *
 *
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"

#include "../OrangeStarterKitComponent/inc-gen/interfaces.h"
#include "../OrangeStarterKitComponent/swir_json.h"
#include "LiveObjects.h"
#include "dataProfileComponent.h"
#include "sensorUtils.h"
#include "legato.h"
#include "interfaces.h"
#include "smsSample.h"
#include "camera.h"
#include "ioRaspi.h"

//--------------------------------------------------------------------------------------------------
/**
 *  App init
 *
 */
//--------------------------------------------------------------------------------------------------



#define DATA_TIMER_IN_MS (60000)

static le_timer_Ref_t dataPubTimerRef;

int latitude = 0;
int longitude = 0;

static bool LedOn;

//-----
/**
 * Live Objects Settings
 */
char* NAMESPACE = "starterkit"; //device identifier namespace (device model, identifier class...)
char imei[20]; //device identifier (IMEI, Serial Number, MAC adress...)

char* timerStreamID = "starterkit!timer"; //identifier of the timeseries the published data belongs to
char* ledID = "starterkit!led";
char* cmdResultStreamID = "starterkit!cmdResult";

//------
/**
 * Orange network settings
 */
char                                        _profileAPN[] = "orange.ltem.spec";
char                                        _profileUser[] = "orange";
char                                        _profilePwd[] = "orange";
le_mdc_Auth_t                               _profileAuth = LE_MDC_AUTH_PAP;
int											_dataProfileIndex = 1;


int count = 0;

static const char PressureFile[] = "/sys/devices/i2c-0/0-0076/iio:device1/in_pressure_input";
static const char TemperatureFile[] = "/sys/devices/i2c-0/0-0076/iio:device1/in_temp_input";

void connectionHandler();

void sendSystemCommand(const char *aCmd,char *aCmdOutput,int aCmdOutputSize) {
    
    FILE *fp;
    fp = NULL;

    memset(aCmdOutput,0x00,aCmdOutputSize);
    LE_INFO("Start cmd       : %s\n",aCmd);
    
    fp = popen(aCmd,"r");
    
    if (NULL != fp) {
        LE_INFO("Success cmd     : %s\n",aCmd);
        if (NULL != aCmdOutput ) {
            while (fgets(aCmdOutput, aCmdOutputSize-1, fp) != NULL) {
                LE_INFO("Cmd Output      : %s", aCmdOutput);
            }
        }
    }
        else {
        LE_INFO("Failed cmd : %s",aCmd);
    }
    
    pclose(fp);
}

/**
 * Reports the pressure kPa.
 */
le_result_t mangOH_ReadPressureSensor
(
    double *reading
)
{
    return ReadDoubleFromFile(PressureFile, reading);
}

/**
 * Reports the temperature in degrees celcius.
 */
le_result_t mangOH_ReadTemperatureSensor
(
    double *reading
)
{
    int temp;
    le_result_t r = ReadIntFromFile(TemperatureFile, &temp);
    if (r != LE_OK)
    {
        return r;
    }

    // The divider is 1000 based on the comments in the kernel driver on bmp280_compensate_temp()
    // which is called by bmp280_read_temp()
    *reading = ((double)temp) / 1000.0;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Push Led current status to LiveObjects
 */
//--------------------------------------------------------------------------------------------------
static void LedPushStatus
(
    void
)
{
    char* model = "on";
    char* tags = "[\"led\", \"on\"]";

	char payload[100] = "";
    sprintf(payload, "{\"led\":%d}", LedOn);

    liveobjects_pubData(ledID, payload, model, tags, latitude, longitude);

}

void toLowerCase(char* aString, int aStringSize) {
    for(int i=0; i<aStringSize; i++){
            aString[i] = tolower(aString[i]);
    }
}

void RemoveCharInString(char* aString, int aStringSize,char aCharToremove)
{
    int j = 0;
    for(int i=0; i<aStringSize; i++){               // Parse all string until size of string
        if(aCharToremove == aString[i]) {           // Char to remove detected
            for(j = i; j < aStringSize;j++) {       // Find the next character different from character to remove
                if(aCharToremove != aString[j]) {   // Next character different from character to remove found
                    aString[i] = aString[j];        // Replace the current character to remove, by the next character different from character to remove found
                    aString[j] = aCharToremove;     // Replace the next character different from character to remove found by the character to remove
                    break;
                }
            }
        }
        else {
            aString[i] = aString[j];
        }
        j++;
    }
}

int takePhoto(char* aFileName) {
    int rc = false;
    Camera cam = {
        .devPath="/dev/ttyUSB0", .serialNum = 0x00, .speed = B115200
    };
    camSetFileToSave(aFileName);
    
    rc = camOpenSerial(&cam);
    LE_INFO("Open CAM: %s", (rc > 0) ? "OK":"KO" );
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
    camSendCommand(E_RESET);
    camCloseSerial();
    return rc;
}

typedef enum {
    E_REMOVE_TTY_USB,
    E_ADD_TTY_USB,
    E_NB_OF_COMMANDS
} addOrRemoveTTY;

void removAddTtyUSB(addOrRemoveTTY aCmd) {
    int fd;
    size_t image_size;
    struct stat st;
    void *image;

    switch(aCmd) {
        case E_REMOVE_TTY_USB:
            syscall(__NR_delete_module, "ftdi_sio",  O_NONBLOCK);                                   // rmmod
            break;
        case E_ADD_TTY_USB:
                fd = open("/lib/modules/3.18.44/kernel/drivers/usb/serial/ftdi_sio.ko", O_RDONLY);
                fstat(fd, &st);
                image_size = st.st_size;
                image = malloc(image_size);
                read(fd, image, image_size);
                close(fd);
                syscall(__NR_init_module, image, image_size, "");                                   // modprobe
                break;
        default:
            LE_DEBUG("Unknow command : %d",(int)aCmd);
            break;
    }
}

static void photoStatus
(
    void
)
{
    char* model = "on";
    char* tags = "[\"photo\", \"url\"]";
    le_result_t rc;
    int camReturn;

    char payload[100] = "";
    char consoleOutput[256];
    const char*   cmdSendPic    = "/usr/bin/python /mnt/flash/sendPic.py";
    char cmdUpLoadFile[256];
    char tryPic = 0;

    char fileToSave[MAX_PATH_SIZE];

    do {
        if (tryPic > 0) {
            removAddTtyUSB(E_REMOVE_TTY_USB);
            removAddTtyUSB(E_ADD_TTY_USB);
            sleep(3);
        }
        memset(fileToSave,0,sizeof(fileToSave));
        snprintf(fileToSave,sizeof(fileToSave),"%s%d.jpg","/tmp/",(int)time(0));
        tryPic++;
        camReturn = takePhoto(fileToSave);
    } while( (false == camReturn) && (tryPic <= 3) );
    
    if(true == camReturn) {
        snprintf(cmdUpLoadFile,sizeof(cmdUpLoadFile),"%s %s",cmdSendPic,fileToSave);
        sendSystemCommand(cmdUpLoadFile,consoleOutput, sizeof(consoleOutput));
        RemoveCharInString(consoleOutput, sizeof(consoleOutput),' ');
        RemoveCharInString(consoleOutput, sizeof(consoleOutput),'\n');
        snprintf(payload,sizeof(payload), "{\"photoURL\":\"%s\"}", consoleOutput);
    }
    else {
        snprintf(payload,sizeof(payload), "{\"photoURL\":\"%s\"}", "Camera KO");
    }
    
    // Reply with the photo url
    rc  = liveobjects_pubData(cmdResultStreamID, payload, model, tags, latitude, longitude);
    LE_INFO("Send pic : %s",(LE_OK == rc) ? "OK":"KO");
}

#define C_CMD_PHOTO         "photo"
#define C_CMD_ADDPHONE      "addphone"
#define C_CMD_REMOVEPHONE   "removephone"
#define C_CMD_ADDEMAIL      "addemail"
#define C_CMD_REMOVEEMAIL   "removeemail"

void smsHandler(char *aSmsBody,char *aSenderNb) {
    char smsContent[100];
    char senderNb[18];
    char cmd[256];
    const char*   cmdModyfyAlert    = "/usr/bin/python /mnt/flash/modifyNotification.py";
    memcpy(smsContent,aSmsBody,sizeof(smsContent));
    memcpy(senderNb,aSenderNb,sizeof(senderNb));
    memset(cmd, 0, sizeof(cmd));
    
    toLowerCase(smsContent,strlen(smsContent));
    RemoveCharInString(smsContent, strlen(smsContent),' ');
    RemoveCharInString(smsContent, strlen(smsContent),'\n');
    RemoveCharInString(senderNb, strlen(senderNb),' ');
    RemoveCharInString(senderNb, strlen(senderNb),'\n');

    LE_INFO("SMS content    : %s => length : %d",smsContent,strlen(smsContent));
    if ( 0 == strncmp(smsContent,C_CMD_PHOTO,strlen(C_CMD_PHOTO)) ) {
        LE_INFO("Photo command");
        photoStatus();
    }
    else if ( 0 == strncmp(smsContent,C_CMD_ADDPHONE,strlen(C_CMD_ADDPHONE)) ) {
        LE_INFO("Add Phone number command: %s",senderNb);
        snprintf(cmd,sizeof(cmd), "%s %s %s",cmdModyfyAlert,C_CMD_ADDPHONE,senderNb);
        sendSystemCommand(cmd,NULL, 0);
        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd,sizeof(cmd),"%s added to alert",senderNb);
        smsmo_SendMessage(senderNb,cmd);
    }
    else if ( 0 == strncmp(smsContent,C_CMD_REMOVEPHONE,strlen(C_CMD_REMOVEPHONE)) ) {
        LE_INFO("Remove Phone number command: %s",senderNb);    
        snprintf(cmd,sizeof(cmd), "%s %s %s",cmdModyfyAlert,C_CMD_REMOVEPHONE,senderNb);
        sendSystemCommand(cmd,NULL, 0);
        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd,sizeof(cmd),"%s removed from alert",senderNb);
        smsmo_SendMessage(senderNb,cmd);
    }
    else if ( 0 == strncmp(smsContent,C_CMD_ADDEMAIL,strlen(C_CMD_ADDEMAIL)) ) {
        LE_INFO("Add email command");   
        snprintf(cmd,sizeof(cmd), "%s %s %s",cmdModyfyAlert,C_CMD_ADDEMAIL,&smsContent[strlen(C_CMD_ADDEMAIL)]);
        sendSystemCommand(cmd,NULL, 0);
        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd,sizeof(cmd),"%s added to alert",&smsContent[strlen(C_CMD_ADDEMAIL)]);
        smsmo_SendMessage(senderNb,cmd);
    }
    else if ( 0 == strncmp(smsContent,C_CMD_REMOVEEMAIL,strlen(C_CMD_REMOVEEMAIL)) ) {
        LE_INFO("Remove email command"); 
        snprintf(cmd,sizeof(cmd), "%s %s %s",cmdModyfyAlert,C_CMD_REMOVEEMAIL,&smsContent[strlen(C_CMD_REMOVEEMAIL)]);
        sendSystemCommand(cmd,NULL, 0);
        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd,sizeof(cmd),"%s removed from alert",&smsContent[strlen(C_CMD_REMOVEEMAIL)]);
        smsmo_SendMessage(senderNb,cmd);
    }
    else {
        LE_INFO("Unknow command !");
        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd,sizeof(cmd),"Unknow command !: %s",smsContent);
        smsmo_SendMessage(senderNb,cmd);
    }
    
}
//--------------------------------------------------------------------------------------------------
/**
 * Toggle the LED when the timer expires
 */
//--------------------------------------------------------------------------------------------------
static void Led
(
    void
)
{
    if (LedOn)
    {
    	LE_INFO("turn off LED");
        ma_led_TurnOff();
        LedOn = false;
    }
    else
    {
    	LE_INFO("turn on LED");
        ma_led_TurnOn();
        LedOn = true;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get embeded light sensor value
 */
//--------------------------------------------------------------------------------------------------
static le_result_t LightSensor
(
    int32_t *reading
)
{
    le_result_t ret = le_adc_ReadValue("EXT_ADC3", reading);

    LE_INFO("Light sensor level = %d", *reading);

    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 * Push command result current status to LiveObjects
 */
//--------------------------------------------------------------------------------------------------
static void sendCommandResultStatus
(
    void
)
{
    char* model = "on";
    char* tags = "[\"hello\", \"world\"]";

    char payload[100] = "{\"hello\":\"world\"}";

    liveobjects_pubData(cmdResultStreamID, payload, model, tags, latitude, longitude);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Live Objects command handler
 */
//--------------------------------------------------------------------------------------------------
static void  command(
		char* req,
		char* params,
		int cid
)
{
    char result[256] = "true";
    
    LE_INFO("CMD %s", req);
    //send a "hello" request form Live Objects UI (from device/command page)
    // => {"hello" : "world"} message can be seen in the Live Objects Data page
    if (strcmp(req, "hello") == 0) {
        sendCommandResultStatus();
        liveobjects_pubCmdRes(result, cid);
    }
    else if (strcmp(req, "photo") == 0) {
        liveobjects_pubCmdRes(result, cid);
        photoStatus();
    }
    else if (strcmp(req, "led") == 0) {
        Led();
        LedPushStatus();
        liveobjects_pubCmdRes(result, cid);
    }

}

//--------------------------------------------------------------------------------------------------
/**
 *  Live Objects configuration request update handler
 *  format :
 *  	params: json object
 *  		{<<param1Key>>: {"t": <<param1Type>>, "v": <<param1Value>>}}
 *  	cid: correlationId
 *
 */
//--------------------------------------------------------------------------------------------------

static void  configUpdate(
		char* params,
		int cid

)
{
	int len;
	char *e;
	e = strchr(params, ':');
	len = (int)(e - params);
	char key[len];
	memset(key, '\0', sizeof(key));
	strncpy(key, &params[1], len- 2);

	char* type = swirjson_getValue(strdup(params), -1, (char *) "t");
	char* value = swirjson_getValue(strdup(params), -1, (char *) "v");

	LE_INFO("config request update : response  %s / %s / %s", key, type, value);

	liveobjects_pubConfigUpdateResponse(key, type, value,cid);
}

//--------------------------------------------------------------------------------------------------
/**
 *  Live Objects resource request update handler
 *  format :
 *  	"id": "<<resourceId>>",
 *	    "old": "<<resourceCurrentVersion>>",
 *	    "new": "<<resourceNewVersion>>",
 *	    "m": <<metadata>> JSON object ...,
 *	    "cid": "<<correlationId>>"
 *
 */
//--------------------------------------------------------------------------------------------------

static void  resourceUpdate(
		char* id,
		char* old,
		char* new,
		char* metadata,
		int correlationId

)
{

	LE_INFO("resource request update :  %s / %s / %s / %s", id, old, new, metadata);
	char response[256] = "";
	sprintf(response, "{%s}", swirjson_szSerialize("done", "true", 0));
	LE_INFO("resource request update : response %s", response);

	liveobjects_pubResourceUpdateResponse("true", correlationId);
}


//--------------------------------------------------------------------------------------------------
/**
 *  Live Objects event callback
 */
//--------------------------------------------------------------------------------------------------
static void OnIncomingMessage(
                const char* topicName,
                const char* key,    //could be empty
                const char* value,  //aka payload
                const char* timestamp,  //could be empty
                void*       pUserContext)
{
    LE_INFO("Received message from topic %s:", topicName);
    LE_INFO("   Message timestamp epoch: %s", timestamp);
    LE_INFO("   Parameter Name: %s", key);
    LE_INFO("   Parameter Value: %s", value);

    char* cid = swirjson_getValue(strdup(value), -1, (char *) "cid");

    if (strcmp(topicName, "dev/cmd") == 0)
    {
    	char* arg = swirjson_getValue(strdup(value), -1, (char *) "arg");
    	char* req = swirjson_getValue(strdup(value), -1, (char *) "req");
    	command(req, arg, atoi(cid));
    }
    else if (strcmp(topicName, "dev/cfg/upd") == 0)
    {
    	char* cfg = swirjson_getValue(strdup(value), -1, (char *) "cfg");
    	configUpdate(cfg, atoi(cid));
    }
    else if (strcmp(topicName, "dev/rsc/upd") == 0)
    {

    	char* id = swirjson_getValue(strdup(value), -1, (char *) "id");
    	char* old = swirjson_getValue(strdup(value), -1, (char *) "old");
    	char* new = swirjson_getValue(strdup(value), -1, (char *) "new");
    	char* m = swirjson_getValue(strdup(value), -1, (char *) "m");

    	resourceUpdate(id, old, new, m, atoi(cid));

    }
    else
    {
    	char* req = swirjson_getValue(strdup(value), -1, (char *) "req");
    	LE_INFO("Unknwon command : %s", req);
    }

}

//--------------------------------------------------------------------------------------------------
/**
 *  publish data to liveobjects
 */
//--------------------------------------------------------------------------------------------------
void demoTimer()
{

	char* model = "lightV1";
	char* tags = "[\"lightlevel\", \"count\"]";
	char pressureStr[100] = "";
	char temperatureStr[100] = "";

	char payload[100] = "";

    int32_t lightLevel = 0;
    double pressure = 0;
    double temperature = 0;

    LightSensor(&lightLevel);

    le_result_t result;
    result = mangOH_ReadPressureSensor(&pressure);

    if( result == LE_OK) {
    	sprintf(pressureStr, ",\"pressure\":%lf", pressure);
    }
    else {
    	LE_INFO("pressure error %d", result);
    }

    result = mangOH_ReadTemperatureSensor(&temperature);
    if(mangOH_ReadTemperatureSensor(&temperature) == LE_OK) {
    	sprintf(temperatureStr, ",\"temp\":%lf", temperature);
    }
    else {
       	LE_INFO("temperature error %d", result);
       }

	sprintf(payload, "{\"count\":%d, \"lightlevel\": %d%s%s}", count, lightLevel, pressureStr, temperatureStr);

	count = count + 1;

	liveobjects_pubData(timerStreamID, payload, model, tags, latitude, longitude);

}

//--------------------------------------------------------------------------------------------------
/**
 *  Called when the Live Objects connection is done
 */
//--------------------------------------------------------------------------------------------------
void connectionHandler()
{
	LE_INFO("Live Objects connection ready");

	liveobjects_AddIncomingMessageHandler(OnIncomingMessage);


	// push your device internal configuration
	// configuration update can be requested from Live Objects UI (cf configUpdate function)
	liveobjects_pubConfig("updateTimer", "str", "60000");

	// push your device resource version configuration
	// update can be pushed from Live Objects UI (cf resourceUpdate function)
	liveobjects_pubResource("firmware", "0.0.1", "{\"name\": \"StarterKit\"}");


}

//--------------------------------------------------------------------------------------------------
/**
 * App init.
 *
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    removAddTtyUSB(E_ADD_TTY_USB);
    SetsmsExternHandler((smsExternHandler)smsHandler);
    SetIoRaspExternHandler((ioRaspExternHandler)photoStatus);
	// configure Orange network settings
	dataProfile_set(_dataProfileIndex, _profileAPN, _profileAuth, _profileUser, _profilePwd);

	//connect to liveObjects
	le_info_GetImei(imei, sizeof(imei));
	liveobjects_connect(APIKEY, NAMESPACE, imei, connectionHandler);


	// start demo timer, publish a counter to LiveObjects
	dataPubTimerRef = le_timer_Create("Data publisher Timer");
	le_timer_SetMsInterval(dataPubTimerRef, DATA_TIMER_IN_MS);
	le_timer_SetRepeat(dataPubTimerRef, 0);
	le_timer_SetHandler(dataPubTimerRef, demoTimer);

	le_timer_Start(dataPubTimerRef);

	LE_INFO("=========================== Starter KIT LTE-M demo application started");

	ma_led_LedStatus_t ledStatus  = ma_led_GetLedStatus();

	if (ledStatus == MA_LED_OFF)
	{
	    LedOn = false;
	} else {
		LedOn = true;
	}
}
