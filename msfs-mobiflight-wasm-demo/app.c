#include <stdio.h>
#include <windows.h>
#include <SimConnect.h>
#include <string.h>

void initializeClientDataArea(HANDLE hSimConnect);
HRESULT subscribeToSimVar(HANDLE hSimConnect, const char* varname);
HRESULT sendCommand(HANDLE hSimConnect, char* command);
HRESULT sendCommandViaDefaultArea(HANDLE hSimConnect, char* command);

enum CLIENT_DATA_ID {
	AREA_SIMVAR_ID,
	AREA_COMMAND_ID,
	AREA_MOBI_COMMAND_ID,
	AREA_RESPONSE_ID,
	AREA_STRINGVAR_ID,
	RESPONSE_DEFINITION_ID,
	COMMAND_DEFINITION_ID,
	RESPONSE_REQUEST_ID,
	SIMVAR_REQUEST_ID,
};

const int SIMVAR_DEFINE_OFFSET = 1000;
int SIMVAR_COUNT = 0;
const int MOBIFLIGHT_MESSAGE_SIZE = 1024;
const int MOBIFLIGHT_STRINGVAR_SIZE = 128;
const int MOBIFLIGHT_STRINGVAR_MAX_AMOUNT = 64;
const int MOBIFLIGHT_STRINGVAR_DATAAREA_SIZE = MOBIFLIGHT_STRINGVAR_SIZE * MOBIFLIGHT_STRINGVAR_MAX_AMOUNT;
const char CLIENT_NAME[] = "TEST_CLIENT";

struct ResponseString {
	char value[MOBIFLIGHT_MESSAGE_SIZE];
};

struct LVarData {
	float value;
};

HANDLE hSimConnect = NULL;

void CALLBACK MyDispatchProc(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext) {
	SIMCONNECT_RECV_EXCEPTION* except;
	SIMCONNECT_RECV_OPEN* openData;
	SIMCONNECT_RECV_CLIENT_DATA* pClientData;
	switch (pData->dwID) {
	case SIMCONNECT_RECV_ID_CLIENT_DATA:
		pClientData = (SIMCONNECT_RECV_CLIENT_DATA*)pData;
		if (pClientData->dwRequestID == RESPONSE_REQUEST_ID) {
			struct ResponseString* pS = (struct ResponseString*)&pClientData->dwData;
			printf("Command: %s\n", pS->value);
		}
		else if (pClientData->dwRequestID >= SIMVAR_DEFINE_OFFSET) {
			LVarData* pS = (LVarData*)&pClientData->dwData;
			printf("SimVar: %f\n", pS->value);
		}
		else {
			printf("Unknown request ID: %d\n", pClientData->dwRequestID);
		}
		break;
	case SIMCONNECT_RECV_ID_EXCEPTION:
		except = (SIMCONNECT_RECV_EXCEPTION*)pData;
		printf("Exception: %d\n", except->dwException);
		break;
	case SIMCONNECT_RECV_ID_OPEN:
		openData = (SIMCONNECT_RECV_OPEN*)pData;
		printf("Connected to %s\n", openData->szApplicationName);
		break;
	default:
		printf("Unknown packet ID: %d\n", pData->dwID);
		break;
	}
}


int main() {
	HRESULT hr;

	if (SUCCEEDED(SimConnect_Open(&hSimConnect, "SimConnect Test", NULL, 0, 0, 0))) {
		printf("Connected to Flight Simulator!\n");

		char* command = (char*)malloc(MOBIFLIGHT_MESSAGE_SIZE);
		if (command == NULL) {
			printf("Failed to allocate memory for command!\n");
			return 1;
		}

		// 初始化需要的ClientArea和DataDefinition
		initializeClientDataArea(hSimConnect);

		// Add a Client
		// 先通过默认CommandArea放松Add命令，然后再通过对应自己client的CommandArea发送其他命令
		sprintf_s(command, MOBIFLIGHT_MESSAGE_SIZE, "THIS COMMAND MIGHT BE IGNORED");
		hr = sendCommandViaDefaultArea(hSimConnect, command);
		sprintf_s(command, MOBIFLIGHT_MESSAGE_SIZE, "MF.Clients.Add.%s", CLIENT_NAME);
		hr = sendCommandViaDefaultArea(hSimConnect, command);

		// Clear All SimVars (Necessary?)
		sprintf_s(command, MOBIFLIGHT_MESSAGE_SIZE, "MF.SimVars.Clear");
		hr = sendCommand(hSimConnect, command);

		// Subscribe a SimVar
		hr = subscribeToSimVar(hSimConnect, "L:S_OH_EXT_LT_NOSE");
		// Subscribe another SimVar
		hr = subscribeToSimVar(hSimConnect, "L:A_OH_PNEUMATIC_FWD_TEMP");

		while (1) {
			SimConnect_CallDispatch(hSimConnect, MyDispatchProc, NULL);
		}

		//while (1) {
		//	printf("Input -> ");
		//	double val;
		//	if (scanf_s("%lf", &val) == 1) {
		//		if (val == -1) break;
		//		sprintf_s(command, MOBIFLIGHT_MESSAGE_SIZE, "MF.SimVars.Set.%f (>L:S_OH_EXT_LT_NOSE)", val);
		//		sendCommand(hSimConnect, command);
		//	}
		//	else {
		//		printf("Invalid input! Please enter a valid integer.\n");
		//		while (getchar() != '\n'); // clear input buffer
		//		continue;
		//	}
		//	SimConnect_CallDispatch(hSimConnect, MyDispatchProc, NULL);
		//}

		hr = SimConnect_Close(hSimConnect);
	}
	else {
		printf("Failed to connect to Flight Simulator!\n");
	}
	return 0;
}

void initializeClientDataArea(HANDLE hSimConnect) {
	HRESULT hr;
	// offset 3, because first two definitions are the client response channels and the built-in aircraft name
	static int SIMVAR_DATA_DEFINITION_OFFSET = 3;

	const int CHANNEL_NAME_SIZE = 100;
	char* channelName = (char*)malloc(CHANNEL_NAME_SIZE);
	if (channelName == NULL) {
		printf("Failed to allocate memory for channel name!\n");
		return;
	}

	// register MobiFlight Default Command Area
	hr = SimConnect_MapClientDataNameToID(hSimConnect, "MobiFlight.Command", AREA_MOBI_COMMAND_ID);

	// register SimVar
	sprintf_s(channelName, CHANNEL_NAME_SIZE, "%s.LVars", CLIENT_NAME);
	hr = SimConnect_MapClientDataNameToID(hSimConnect, channelName, AREA_SIMVAR_ID);

	// register Command
	sprintf_s(channelName, CHANNEL_NAME_SIZE, "%s.Command", CLIENT_NAME);
	hr = SimConnect_MapClientDataNameToID(hSimConnect, channelName, AREA_COMMAND_ID);

	// register Response
	sprintf_s(channelName, CHANNEL_NAME_SIZE, "%s.Response", CLIENT_NAME);
	hr = SimConnect_MapClientDataNameToID(hSimConnect, channelName, AREA_RESPONSE_ID);

	// register StringVars
	sprintf_s(channelName, CHANNEL_NAME_SIZE, "%s.StringVars", CLIENT_NAME);
	hr = SimConnect_MapClientDataNameToID(hSimConnect, channelName, AREA_STRINGVAR_ID);

	// response data definition
	hr = SimConnect_AddToClientDataDefinition(hSimConnect, RESPONSE_DEFINITION_ID, 0, MOBIFLIGHT_MESSAGE_SIZE);

	// command data definition
	hr = SimConnect_AddToClientDataDefinition(hSimConnect, COMMAND_DEFINITION_ID, 0, MOBIFLIGHT_MESSAGE_SIZE);

	// Listen to the response
	hr = SimConnect_RequestClientData(hSimConnect, AREA_RESPONSE_ID, RESPONSE_REQUEST_ID, RESPONSE_DEFINITION_ID, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED);
}

HRESULT subscribeToSimVar(HANDLE hSimConnect, const char* varname) {
	HRESULT hr;
	char* command = (char*)malloc(MOBIFLIGHT_MESSAGE_SIZE);
	if (command == NULL) {
		printf("Failed to allocate memory for command!\n");
		return E_FAIL;
	}

	int id = SIMVAR_DEFINE_OFFSET + SIMVAR_COUNT;
	int offset = SIMVAR_COUNT * sizeof(float);
	// Add a SimVar (WASM)
	sprintf_s(command, MOBIFLIGHT_MESSAGE_SIZE, "MF.SimVars.Add.(%s)", varname);
	hr = sendCommand(hSimConnect, command);
	// Add a SimVar in data definition
	hr = SimConnect_AddToClientDataDefinition(hSimConnect, id, offset, SIMCONNECT_CLIENTDATATYPE_FLOAT32);
	hr = SimConnect_RequestClientData(hSimConnect, AREA_SIMVAR_ID, id, id, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET, SIMCONNECT_CLIENT_DATA_REQUEST_FLAG_CHANGED);
	SIMVAR_COUNT++;
	printf("Subscribing to SimVar: %s\n", command);
	return hr;
}

HRESULT sendCommand(HANDLE hSimConnect, char* command) {
	HRESULT hr;
	hr = SimConnect_SetClientData(hSimConnect, AREA_COMMAND_ID, COMMAND_DEFINITION_ID, NULL, 0, MOBIFLIGHT_MESSAGE_SIZE, command);
	return hr;
}

HRESULT sendCommandViaDefaultArea(HANDLE hSimConnect, char* command) {
	HRESULT hr;
	hr = SimConnect_SetClientData(hSimConnect, AREA_MOBI_COMMAND_ID, COMMAND_DEFINITION_ID, NULL, 0, MOBIFLIGHT_MESSAGE_SIZE, command);
	return hr;
}

