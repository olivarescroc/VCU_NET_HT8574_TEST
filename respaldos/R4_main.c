//al R3 se le agrega lectura de entradas, 
//I (978355) ht8574_control: Estado de las entradas: 0xff
//si una entrada es puesta a gnd se activa y pasa a 0

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

// Configuración para el ESP32 con dos expanders HT8574
#define I2C_MASTER_SCL_IO 15                                      /*!< GPIO para I2C master clock */
#define I2C_MASTER_SDA_IO 4                                       /*!< GPIO para I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0                                  /*!< Número del puerto I2C */
#define I2C_ADDRESS_OUTPUT ESP_IO_EXPANDER_I2C_HT8574_ADDRESS_000 /*!< Dirección I2C del expansor de salida */
#define I2C_ADDRESS_INPUT ESP_IO_EXPANDER_I2C_HT8574_ADDRESS_001  /*!< Dirección I2C del expansor de entrada */

// Pines de salida 1 a 6 (mapeados según el expansor de salida)
#define OUTPUT_PINS {IO_EXPANDER_PIN_NUM_0, IO_EXPANDER_PIN_NUM_1, IO_EXPANDER_PIN_NUM_2, \
                     IO_EXPANDER_PIN_NUM_3, IO_EXPANDER_PIN_NUM_4, IO_EXPANDER_PIN_NUM_5}

static const char *TAG = "ht8574_control";
static esp_io_expander_handle_t io_expander_output = NULL;
static esp_io_expander_handle_t io_expander_input = NULL;
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
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error en la instalación del bus I2C");
        return;
    }
}

// Inicialización de la expansora de IO en la dirección de salida
static void io_expander_output_init(void)
{
    esp_err_t ret = esp_io_expander_new_i2c_ht8574(i2c_handle, I2C_ADDRESS_OUTPUT, &io_expander_output);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al crear la expansora de salida HT8574");
        return;
    }

    // Configurar los pines 1 a 6 como salidas
    uint32_t output_pins_mask = IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2 |
                                IO_EXPANDER_PIN_NUM_3 | IO_EXPANDER_PIN_NUM_4 | IO_EXPANDER_PIN_NUM_5;
    ret = esp_io_expander_set_dir(io_expander_output, output_pins_mask, IO_EXPANDER_OUTPUT);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al configurar los pines de salida");
    }
}

// Inicialización de la expansora de IO en la dirección de entrada
static void io_expander_input_init(void)
{
    esp_err_t ret = esp_io_expander_new_i2c_ht8574(i2c_handle, I2C_ADDRESS_INPUT, &io_expander_input);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al crear la expansora de entrada HT8574");
        return;
    }

    // Configurar todos los pines como entradas
    ret = esp_io_expander_set_dir(io_expander_input, 0xFF, IO_EXPANDER_INPUT);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error al configurar los pines de entrada");
    }
}

// Tarea para monitorear el estado de las entradas
void io_input_monitor_task(void *arg)
{
    uint32_t input_state = 0;

    while (true)
    {
        // Leer el estado de todas las entradas
        esp_err_t ret = esp_io_expander_get_level(io_expander_input, 0xFF, &input_state);
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "Estado de las entradas: 0x%02lx", (unsigned long)input_state);
        }
        else
        {
            ESP_LOGE(TAG, "Error al leer el estado de las entradas");
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Monitoreo cada segundo
    }
}

// Tarea principal para encender y apagar las salidas de la 1 a la 6 de una en una, 5 veces
void io_control_task(void *arg)
{
    const uint32_t pins[] = OUTPUT_PINS; // Pines de salida 1 a 6
    const size_t num_pins = sizeof(pins) / sizeof(pins[0]);

    for (int cycle = 0; cycle < 5; cycle++)
    {
        ESP_LOGI(TAG, "Ciclo %d de 5", cycle + 1);

        // Encender las salidas de una en una
        for (size_t i = 0; i < num_pins; i++)
        {
            ESP_LOGI(TAG, "Encendiendo salida %d", i + 1);
            esp_io_expander_set_level(io_expander_output, pins[i], 0); // Encender salida actual
            vTaskDelay(pdMS_TO_TICKS(1000));                           // Espera 1 segundo
        }

        // Apagar las salidas de una en una
        for (size_t i = 0; i < num_pins; i++)
        {
            ESP_LOGI(TAG, "Apagando salida %d", i + 1);
            esp_io_expander_set_level(io_expander_output, pins[i], 1); // Apagar salida actual
            vTaskDelay(pdMS_TO_TICKS(1000));                           // Espera 1 segundo
        }
    }

    // Apagar todas las salidas al finalizar los ciclos
    for (size_t i = 0; i < num_pins; i++)
    {
        esp_io_expander_set_level(io_expander_output, pins[i], 1); // Asegurar que todas las salidas queden apagadas
    }

    ESP_LOGI(TAG, "Proceso completado. Todas las salidas están apagadas.");
    vTaskDelete(NULL); // Finaliza la tarea
}

void app_main(void)
{
    // Inicializa el bus I2C y ambas expansoras de IO
    i2c_bus_init();
    io_expander_output_init();
    io_expander_input_init();

    // Crea la tarea de control de salidas
    xTaskCreate(io_control_task, "io_control_task", 2048, NULL, 5, NULL);

    // Crea la tarea de monitoreo de entradas
    xTaskCreate(io_input_monitor_task, "io_input_monitor_task", 2048, NULL, 5, NULL);
}
