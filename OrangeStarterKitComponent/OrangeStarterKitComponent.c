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

#define DATA_TIMER_IN_MS (60000)

static le_timer_Ref_t dataPubTimerRef;

int latitude = 0;
int longitude = 0;

static bool LedOn;

//-----
/**
 * Live Objects Settings
 */

char* APIKEY =  "000000000000000000000000000000000"; //a valid API key value

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
	LE_INFO("CMD %s", req);
	//send a "hello" request form Live Objects UI (from device/command page)
	// => {"hello" : "world"} message can be seen in the Live Objects Data page
	if (strcmp(req, "hello") == 0) {
		sendCommandResultStatus();
	}
	//send a led request form Live Objects UI (from device/command page)
	// => {"led" : <<led status>> } message can be seen in the Live Objects Data page
	else if (strcmp(req, "led") == 0) {
		Led();
		LedPushStatus();
	}

	char result[256] = "true";
	liveobjects_pubCmdRes(result, cid);
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
