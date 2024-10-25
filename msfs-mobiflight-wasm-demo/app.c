#include <stdio.h>
#include <windows.h>
#include <SimConnect.h>
#include <string.h>

void initializeClientDataArea(HANDLE hSimConnect);
HRESULT subscribeToSimVar(HANDLE hSimConnect, const char* varname);
HRESULT sendCommand(HANDLE hSimConnect, char* command);

enum CLIENT_DATA_ID {
	AREA_SIMVAR_ID,
	AREA_COMMAND_ID,
	AREA_RESPONSE_ID,
	AREA_STRINGVAR_ID,
	RESPONSE_DEFINITION_ID,
	RESPONSE_REQUEST_ID,
	SIMVAR_REQUEST_ID,
};

const int SIMVAR_DEFINE_OFFSET = 1000;
int SIMVAR_COUNT = 0;
const int MOBIFLIGHT_MESSAGE_SIZE = 1024;
const int MOBIFLIGHT_STRINGVAR_SIZE = 128;
const int MOBIFLIGHT_STRINGVAR_MAX_AMOUNT = 64;
const int MOBIFLIGHT_STRINGVAR_DATAAREA_SIZE = MOBIFLIGHT_STRINGVAR_SIZE * MOBIFLIGHT_STRINGVAR_MAX_AMOUNT;

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

		initializeClientDataArea(hSimConnect);

		char* command = (char*)malloc(MOBIFLIGHT_MESSAGE_SIZE);
		if (command == NULL) {
			printf("Failed to allocate memory for command!\n");
			return 1;
		}

		sprintf_s(command, MOBIFLIGHT_MESSAGE_SIZE, "THIS COMMAND MIGHT BE IGNORED");
		hr = sendCommand(hSimConnect, command);

		// Add a Client
		sprintf_s(command, MOBIFLIGHT_MESSAGE_SIZE, "MF.Clients.Add.TestClient");
		hr = sendCommand(hSimConnect, command);

		// Clear All SimVars
		sprintf_s(command, MOBIFLIGHT_MESSAGE_SIZE, "MF.SimVars.Clear");
		hr = sendCommand(hSimConnect, command);

		Sleep(1000);

		// Subscribe a SimVar
		hr = subscribeToSimVar(hSimConnect, "L:S_OH_EXT_LT_NOSE");
		// Subscribe another SimVar
		hr = subscribeToSimVar(hSimConnect, "L:A_OH_PNEUMATIC_FWD_TEMP");

		while (1) {
			SimConnect_CallDispatch(hSimConnect, MyDispatchProc, NULL);
		}

		while (1) {
			printf("Input -> ");
			double val;
			if (scanf_s("%lf", &val) == 1) {
				if (val == -1) break;
				sprintf_s(command, MOBIFLIGHT_MESSAGE_SIZE, "MF.SimVars.Set.%f (>L:S_OH_EXT_LT_NOSE)", val);
				sendCommand(hSimConnect, command);
			}
			else {
				printf("Invalid input! Please enter a valid integer.\n");
				while (getchar() != '\n'); // clear input buffer
				continue;
			}
			SimConnect_CallDispatch(hSimConnect, MyDispatchProc, NULL);
		}

		hr = SimConnect_Close(hSimConnect);
	}
	else {
		printf("Failed to connect to Flight Simulator!\n");
	}
	return 0;
}

void initializeClientDataArea(HANDLE hSimConnect) {
	HRESULT hr;
	static int RESPONSE_OFFSET = 0;
	// offset 3, because first two definitions are the client response channels and the built-in aircraft name
	static int SIMVAR_DATA_DEFINITION_OFFSET = 3;

	// register SimVar
	hr = SimConnect_MapClientDataNameToID(hSimConnect, "MobiFlight.LVars", AREA_SIMVAR_ID);

	// register Command
	hr = SimConnect_MapClientDataNameToID(hSimConnect, "MobiFlight.Command", AREA_COMMAND_ID);

	// register Response
	hr = SimConnect_MapClientDataNameToID(hSimConnect, "MobiFlight.Response", AREA_RESPONSE_ID);

	// register StringVars
	hr = SimConnect_MapClientDataNameToID(hSimConnect, "MobiFlight.StringVars", AREA_STRINGVAR_ID);

	hr = SimConnect_AddToClientDataDefinition(hSimConnect, RESPONSE_DEFINITION_ID, RESPONSE_OFFSET, MOBIFLIGHT_MESSAGE_SIZE);

	// Listen to the response
	hr = SimConnect_RequestClientData(hSimConnect, AREA_RESPONSE_ID, RESPONSE_REQUEST_ID, RESPONSE_DEFINITION_ID, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED);
}

HRESULT subscribeToSimVar(HANDLE hSimConnect, const char* varname) {
	HRESULT hr;
	char* command = (char*)malloc(MOBIFLIGHT_MESSAGE_SIZE);
	int id = SIMVAR_DEFINE_OFFSET + SIMVAR_COUNT;
	int offset = SIMVAR_COUNT * sizeof(float);
	hr = SimConnect_AddToClientDataDefinition(hSimConnect, id, offset, sizeof(float));
	hr = SimConnect_RequestClientData(hSimConnect, AREA_SIMVAR_ID, id, id, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET, SIMCONNECT_CLIENT_DATA_REQUEST_FLAG_CHANGED);
	sprintf_s(command, MOBIFLIGHT_MESSAGE_SIZE, "MF.SimVars.Add.(%s)", varname);
	hr = sendCommand(hSimConnect, command);
	printf("Subscribing to SimVar: %s\n", command);
	SIMVAR_COUNT++;
	return hr;
}

HRESULT sendCommand(HANDLE hSimConnect, char* command) {
	HRESULT hr;
	hr = SimConnect_SetClientData(hSimConnect, AREA_COMMAND_ID, RESPONSE_DEFINITION_ID, NULL, 0, MOBIFLIGHT_MESSAGE_SIZE, command);
	return hr;
}

