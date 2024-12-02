//igual a R2 pero realiza el ciclo 5 veces y apaga todo

/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity_test_utils_memory.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_io_expander_ht8574.h"

// Configuración para el ESP32 con HT8574
#define I2C_MASTER_SCL_IO   15          /*!< GPIO para I2C master clock */
#define I2C_MASTER_SDA_IO   4           /*!< GPIO para I2C master data  */
#define I2C_MASTER_NUM      I2C_NUM_0   /*!< Número del puerto I2C */
#define I2C_ADDRESS         ESP_IO_EXPANDER_I2C_HT8574_ADDRESS_000 /*!< Dirección I2C del expansor */

// Pines de salida 1 a 6 (mapeados según el expansor)
#define OUTPUT_PINS { IO_EXPANDER_PIN_NUM_0, IO_EXPANDER_PIN_NUM_1, IO_EXPANDER_PIN_NUM_2, \
                      IO_EXPANDER_PIN_NUM_3, IO_EXPANDER_PIN_NUM_4, IO_EXPANDER_PIN_NUM_5 }

static const char *TAG = "ht8574_control";
static esp_io_expander_handle_t io_expander = NULL;
static i2c_master_bus_handle_t i2c_handle = NULL;

// Inicialización del bus I2C
static void i2c_bus_init(void)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &i2c_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error en la instalación del bus I2C");
        return;
    }
}

// Desinicialización del bus I2C
static void i2c_bus_deinit(void)
{
    esp_err_t ret = i2c_del_master_bus(i2c_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al desinstalar el bus I2C");
    }
}

// Inicialización de la expansora de IO HT8574
static void io_expander_init(void)
{
    esp_err_t ret = esp_io_expander_new_i2c_ht8574(i2c_handle, I2C_ADDRESS, &io_expander);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al crear la expansora HT8574");
        return;
    }

    // Configurar los pines 1 a 6 como salidas
    uint32_t output_pins_mask = IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2 |
                                 IO_EXPANDER_PIN_NUM_3 | IO_EXPANDER_PIN_NUM_4 | IO_EXPANDER_PIN_NUM_5;
    ret = esp_io_expander_set_dir(io_expander, output_pins_mask, IO_EXPANDER_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al configurar los pines de salida");
    }
}

// Desinicialización de la expansora de IO HT8574
static void io_expander_deinit(void)
{
    esp_err_t ret = esp_io_expander_del(io_expander);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al eliminar la expansora HT8574");
    }
}

// Tarea principal para encender y apagar las salidas de la 1 a la 6 de una en una, 5 veces
void io_control_task(void *arg)
{
    const uint32_t pins[] = OUTPUT_PINS;  // Pines de salida 1 a 6
    const size_t num_pins = sizeof(pins) / sizeof(pins[0]);

    for (int cycle = 0; cycle < 5; cycle++) {
        ESP_LOGI(TAG, "Ciclo %d de 5", cycle + 1);

        // Encender las salidas de una en una
        for (size_t i = 0; i < num_pins; i++) {
            ESP_LOGI(TAG, "Encendiendo salida %d", i + 1);
            esp_io_expander_set_level(io_expander, pins[i], 0);  // Encender salida actual
            vTaskDelay(pdMS_TO_TICKS(1000));                     // Espera 1 segundo
        }

        // Apagar las salidas de una en una
        for (size_t i = 0; i < num_pins; i++) {
            ESP_LOGI(TAG, "Apagando salida %d", i + 1);
            esp_io_expander_set_level(io_expander, pins[i], 1);  // Apagar salida actual
            vTaskDelay(pdMS_TO_TICKS(1000));                     // Espera 1 segundo
        }
    }

    // Apagar todas las salidas al finalizar los ciclos
    for (size_t i = 0; i < num_pins; i++) {
        esp_io_expander_set_level(io_expander, pins[i], 1);  // Asegurar que todas las salidas queden apagadas
    }

    ESP_LOGI(TAG, "Proceso completado. Todas las salidas están apagadas.");
    vTaskDelete(NULL);  // Finaliza la tarea
}

void app_main(void)
{
    // Inicializa el bus I2C y la expansora de IO
    i2c_bus_init();
    io_expander_init();

    // Crea la tarea de control de salidas
    xTaskCreate(io_control_task, "io_control_task", 2048, NULL, 5, NULL);
}
