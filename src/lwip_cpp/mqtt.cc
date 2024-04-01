#include "mqtt.h"

#include <format>
#include <memory>

namespace lwip {

static Maybe<std::unique_ptr<typename Tp><MqttClient>> MqttClient::Create(
    ip_addr_t address, const Options& options) {
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
      xQueueSend(static_cast<QueueHandle_t>(queue_arg), &status, portMAX_DELAY);
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
      +[](void* arg) { static_cast<MqttClient*>(arg)->WriterTask(); }, "writer",
      512, client.get(), 1, &client->writer_task_);

  mqtt_incoming_publish_cb_t pub_cb = +[](void* self, const char* topic,
                                          u32_t num_data) {
    static_cast<MqttClient*>(self)->SetTopic(std::string_view(topic), num_data);
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

Maybe<std::unique_ptr<MqttClient>> MqttClient::Create(std::string_view hostname,
                                                      const Options& options) {
  ip_addr_t ipaddr;
  err_t err = netconn_gethostbyname(hostname, &ipaddr);
  if (err != ERR_OK) {
    return Error{ErrorCode::UNKNOWN,
                 std::format("resolve %s: %s", hostname, lwip_strerr(err))};
  }
  return Create(ipaddr);
}

}  // namespace lwip