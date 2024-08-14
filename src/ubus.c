#include "ubus.h"

#include "log_level.h"
#include "tuya_action_log.h"
#include <syslog.h>
#include <libubox/blobmsg_json.h>

struct ubus_context *g_ubus_context;

enum {
	SYSTEM_INFO_MEMORY,
	SYSTEM_INFO_LOAD,
	SYSTEM_INFO_UPTIME,
	__SYSTEM_INFO_MAX,
};

enum {
	SYSTEM_INFO_MEMORY_TOTAL,
	SYSTEM_INFO_MEMORY_FREE,
	__SYSTEM_INFO_MEMORY_MAX,
};

enum {
	ESP_RESPONSE_RESULT,
	ESP_RESPONSE_MESSAGE,
	ESP_RESPONSE_DATA,
	__ESP_RESPONSE_MAX
};

static const struct blobmsg_policy system_info_policy[__SYSTEM_INFO_MAX] = {
	[SYSTEM_INFO_MEMORY] = {.name = "memory", .type = BLOBMSG_TYPE_TABLE},
	[SYSTEM_INFO_UPTIME] = {.name = "uptime", .type = BLOBMSG_TYPE_INT32},
	[SYSTEM_INFO_LOAD] = {.name = "load", .type = BLOBMSG_TYPE_ARRAY},
};

static const struct blobmsg_policy system_info_memory_policy[__SYSTEM_INFO_MEMORY_MAX] = {
	[SYSTEM_INFO_MEMORY_TOTAL] = {.name = "total", .type = BLOBMSG_TYPE_INT64},
	[SYSTEM_INFO_MEMORY_FREE] = {.name = "free", .type = BLOBMSG_TYPE_INT64},
};

static const struct blobmsg_policy esp_response_policy[__ESP_RESPONSE_MAX] = {
	[ESP_RESPONSE_RESULT] = {.name = "result", .type = BLOBMSG_TYPE_STRING},
	[ESP_RESPONSE_MESSAGE] = {.name = "message", .type = BLOBMSG_TYPE_STRING},
	[ESP_RESPONSE_DATA] = {.name = "data", .type = BLOBMSG_TYPE_STRING},
};

static void ubus_parse_esp_response(struct ubus_request *req, int type, struct blob_attr *msg) {
	struct EspResponse *esp_response = (struct EspResponse *) req->priv;
	struct blob_attr *esp_response_table[__SYSTEM_INFO_MAX];

	blobmsg_parse(esp_response_policy, __ESP_RESPONSE_MAX, esp_response_table, blob_data(msg), blob_len(msg));

	if (esp_response_table[ESP_RESPONSE_RESULT] == NULL) {
		esp_response->parsed_successfuly = false;
		return;
	}

	char *result = blobmsg_get_string(esp_response_table[ESP_RESPONSE_RESULT]);
	esp_response->success = strcmp(result, "ok") == 0 ? true : false;

	if (esp_response_table[ESP_RESPONSE_MESSAGE] != NULL) {
		char *message = blobmsg_get_string(esp_response_table[ESP_RESPONSE_MESSAGE]);
		esp_response->message = calloc(strlen(message) + 1, sizeof(char));
		strcpy(esp_response->message, message);
	}
	if (esp_response_table[ESP_RESPONSE_DATA] != NULL) {
		char *data = blobmsg_get_string(esp_response_table[ESP_RESPONSE_DATA]);
		esp_response->data = calloc(strlen(data) + 1, sizeof(char));
		strcpy(esp_response->data, data);
	}
}

static void ubus_parse_system_info(struct ubus_request *req, int type, struct blob_attr *msg) {
	struct SystemInfo *systemInfo = (struct SystemInfo *) req->priv;
	struct blob_attr *system_info[__SYSTEM_INFO_MAX];
	struct blob_attr *system_info_memory[__SYSTEM_INFO_MEMORY_MAX];

	blobmsg_parse(system_info_policy, __SYSTEM_INFO_MAX, system_info, blob_data(msg), blob_len(msg));

	if (system_info[SYSTEM_INFO_MEMORY] == NULL ||
	    system_info[SYSTEM_INFO_LOAD] == NULL ||
	    system_info[SYSTEM_INFO_UPTIME] == NULL) {
		systemInfo->parsed_successfuly = false;
		return;
	}

	blobmsg_parse(system_info_memory_policy, __SYSTEM_INFO_MEMORY_MAX, system_info_memory,
	              blobmsg_data(system_info[SYSTEM_INFO_MEMORY]),
	              blobmsg_data_len(system_info[SYSTEM_INFO_MEMORY]));

	systemInfo->total = blobmsg_get_u64(system_info_memory[SYSTEM_INFO_MEMORY_TOTAL]);
	systemInfo->free = blobmsg_get_u64(system_info_memory[SYSTEM_INFO_MEMORY_FREE]);
	systemInfo->uptime = blobmsg_get_u32(system_info[SYSTEM_INFO_UPTIME]);
	systemInfo->parsed_successfuly = true;

	struct blob_attr *loads = system_info[SYSTEM_INFO_LOAD];
	struct blob_attr *cur;
	size_t rem;
	int i = 0;
	blobmsg_for_each_attr(cur, loads, rem) {
		if (i >= 3) {
			break;
		}
		systemInfo->load[i] = blobmsg_get_u32(cur);
		i++;
	}
}

struct SystemInfo *ubus_get_system_info(struct SystemInfo *systemInfo, struct ubus_context *ctx) {
	unsigned int id;
	if (ubus_lookup_id(ctx, "system", &id) ||
	    ubus_invoke(ctx, id, "info", NULL, ubus_parse_system_info, systemInfo, 3000)) {
		syslog(LOG_LEVEL_ERROR, "Failed to request system info from system.");
		return NULL;
	}

	if (!systemInfo->parsed_successfuly) {
		syslog(LOG_LEVEL_DEBUG, "Failed to parse system info from ubus.");
		return NULL;
	}

	return systemInfo;
}


bool ubus_invoke_toggle_esp_pin(struct EspRequest request, struct EspResponse *response) {
	unsigned int id;
	if (ubus_lookup_id(g_ubus_context, "commesp", &id) != 0) {
		return false; // TODO
	}

	struct blob_buf ubus_message = {};
	blob_buf_init(&ubus_message, 0);
	esp_create_ubus_request_blobmsg(&ubus_message, ESP_ACTION_TOGGLE_PIN, request);
	if (ubus_invoke(g_ubus_context, // TODO ugly
	                id,
	                request.pin_power ? "on" : "off",
	                ubus_message.head,
	                ubus_parse_esp_response,
	                response,
	                3000) != 0) {
		return false; // TODO leak
	}

	blob_buf_free(&ubus_message);

	return true; // TODO
}

// TODO
// Implementuot toggle_pin error'us
// Implementuot list_devices (šlykštus)
// Implementuot list_devices error'us
// Sutvarkyt UBUS error'us tuya action'e
// Sutvarkyt main'ą
// Ubus init ir deinit

bool ubus_invoke_list_devices(void) {
	unsigned int id;
	if (ubus_lookup_id(g_ubus_context, "commesp", &id) != 0) {
		return false;
	}

	return true;
}

bool ubus_init() {
	// TODO
}

void ubus_deinit() {
	// TODO
}
