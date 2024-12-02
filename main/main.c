// se modifica logs seriales y se dejan globales las variables de I/O para dejarlas accesibles para otras tareas
//estados de salidas y entradas.
// uint32_t input_state = 0;
// uint32_t output_state = 0;

//LOGS
// I (15755) HT8574: Out: 111111
// I (15855) HT8574: In: 000000
// I (15855) HT8574: Out: 111111
// I (15955) HT8574: In: 000000
// I (15955) HT8574: Out: 111111
// I (16055) HT8574: In: 000000

/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_io_expander_ht8574.h"

// Configuración para el ESP32 con dos expanders HT8574
#define I2C_MASTER_SCL_IO 15                                      /*!< GPIO para I2C master clock */
#define I2C_MASTER_SDA_IO 4                                       /*!< GPIO para I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0                                  /*!< Número del puerto I2C */

#define I2C_ADDRESS_INPUT ESP_IO_EXPANDER_I2C_HT8574_ADDRESS_010  /*!< Dirección I2C del expansor de entrada 0x22*/
#define I2C_ADDRESS_OUTPUT ESP_IO_EXPANDER_I2C_HT8574_ADDRESS_100 /*!< Dirección I2C del expansor de salida 0x24*/

// Pines de salida 1 a 6 (mapeados según el expansor de salida)
#define OUTPUT_PINS {IO_EXPANDER_PIN_NUM_0, IO_EXPANDER_PIN_NUM_1, IO_EXPANDER_PIN_NUM_2, \
                     IO_EXPANDER_PIN_NUM_3, IO_EXPANDER_PIN_NUM_4, IO_EXPANDER_PIN_NUM_5}

#define TAG_HT8574 "HT8574"
#define REGENERATION_TORQUE 10

//estados de salidas y entradas.
uint32_t input_state = 0;
uint32_t output_state = 0;


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
        ESP_LOGE(TAG_HT8574, "Error en la instalación del bus I2C");
        return;
    }
}

// Inicialización de la expansora de IO en la dirección de salida
static void io_expander_output_init(void)
{
    esp_err_t ret = esp_io_expander_new_i2c_ht8574(i2c_handle, I2C_ADDRESS_OUTPUT, &io_expander_output);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_HT8574, "Error al crear la expansora de salida HT8574");
        return;
    }

    // Configurar los pines 1 a 6 como salidas
    uint32_t output_pins_mask = IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2 |
                                IO_EXPANDER_PIN_NUM_3 | IO_EXPANDER_PIN_NUM_4 | IO_EXPANDER_PIN_NUM_5;
    ret = esp_io_expander_set_dir(io_expander_output, output_pins_mask, IO_EXPANDER_OUTPUT);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_HT8574, "Error al configurar los pines de salida");
    }
}

// Inicialización de la expansora de IO en la dirección de entrada
static void io_expander_input_init(void)
{
    esp_err_t ret = esp_io_expander_new_i2c_ht8574(i2c_handle, I2C_ADDRESS_INPUT, &io_expander_input);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_HT8574, "Error al crear la expansora de entrada HT8574");
        return;
    }

    // Configurar todos los pines como entradas
    ret = esp_io_expander_set_dir(io_expander_input, 0xFF, IO_EXPANDER_INPUT);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_HT8574, "Error al configurar los pines de entrada");
    }
}

// Tarea para monitorear el estado de las entradas y salidas
// Tarea para monitorear el estado de las entradas y salidas
void io_input_monitor_task(void *arg)
{
    // uint32_t input_state = 0;
    // uint32_t output_state = 0;

    while (true)
    {
        // Leer el estado de todas las entradas
        esp_err_t ret = esp_io_expander_get_level(io_expander_input, 0xFF, &input_state);
        if (ret == ESP_OK)
        {
            // Formatear el estado de las entradas en binario
            char input_binary[7];
            for (int i = 0; i < 6; i++)
            {
                input_binary[5 - i] = ((input_state >> i) & 0x1) ? '0' : '1';
            }
            input_binary[6] = '\0'; // Agregar el carácter nulo al final
            ESP_LOGI(TAG_HT8574, "In: %s", input_binary);
        }
        else
        {
            ESP_LOGE(TAG_HT8574, "Error al leer el estado de las entradas");
        }

        // Leer el estado de todas las salidas
        ret = esp_io_expander_get_level(io_expander_output, 0xFF, &output_state);
        if (ret == ESP_OK)
        {
            // Formatear el estado de las salidas en binario
            char output_binary[7];
            for (int i = 0; i < 6; i++)
            {
                output_binary[5 - i] = ((output_state >> i) & 0x1) ? '0' : '1';
            }
            output_binary[6] = '\0'; // Agregar el carácter nulo al final
            ESP_LOGI(TAG_HT8574, "Out: %s", output_binary);
        }
        else
        {
            ESP_LOGE(TAG_HT8574, "Error al leer el estado de las salidas");
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Monitoreo cada segundo
    }
}


// Tarea principal para encender y apagar las salidas de la 1 a la 6 de una en una, 1 vez
void io_control_task(void *arg)
{
    const uint32_t pins[] = OUTPUT_PINS; // Pines de salida 1 a 6
    const size_t num_pins = sizeof(pins) / sizeof(pins[0]);

    for (int cycle = 0; cycle < 1; cycle++)  // Ciclo único
    {
        ESP_LOGI(TAG_HT8574, "Ciclo %d de %d", cycle + 1, 1);

        // Encender las salidas de una en una
        for (size_t i = 0; i < num_pins; i++)
        {
            ESP_LOGI(TAG_HT8574, "Encendiendo salida %d", i + 1);
            esp_io_expander_set_level(io_expander_output, pins[i], 0); // Encender salida actual
            vTaskDelay(pdMS_TO_TICKS(1000));                           // Espera 1 segundo
        }

        // Apagar las salidas de una en una
        // for (size_t i = 0; i < num_pins; i++)
        // {
        //     ESP_LOGI(TAG_HT8574, "Apagando salida %d", i + 1);
        //     esp_io_expander_set_level(io_expander_output, pins[i], 1); // Apagar salida actual
        //     vTaskDelay(pdMS_TO_TICKS(1000));                           // Espera 1 segundo
        // }
    }

    // Apagar todas las salidas al finalizar el ciclo
    // for (size_t i = 0; i < num_pins; i++)
    // {
    //     esp_io_expander_set_level(io_expander_output, pins[i], 1); // Asegurar que todas las salidas queden apagadas
    // }

    ESP_LOGI(TAG_HT8574, "Proceso completado. Todas las salidas están apagadas.");

    // Crear la tarea de monitoreo de entradas y salidas después de que el ciclo de control de salidas ha terminado
    xTaskCreate(io_input_monitor_task, "io_input_monitor_task", 2048, NULL, 5, NULL);

    vTaskDelete(NULL); // Finaliza la tarea de control de salidas
}

void app_main(void)
{
    // Inicializa el bus I2C y ambas expansoras de IO
    i2c_bus_init();
    io_expander_output_init();
    io_expander_input_init();

    // Crea la tarea de control de salidas
    xTaskCreate(io_control_task, "io_control_task", 2048, NULL, 5, NULL);
}
