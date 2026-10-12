#include <string>
#include <vector>
#include <cstdio>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "nvs_flash.h"

#include "cJSON.h"

// =====================================================
// AIBOT CONFIGURATION
// =====================================================

static const char *TAG = "AIBOT";

#define WIFI_SSID       "YOUR_WIFI_NAME"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

#define OPENAI_API_KEY  "YOUR_API_KEY_HERE"
#define AI_MODEL        "gpt-4o-mini"

#define API_URL "https://api.openai.com/v1/chat/completions"

// Maximum conversation history entries, including system prompt.
#define MAX_MESSAGES 12

// =====================================================
// GLOBAL VARIABLES
// =====================================================

static bool wifi_connected = false;

struct ChatMessage {
    std::string role;
    std::string content;
};

static std::vector<ChatMessage> conversation;

// =====================================================
// WI-FI
// =====================================================

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
) {
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }

    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;

        ESP_LOGW(TAG, "Wi-Fi disconnected. Reconnecting...");
        esp_wifi_connect();
    }

    if (event_base == IP_EVENT &&
        event_id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
        ESP_LOGI(TAG, "Wi-Fi connected!");
    }
}

static void wifi_init() {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        nullptr
    ));

    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        nullptr
    ));

    wifi_config_t wifi_config = {};

    strncpy(
        reinterpret_cast<char *>(wifi_config.sta.ssid),
        WIFI_SSID,
        sizeof(wifi_config.sta.ssid) - 1
    );

    strncpy(
        reinterpret_cast<char *>(wifi_config.sta.password),
        WIFI_PASSWORD,
        sizeof(wifi_config.sta.password) - 1
    );

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(esp_wifi_set_config(
        WIFI_IF_STA,
        &wifi_config
    ));

    ESP_ERROR_CHECK(esp_wifi_start());
}

// =====================================================
// CONVERSATION MEMORY
// =====================================================

static void initialize_conversation() {
    conversation.clear();

    conversation.push_back({
        "system",
        "You are AIbot, a friendly, intelligent conversational "
        "assistant running on an ESP32-S3 device. "
        "Answer naturally and clearly. Keep spoken-style replies "
        "reasonably concise. Remember the conversation history "
        "provided to you. If you do not know something, say so."
    });
}

static void add_message(
    const std::string &role,
    const std::string &content
) {
    conversation.push_back({role, content});

    // Preserve the system prompt while limiting memory usage.
    while (conversation.size() > MAX_MESSAGES) {
        conversation.erase(conversation.begin() + 1);
    }
}

// =====================================================
// SEND MESSAGE TO AI
// =====================================================

static std::string ask_ai(const std::string &user_text) {
    if (!wifi_connected) {
        return "I am not connected to Wi-Fi.";
    }

    if (std::string(OPENAI_API_KEY) == "YOUR_API_KEY_HERE") {
        return "Please configure your API key in the firmware.";
    }

    // Build JSON request.
    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "model", AI_MODEL);

    cJSON *messages = cJSON_AddArrayToObject(root, "messages");

    // Include conversation history.
    for (const auto &msg : conversation) {
        cJSON *item = cJSON_CreateObject();

        cJSON_AddStringToObject(
            item,
            "role",
            msg.role.c_str()
        );

        cJSON_AddStringToObject(
            item,
            "content",
            msg.content.c_str()
        );

        cJSON_AddItemToArray(messages, item);
    }

    // Add the new user message.
    cJSON *user_message = cJSON_CreateObject();

    cJSON_AddStringToObject(
        user_message,
        "role",
        "user"
    );

    cJSON_AddStringToObject(
        user_message,
        "content",
        user_text.c_str()
    );

    cJSON_AddItemToArray(messages, user_message);

    cJSON_AddNumberToObject(root, "temperature", 0.7);
    cJSON_AddNumberToObject(root, "max_tokens", 250);

    char *json_body = cJSON_PrintUnformatted(root);

    if (json_body == nullptr) {
        cJSON_Delete(root);
        return "I could not prepare the request.";
    }

    // Collect the HTTP response.
    std::string response_body;

    esp_http_client_config_t config = {};

    config.url = API_URL;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 60000;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    config.user_data = &response_body;

    config.event_handler = [](esp_http_client_event_t *evt) {
        if (evt->event_id == HTTP_EVENT_ON_DATA &&
            evt->user_data != nullptr &&
            evt->data != nullptr &&
            evt->data_len > 0) {

            auto *buffer =
                static_cast<std::string *>(evt->user_data);

            buffer->append(
                static_cast<char *>(evt->data),
                evt->data_len
            );
        }

        return ESP_OK;
    };

    esp_http_client_handle_t client =
        esp_http_client_init(&config);

    if (client == nullptr) {
        free(json_body);
        cJSON_Delete(root);
        return "Could not initialize the network request.";
    }

    char authorization[256];

    snprintf(
        authorization,
        sizeof(authorization),
        "Bearer %s",
        OPENAI_API_KEY
    );

    esp_http_client_set_header(
        client,
        "Content-Type",
        "application/json"
    );

    esp_http_client_set_header(
        client,
        "Authorization",
        authorization
    );

    esp_http_client_set_post_field(
        client,
        json_body,
        strlen(json_body)
    );

    ESP_LOGI(TAG, "Thinking...");

    esp_err_t err = esp_http_client_perform(client);

    int status_code =
        esp_http_client_get_status_code(client);

    esp_http_client_cleanup(client);

    free(json_body);
    cJSON_Delete(root);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "HTTP request failed: %s",
            esp_err_to_name(err)
        );

        return "I could not reach the AI service. Check your internet.";
    }

    if (status_code < 200 || status_code >= 300) {
        ESP_LOGE(TAG, "API HTTP status: %d", status_code);
        ESP_LOGE(TAG, "API response: %s", response_body.c_str());

        if (status_code == 401) {
            return "The API key was rejected. Check your configuration.";
        }

        if (status_code == 429) {
            return "The API request limit was reached. Check your account.";
        }

        return "The AI service returned an error. Check the serial log.";
    }

    // Parse AI response.
    cJSON *response_json =
        cJSON_Parse(response_body.c_str());

    if (response_json == nullptr) {
        return "I received an invalid response from the AI.";
    }

    cJSON *choices = cJSON_GetObjectItem(
        response_json,
        "choices"
    );

    cJSON *choice = cJSON_GetArrayItem(choices, 0);

    cJSON *message = cJSON_GetObjectItem(
        choice,
        "message"
    );

    cJSON *content = cJSON_GetObjectItem(
        message,
        "content"
    );

    std::string reply;

    if (cJSON_IsString(content) && content->valuestring) {
        reply = content->valuestring;
    } else {
        reply = "I could not understand the AI response.";
    }

    cJSON_Delete(response_json);

    // Save both sides of the conversation.
    add_message("user", user_text);
    add_message("assistant", reply);

    return reply;
}

// =====================================================
// SERIAL CHAT INTERFACE
// =====================================================

static std::string read_line() {
    std::string input;

    while (true) {
        int ch = getchar();

        if (ch == EOF) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (ch == '\r' || ch == '\n') {
            if (!input.empty()) {
                printf("\n");
                return input;
            }

            continue;
        }

        if (ch == 8 || ch == 127) {
            if (!input.empty()) {
                input.pop_back();
                printf("\b \b");
            }

            continue;
        }

        if (ch >= 32 && ch <= 126) {
            input += static_cast<char>(ch);
            putchar(ch);
            fflush(stdout);
        }
    }
}

// =====================================================
// MAIN
// =====================================================

extern "C" void app_main(void) {
    printf("\n");
    printf("====================================\n");
    printf("          AIBOT STARTING\n");
    printf("====================================\n");

    wifi_init();

    printf("Connecting to Wi-Fi");

    while (!wifi_connected) {
        printf(".");
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    printf("\nWi-Fi connected!\n");

    initialize_conversation();

    printf("\n");
    printf("AIbot is ready.\n");
    printf("Type a message and press Enter.\n");
    printf("Type /reset to clear conversation memory.\n");
    printf("------------------------------------\n");

    while (true) {
        printf("\nYou: ");
        fflush(stdout);

        std::string user_input = read_line();

        if (user_input == "/reset") {
            initialize_conversation();
            printf("AIbot: Conversation memory cleared.\n");
            continue;
        }

        if (user_input.empty()) {
            continue;
        }

        printf("\nAIbot: Thinking...\n");

        std::string reply = ask_ai(user_input);

        printf("\nAIbot: %s\n", reply.c_str());
    }
}