#ifndef LIVEOBJECTS_H_INC
#define LIVEOBJECTS_H_INC

/**
 *  Live Objects MQTT emit topics
 */
char _topicConfig[] 			= "dev/cfg";
char _topicData[]				= "dev/data";
char _topicResource[] 			= "dev/rsc";
char _topicCommandRsp[] 		= "dev/cmd/res";
char _topicResourceUpdResp[] 	= "dev/rsc/upd/res";
char _topicResourceUpdErr[] 	= "dev/rsc/upd/err";

//----
/**
 *  Live Objects MQTT subscribe topics
 */
char _topicConfigUpdate[] 		= "dev/cfg/upd";
char _topicCommand[] 			= "dev/cmd";
char _topicResourceUpd[]		= "dev/rsc/upd";

LE_SHARED void liveobjects_connect(char* apikey, char* namespace, char* id, void* connectionHandler);
LE_SHARED le_result_t liveobjects_pubData(char* streamid, char* payload, char* model, char* tags, int latitude, int longitude);
LE_SHARED void liveobjects_pubCmdRes(char* jsonStr, int cid);
LE_SHARED void liveobjects_pubConfig(char* key, char* type, char* value);
LE_SHARED void liveobjects_pubConfigUpdateResponse(char* key, char* type, char* value, int cid);
LE_SHARED void liveobjects_pubResource(char* ressourceId, char* version, char* metadata);
LE_SHARED void liveobjects_pubResourceUpdateResponse(char* response, int correlationId);
LE_SHARED void liveobjects_pubResourceUpdateResponseError(char* errorCode, char* errorDetails);
LE_SHARED void liveobjects_AddIncomingMessageHandler(void* msgHandler);

#endif
