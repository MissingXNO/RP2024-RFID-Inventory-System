/**
 * @file inventory.c
 *
 * @mainpage Inventory Manager
 *
 * @section description Description
 * This program turns the Raspberry Pi Pico W (RP2040) into a inventory management system.
 * The code includes support to input peripherals such as a 4x4 Keypad and MFRC522 RFID Module, and
 * output peripherals such as I2C LCD 16x2 display.
 * The system starts in an idle state where it will ask the user to introduce an RFID card called ADMIN card.
 * Only card with the proper UID will take the user to the second step of ADMIN verification process.
 * The second stage will ask the user for an ADMIN PIN.
 * Once the correct PIN is introduced, the program will ask for an RFID card that indicates the maximum capacity of the warehouse.
 * When the previous step is finished, the program will show the previously set capacity and then, will wait for boxes to be scanned with the RFID reader
 * The boxes would have an RFID TAG attached, so user will have to scan that tag.
 * Every time a tag is scanned, the program will ask for an operation to be done.
 * press A to insert the box to the inventory, press B to extract it from the inventory. Press C to check the current state of the inventory. Press D to reset the inventory to zero.
 * For operations C and D, there is no need to put a box tag in the reader, you could use any other tag, like capacity card or admin card. But only box tags will change the inventory when A or B are pressed.
 * 
 * During the operation, the LCD will provide feedback so the user wont get lost during its operation.
 * The interaction is also possible due to the 4x4 keypad, which will work with interrupts.
 * Polling + interrupts is the methodology that is being used in the whole program.
 *
 * @section circuit Circuit
 * - Push-button connected to GP16
 * - Keypad Rows connected like    (r1, r2, r3, r4) to (GP18, GP19, GP20, GP21) . Could be changed from here, as long as initialization is this case does NOT required consecutive pins
 * - Keypad columns connected like (c1, c2, c3, c4) to (GP22, GP26, GP27, GP28) . Could be changed from here, as long as initialization is this case does NOT required consecutive pins
 *
 * @section libraries Libraries
 * - math.h (https://pubs.opengroup.org/onlinepubs/009695399/basedefs/math.h.html)
 *   - Useful to perform mathematical operations, necessary to build function equations.
 * - string.h (https://www.ibm.com/docs/es/i/7.5?topic=files-stringh)
 *   - Necessary to process strings such as the input introduced by the Keypad. mainly used to convert strings into float or any numeric representation.
 * - time.h (https://www.ibm.com/docs/es/i/7.5?topic=files-timeh)
 *   - Used to get the current time in order to generate signals with a real-like time variable instead of calculating/generating it
 * - stdbool.h (https://www.ibm.com/docs/es/i/7.5?topic=files-stdboolh)
 *   - Needed to support the use of logical and boolean operators.
 * - stdlib.h (https://www.ibm.com/docs/es/i/7.5?topic=files-stdlibh)
 *   - Standard C library. 
 * - stdio.h (https://www.ibm.com/docs/es/i/7.5?topic=files-stdioh)
 *   - Standard Input/Output library.
 * - hardware/gpio.h (https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#hardware_gpio)
 *   - This library is mandatory to manipulate the General Purpose Input/Ouput ports available in the Raspberry Pi Pico board
 * - pico/stdlib.h (https://www.raspberrypi.com/documentation//pico-sdk/stdlib_8h_source.html)
 *   - Standard library associated to the Pico Board.
 * - mfrc522.h (https://github.com/BenjaminModica/pico-mfrc522)
 *   - adaptation of the Arduino's library, "mfrc522.h" (originally developed by Miguel Balboa:  https://github.com/miguelbalboa/rfid) by Benjamin Modica (Thank you, Ben!)
 * - pico_keypad4x4.h
 *   - Library Previosly developed for the course, for keypad handling.
 * - lcd_1602_i2c.h ()
 * @section notes Notes
 * - This project works implementing polling + interrupt methodology for the peripherals. Keypad, lcd and mfrc
 * - Default values can be changed with testing purposes. 
 *
 * @section todo ToDo
 * - This code and its structure could be further optimized. Functionalities are working fine, however modifications can be made in orden to make the operation of the system wasier and more comfortable
 *
 * @section author Authors
 * - Created by Santiago Giraldo Tabares & Ana María Velasco Montenegro on april, 2024
 *
 *
 * Copyright (c) No-license.
 */

#include "mfrc522.h"
#include "pico_keypad4x4.h"
#include "lcd_1602_i2c.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/spi.h"
#include "hardware/sync.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "inventory.h"


int main() {
    stdio_init_all();
    setup();
    pico_keypad_init(columns, rows, matrix);
    //spi_init(spi0, 1000 * 1000);
    // Initialize the GPIO for the button
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_set_function(BUTTON_PIN, GPIO_FUNC_SIO);  // Set GPIO function
    gpio_pull_up(BUTTON_PIN);  // Enable pull-up resistor

    // Configure interrupt for falling edge
    gpio_set_irq_enabled(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true);

    // Set up the interrupt handler
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &button_handler);
    // Set up the interrupt handler


    #if !defined(i2c_default) || !defined(PICO_DEFAULT_I2C_SDA_PIN) || !defined(PICO_DEFAULT_I2C_SCL_PIN)
    #warning i2c/lcd_1602_i2c example requires a board with I2C pins
    #else
    // This example will use I2C0 on the default SDA and SCL pins (4, 5 on a Pico)
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

    lcd_init();



    sleep_ms(500);
    
    while (!logged) {
        message[0]="Bienvenido al";
        message[1]="sistema";
        message[2]="de inventario";
        message[3]=" :) ";
        printf("\nBienvenido al sistema de inventario");
        send_to_lcd(message, 4);

        message[0] = "INSERTE TARJETA";
        message[1] = "DE ADMINISTRADOR";
        send_to_lcd(message, 2);
        admin_card_authentication();

        

        printf("\nIngrese PIN de seguridad: ");
        message[0] = "Ingrese PIN";
        message[1] = "de seguridad:";
        send_to_lcd(message, 2);

        password = get_password();
        printf("\nPIN secreto: %u", password);

        // Setup GPIO interrupts for the keypad columns
        pico_keypad_irq_enable(true, gpio_callback);

        int index = 0;

        while (!is_fully_set(security_pin)) {
            if (key_pressed) {
                security_pin[index] = last_key;
                index++;
                key_pressed = false;
            }
        }

        if (security_pin[0] == '2' && security_pin[1] == '0' && security_pin[2] == '0' && security_pin[3] == '1') {
            printf("\nPIN correcto!.");
            logged = true;

        } else {
            message[0] = "PIN INCORRECTO:";
            message[1] = security_pin;
            message[2] = "INTENTE DE NUEVO:";
            message[3] = ":(";
            send_to_lcd(message, 4);
            printf("\nINTENTALO DE NUEVO");
            strncpy(security_pin, "****", sizeof(security_pin) - 1);
            security_pin[sizeof(security_pin) - 1] = '\0';
            logged = false;
        }
    }
    message[0] = "Admin autorizado";
    message[1] = "con exito!";
    send_to_lcd(message, 2);
    printf("\n\nAdministrador autenticado con éxito\n");
    printf("\n");

    // Setup GPIO interrupts for the keypad columns
    pico_keypad_irq_enable(false, gpio_callback);

    // Aqui viene la etapa de deteccion de cajas
    message[0] = "Por favor,";
    message[1] = "Indique:";
    message[2] = "Capacidad";
    message[3] = "de la bodega";
    send_to_lcd(message, 4); // "Por favor, indique la capacidad de la bodega"
    readMaxProductsFromSector2Block2();

    //send_to_lcd(message3, sizeof(message3) / sizeof(message3[0]));
    printf("\n\nCapacidad de bodega:\n");
    printf("tipo 1: %u\n", max_prod_1);
    printf("tipo 2: %u\n", max_prod_2);
    printf("tipo 3: %u\n", max_prod_3);
    printf("tipo 4: %u\n", max_prod_4);
    printf("tipo 5: %u\n", max_prod_5);
    printf("\n");
    
    send_capacity_to_lcd();

    //// HERE I SHOULD START READING TAGS AND EXTRACTING THEIR INFO FROM SECTOR 4, BLOCK 2. /////////
    PCD_Reset(mfrc522);
    PCD_Init(mfrc522, spi0);
    readProductInfoFromSector4Block2();
    printf("\nPASSED LAST FUNCTION\n");

    /// HERE GOES THE REST OF THE CODE (NOTHING FOR NOW)
    while (1) {
    }

    #endif
    return 0;
}

