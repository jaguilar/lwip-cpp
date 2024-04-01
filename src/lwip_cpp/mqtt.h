#include <memory>
#include <string>
#include <string_view>

#include "error.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/ip_addr.h"

namespace lwip {

class MqttClient {
 public:
  ~MqttClient() {
    if (client_) mqtt_client_free(client_);
    if (queue_) vQueueDelete(queue_);
  }

  struct Options {
    std::string_view username;
    std::string_view password;
  };

  static Maybe<std::unique_ptr<MqttClient>> Create(std::string_view hostname,
                                                   const Options& options);

  static Maybe<std::unique_ptr<MqttClient>> Create(ip_addr_t address,
                                                   const Options& options) {
    const struct mqtt_connect_client_info_t client_info = {
        .client_id = "pingpong",
        .client_user = options.username.data(),
        .client_pass = options.password.data(),
    };

    client = std::make_unique<MqttClient>();
    client->client_ = mqtt_client_new();
    client->queue_ = xQueueCreate(10, sizeof(std::string*));

    {
      QueueHandle_t result_queue =
          xQueueCreate(1, sizeof(mqtt_connection_status_t));
      mqtt_connection_cb_t cb = +[](mqtt_client_s* client, void* queue_arg,
                                    mqtt_connection_status_t status) {
        xQueueSend(static_cast<QueueHandle_t>(queue_arg), &status,
                   portMAX_DELAY);
      };
      err = mqtt_client_connect(client->client_, &mqtt_server_address, 1883, cb,
                                result_queue, &client_info);
      if (err != ERR_OK) {
        panic("error from mqtt_client_connect: %s\n", lwip_strerr(err));
      }
      mqtt_connection_status_t status;
      if (xQueueReceive(result_queue, &status, portMAX_DELAY) != pdTRUE) {
        panic("failed to receive error from queue\n");
      }
      if (status != MQTT_CONNECT_ACCEPTED) {
        panic("error connecting to mqtt: %d\n", status);
      }
    }

    xTaskCreate(
        +[](void* arg) { static_cast<MqttClient*>(arg)->WriterTask(); },
        "writer", 512, client.get(), 1, &client->writer_task_);

    mqtt_incoming_publish_cb_t pub_cb =
        +[](void* self, const char* topic, u32_t num_data) {
          static_cast<MqttClient*>(self)->SetTopic(std::string_view(topic),
                                                   num_data);
        };
    mqtt_incoming_data_cb_t data_cb =
        +[](void* self, const u8_t* data, u16_t len, u8_t flags) {
          static_cast<MqttClient*>(self)->RecvData(
              std::string_view(reinterpret_cast<const char*>(data), len));
        };
    mqtt_set_inpub_callback(client->client_, pub_cb, data_cb, client.get());
    err = mqtt_subscribe(client->client_, "/ping", 1, NULL, NULL);
    if (err != ERR_OK) {
      panic("error subscribing to mqtt: %s\n", lwip_strerr(err));
    }
    return 0;
  }

 private:
  MqttClient() { topic_.reserve(32); }

  void SetTopic(std::string_view topic, int num_data) {
    topic_.assign(topic);
    num_expected_items_ = num_data;
  }

  void RecvData(std::string_view payload) {
    --num_expected_items_;
    if (num_expected_items_ == 0) {
      topic_ = "";
    }
    if (num_expected_items_ < 0) {
      panic("more than the expected number of items received\n");
    }
    if (topic_ != "/ping") return;
    std::string* buffer = new std::string(payload);
    xQueueSend(queue_, &buffer, portMAX_DELAY);
  }

  static void WriterTask(void* arg) {
    reinterpret_cast<MqttClient*>(arg)->WriterTask();
  }

  void WriterTask() {
    while (true) {
      std::string* buffer_ptr;
      if (xQueueReceive(queue_, &buffer_ptr, pdMS_TO_TICKS(1000)) != pdTRUE) {
        continue;
      }
      std::unique_ptr<std::string> buffer(buffer_ptr);

      err_t err;
      if ((err = mqtt_publish(client_, "/pong", buffer->data(), buffer->size(),
                              1, false, NULL, NULL)) != ERR_OK) {
        printf("error publishing to mqtt: %s\n", lwip_strerr(err));
      }
    }
  }

  mqtt_client_t* client_ = nullptr;
  TaskHandle_t writer_task_ = nullptr;
  QueueHandle_t queue_ = nullptr;

  std::string topic_;
  int num_expected_items_ = 0;
};

}  // namespace lwip