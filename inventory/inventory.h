#ifndef INVENTORY_H
#define INVENTORY_H

#include "inventory.h"
#include "mfrc522.h"
#include "pico_keypad4x4.h"
#include "lcd_1602_i2c.h"
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

#include <stdbool.h>



// Declaraciones de las funciones
void setup();
void admin_card_authentication();
uint get_password();
void gpio_callback(uint gpio, uint32_t events);
void send_to_lcd(char *message[], int num_messages);
bool is_fully_set(char *pin);
void readMaxProductsFromSector2Block2();
const char* getProductName(int productCode);
void readProductInfoFromSector4Block2();
void load_to_flash(int type, int value, bool add);
void read_all_types();
void button_handler();
void reset_inventory();
bool select_mode();
void int_to_string(int num, char *str);
void send_capacity_to_lcd();
void check_inventory();



#define MAX_LINES 2
#define MAX_CHARS 16
#define BUTTON_PIN 28
#define RST_PIN         9
#define SS_PIN          10

//#define FLASH_SECTOR_SIZE 4096
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - 5 * FLASH_SECTOR_SIZE)

#define FLASH_TARGET_OFFSET_TYPE1 (PICO_FLASH_SIZE_BYTES - 5 * FLASH_SECTOR_SIZE)
#define FLASH_TARGET_OFFSET_TYPE2 (PICO_FLASH_SIZE_BYTES - 4 * FLASH_SECTOR_SIZE)
#define FLASH_TARGET_OFFSET_TYPE3 (PICO_FLASH_SIZE_BYTES - 3 * FLASH_SECTOR_SIZE)
#define FLASH_TARGET_OFFSET_TYPE4 (PICO_FLASH_SIZE_BYTES - 2 * FLASH_SECTOR_SIZE)
#define FLASH_TARGET_OFFSET_TYPE5 (PICO_FLASH_SIZE_BYTES - 1 * FLASH_SECTOR_SIZE)

#define TYPE_OFFSET(type) (FLASH_TARGET_OFFSET + (type - 1) * FLASH_SECTOR_SIZE)

#define FLASH_BASE_ADDR 0x10000000  // Flash memory base address

char str[16]; //USED FOR STR TO INT CONVERSION


MFRC522Ptr_t mfrc522;



uint columns[4] = {10, 11, 12, 13};
uint rows[4] = {6, 7, 8, 9};
char matrix[16] = {
    '1', '2', '3', 'A',
    '4', '5', '6', 'B',
    '7', '8', '9', 'C',
    '*', '0', '#', 'D'
};

volatile char last_key = '\0';
volatile bool key_pressed = false;
uint password = 0;
char security_pin[5] = "****"; // 4 characters + null terminator
bool logged = false;

// Max amount of boxes that can be saved inside the warehouse (per type of product)
int max_prod_1 = 5;
int max_prod_2 = 1;
int max_prod_3 = 3;
int max_prod_4 = 2;
int max_prod_5 = 4;

// Define global variables to store the total values
int total_1 = 0;
int total_2 = 0;
int total_3 = 0;
int total_4 = 0;
int total_5 = 0;

static char *message[] = {
    "Bienvenido al", "sistema",
    "de inventario", " :) ",
};

/**
 * Set up function, similar to those in Arduino. Initializes spi, the MFRC522 and PINS.
 *
 */
void setup() {
    spi_init(spi0, 1000 * 1000);
    gpio_set_function(SS_PIN, GPIO_FUNC_SPI);
    gpio_set_function(18, GPIO_FUNC_SPI); // SCK
    gpio_set_function(19, GPIO_FUNC_SPI); // MOSI
    gpio_set_function(16, GPIO_FUNC_SPI); // MISO

    gpio_init(RST_PIN);
    gpio_set_dir(RST_PIN, GPIO_OUT);
    gpio_put(RST_PIN, 1);

    mfrc522 = MFRC522_Init();
    PCD_Init(mfrc522, spi0);
}

/**
 * Read the card provide by the user and check if the CARD is valid or if it belongs to the ADMINS by checking the UID in the card.
 * Beware: The Admin UID is programmed here, after reading the RFID card you selected to be ADMIN CARD.
 */
void admin_card_authentication() {
    // Declare card UID's
    uint8_t tag1[] = {0xBA, 0x78, 0x82, 0x77};
    // Declare authentication variables
    bool authorized_card = false; // True if the RFID authentication card matches its UID. False in other case.
    uint password = 2001;         // Admin password. If the introduces value matches this password AND authorized card is TRUE, Autherized returns TRUE.

    //MFRC522Ptr_t mfrc = MFRC522_Init();
    //PCD_Init(mfrc522, spi0);

    sleep_ms(500);
    while (!authorized_card) {
        // Wait for new card
        printf("\nWaiting for card\n\r");
        while (!PICC_IsNewCardPresent(mfrc522));
        // Select the card
        printf("Selecting card\n\r");
        PICC_ReadCardSerial(mfrc522);

        // Show UID on serial monitor
        printf("PICC dump: \n\r");
        PICC_DumpToSerial(mfrc522, &(mfrc522->uid));

        // Authorization with uid
        printf("Uid is: ");
        for (int i = 0; i < 4; i++) {
            printf("%x ", mfrc522->uid.uidByte[i]);
        }
        printf("\n\r");

        if (memcmp(mfrc522->uid.uidByte, tag1, 4) == 0) {
            printf("Authentication Success\n\r");
            authorized_card = true;
        } else {
            printf("Authentication Failed\n\r");
            authorized_card = false;
        }
    }
}

/**
 * Temporary function to get a testing password. Not very usefull on the final implementation
 * @return password, which is 2001 in this case.
 */
uint get_password() {
    uint password = 2001; // Instead of this, get password by keypad handling.
    return password;
}


/**
 * This function is the ISR related to the Keypad. While IRQ are enabled, program will jump here to get the key that was pressed in the 4X4 keypad. Includes debouncing mechanism and feedback by serial monitor.
 * 
 */
void gpio_callback(uint gpio, uint32_t events) {
    if (!debounce_time_passed()) {
        return;
    }

    char key = pico_keypad_get_key();
    if (key != '\0') {
        printf("Key pressed: %c\n", key);
        last_key = key;
        key_pressed = true;
        last_press_time = get_absolute_time();
    }
}

/**
 * As the name implies, the function receive the message (a string or pointer of char array) and the number of messages to be displayed on the LCD. Uses the lcd_1602_i2c.h library
 */
void send_to_lcd(char *message[], int num_messages) {
    for (int m = 0; m < num_messages; m += MAX_LINES) {
        for (int line = 0; line < MAX_LINES; line++) {
            lcd_set_cursor(line, (MAX_CHARS / 2) - strlen(message[m + line]) / 2);
            lcd_string(message[m + line]);
        }
        sleep_ms(2000);
        lcd_clear();
    }
}


/**
 * Checks if the array that stores the password is full of characters different from '*'. It is fully set when EVERY CHARACTER in the password array is DIFFERENT from a '*' 
 * @return true if password does not contain '*'. 
 */
bool is_fully_set(char *pin) {
    for (int i = 0; i < 4; i++) {
        if (pin[i] == '*') {
            return false;
        }
    }
    return true;
}

/**
 * Read sector 2, block 2 of the approached RFID tag. In this position is where the max capacity of the warehouse is stored. It checks if the UID corresponds to the UID of MAX CAPACITY CARD and if so,
 * proceeds to extract and save that information into variables to further use. 
 */
void readMaxProductsFromSector2Block2() {
//MFRC522Ptr_t mfrc522;




    printf("\nGOT INSIDE THE FUNCTION\n");


    // Define the UID of the valid card
    uint8_t validUID[] = {0x85, 0x28, 0xEE, 0xC7};
    MIFARE_Key key;
    for (uint8_t i = 0; i < 6; i++) key.keybyte[i] = 0xFF; // Default key

    bool validCardDetected = false;

    while (!validCardDetected) {
        // Wait for a new card to be present
        printf("Waiting for card\n");
        while (!PICC_IsNewCardPresent(mfrc522));

        // Select the card
        printf("Card detected, selecting card\n");
        if (PICC_ReadCardSerial(mfrc522)) {
            printf("Card selected\n");

            // Check if the card's UID matches the valid UID
            if (memcmp(mfrc522->uid.uidByte, validUID, 4) == 0) {
                printf("Valid card detected\n");

                // Authenticate sector 2, block 2
                printf("\nPASSED THE 1ST FOR LOOP\n");
                if (PCD_Authenticate(mfrc522, PICC_CMD_MF_AUTH_KEY_A, 2 * 4 + 2, &key, &(mfrc522->uid)) == STATUS_OK) {
                    uint8_t buffer[18];
                    uint8_t bufferSize = sizeof(buffer);

                    printf("\nGOT INSIDE 1ST IF\n");

                    // Read data from sector 2, block 2
                    if (MIFARE_Read(mfrc522, 2 * 4 + 2, buffer, &bufferSize) == STATUS_OK) {
                        printf("\nGOT INSIDE 2ND IF\n");
                        printf("32 Nibbles: ");
                        for (int i = 0; i < 16; i++) {
                            printf("%02X ", buffer[i]);
                        }
                        printf("\n");

                        // Extract the 12-bit quantities for each product type from the buffer
                        int maxProducts[5];
                        maxProducts[0] = ((buffer[8] << 8) | buffer[9]) & 0xFFF;   // Type 1
                        maxProducts[1] = ((buffer[10] << 4) | (buffer[11] >> 4)) & 0xFFF; // Type 2
                        maxProducts[2] = ((buffer[11] << 8) | buffer[12]) & 0xFFF; // Type 3
                        maxProducts[3] = ((buffer[13] << 4) | (buffer[14] >> 4)) & 0xFFF; // Type 4
                        maxProducts[4] = ((buffer[14] << 8) | buffer[15]) & 0xFFF; // Type 5

                        printf("Max Products:\n");
                        for (int i = 0; i < 5; i++) {
                            printf("Type %d: %d\n", i + 1, maxProducts[i]);
                        }

                        // Update global max_prod variables
                        max_prod_1 = maxProducts[0];
                        max_prod_2 = maxProducts[1];
                        max_prod_3 = maxProducts[2];
                        max_prod_4 = maxProducts[3];
                        max_prod_5 = maxProducts[4];

                        validCardDetected = true; // Successfully read the valid card and extracted the data
                    } else {
                        printf("Error reading the block.\n");
                    }
                } else {
                    printf("Error authenticating the block.\n");
                }

                
            } else {
                printf("Invalid card detected\n");
            }

        PICC_HaltA(mfrc522); // Halt the PICC

        } else {
            printf("Failed to select card\n");
        }
    }
    PICC_HaltA(mfrc522); // Halt the PICC
    printf("\nFINISHED\n");
    sleep_ms(500);
    // Add return statement to exit the function
    return;
}

/**
 * Depending on the type of product extracted, it assigns a product name (change "Producto 1" for "Rice" or  "Bread" as an example)
 *
 * @return Name of the product, corresponding to its type
 */
const char* getProductName(int productCode) {
    switch (productCode) {
        case 1:
            return "Producto 1";
        case 2:
            return "Producto 2";
        case 3:
            return "Producto 3";
        case 4:
            return "Producto 4";
        case 5:
            return "Producto 5";
        default:
            return "Producto Desconocido";
    }
}

/**
 * Read the information stored in the Sector 4, block 2, of the RFID TAGS. In that position is where Type of product, Quantity, Price per Unit and Selling price are stored. 
 * Once this data is extracte, is formated and stored in variables for further handling. Depending on the operation, Quantity could be stored in flash memory.
 */
void readProductInfoFromSector4Block2() {
    printf("\nGOT INSIDE THE FUNCTION\n");


    // Define the UID of the valid card
    uint8_t validUID[] = {0xFF, 0xFF, 0xFF, 0xFF};
    MIFARE_Key key;
    for (uint8_t i = 0; i < 6; i++) key.keybyte[i] = 0xFF; // Default key

    bool validCardDetected = false;

    while (!validCardDetected) {
        PCD_Reset(mfrc522);
        PCD_Init(mfrc522, spi0);
        // Wait for a new card to be present
        message[0] = "ESPERANDO TAG";
        message[1] = "...";
        send_to_lcd(message, 2);
        printf("Waiting for card\n");
        while (!PICC_IsNewCardPresent(mfrc522));

        // Select the card
        printf("Card detected, selecting card\n");
        if (PICC_ReadCardSerial(mfrc522)) {
            printf("Card selected\n");

            // Check if the card's UID matches the valid UID
            if (!memcmp(mfrc522->uid.uidByte, validUID, 4) == 0) {
                printf("Valid card detected\n");

                // Authenticate sector 2, block 2
                printf("\nPASSED THE 1ST FOR LOOP\n");
                if (PCD_Authenticate(mfrc522, PICC_CMD_MF_AUTH_KEY_A, 4 * 4 + 2, &key, &(mfrc522->uid)) == STATUS_OK) {
                    uint8_t buffer[18];
                    uint8_t bufferSize = sizeof(buffer);

                    printf("\nGOT INSIDE 1ST IF\n");

                    // Read data from sector 4, block 2
                    if (MIFARE_Read(mfrc522, 4 * 4 + 2, buffer, &bufferSize) == STATUS_OK) {
                        printf("32 Nibbles: ");
                        for (int i = 0; i < 16; i++) {
                            printf("%02X ", buffer[i]);
                        }
                        printf("\n");

                        // Extract product type, quantity, price, and selling price from nibbles
                        uint8_t productType = buffer[15] & 0x0F;
                        uint16_t quantity = (((buffer[14]) << 8) | (buffer[15] & 0xF0)) >> 4;
                        uint16_t price = (buffer[12] << 8) | (buffer[13]);
                        uint16_t sellingPrice = (buffer[10] << 8) | (buffer[11]);

                        // Convert the product type to a product name
                        const char *nombreProducto = getProductName(productType);

                        printf("Product Information:\n");
                        printf("Tipo de Producto: %s\n", nombreProducto);
                        printf("Cantidad de Producto: %d\n", quantity);
                        printf("Precio Unitario: %d\n", price);
                        printf("Precio de Venta: %d\n", sellingPrice);

                        //GET OPERATION TYPE/////////////////////////////////////////////////////////////////////////////////////////

                        bool mode = select_mode();
                        read_all_types();
                        load_to_flash(productType, quantity, mode);
                        printf("\nFINISHED KEYBOARD\n");
                        read_all_types();
                    } else {
                        printf("Error leyendo el bloque.\n");
                    }
                } else {
                    printf("Error authenticating the block.\n");
                }

                PICC_HaltA(mfrc522); // Halt the PICC

            } else {
                printf("Invalid card detected\n");
            }

            PICC_HaltA(mfrc522); // Halt the PICC

        } else {
            printf("Failed to select card\n");
        }
    }
    printf("\nFINISHED\n");
    sleep_ms(500);
    // Add return statement to exit the function
    return;
}


/**
 * As the name implies, it saves the information (quantity) in the corresponding type of product (there is a flash mem sector for each type). Savings are addition or substraction, and are bounded between 0 and max capacity given by the MAX CAPACITY CARD.
 */
void load_to_flash(int type, int value, bool add) {
    uint32_t flash_offset;
    int max_amount;
    
    // Get the maximum amount for the given type
    switch (type) {
        case 1:
            max_amount = max_prod_1;
            flash_offset = FLASH_TARGET_OFFSET_TYPE1;
            break;
        case 2:
            max_amount = max_prod_2;
            flash_offset = FLASH_TARGET_OFFSET_TYPE2;
            break;
        case 3:
            max_amount = max_prod_3;
            flash_offset = FLASH_TARGET_OFFSET_TYPE3;
            break;
        case 4:
            max_amount = max_prod_4;
            flash_offset = FLASH_TARGET_OFFSET_TYPE4;
            break;
        case 5:
            max_amount = max_prod_5;
            flash_offset = FLASH_TARGET_OFFSET_TYPE5;
            break;
        default:
            printf("Invalid type\n");
            return;
    }

    // Read the current value from flash
    uint32_t current_value;
    memcpy(&current_value, (const uint8_t *)(XIP_BASE + flash_offset), sizeof(current_value));

    if (current_value == 0xFFFFFFFF) {
        // If flash is uninitialized, set current_value to 0
        current_value = 0;
    }

    // Calculate the new value based on the operation
    if (add) {
        current_value += value;
        message[0] = "INGRESASTE:";
        int_to_string(value,str);
        message[1] = str;
        send_to_lcd(message, 2);
    } else {
        
        // Ensure value doesn't go negative
        if (value > current_value) {
            current_value = 0;
            /*message[0] = "RETIRASTE";
            int_to_string(current_value-value,str);
            message[1] = str;
            send_to_lcd(message, 2);*/
        } else {
            current_value -= value;
            message[0] = "RETIRASTE:";
            int_to_string(value,str);
            message[1] = str;
            send_to_lcd(message, 2);
        }
    }

    // Ensure the value doesn't exceed the maximum amount
    if (current_value > max_amount) {
        current_value = max_amount;
    }

    // Prepare the buffer to write to flash
    uint32_t buf[FLASH_PAGE_SIZE / sizeof(uint32_t)] = {0}; // Initialize buffer to 0
    buf[0] = current_value;

    // Erase the flash sector before writing
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(flash_offset, FLASH_SECTOR_SIZE);
    flash_range_program(flash_offset, (const uint8_t *)buf, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
}

/**
 * Read the information stored in the flash memory. This is, the quantity of each product.
 */
void read_all_types() {
    uint32_t flash_offsets[] = {
        FLASH_TARGET_OFFSET_TYPE1,
        FLASH_TARGET_OFFSET_TYPE2,
        FLASH_TARGET_OFFSET_TYPE3,
        FLASH_TARGET_OFFSET_TYPE4,
        FLASH_TARGET_OFFSET_TYPE5
    };

    // Iterate through each type and read the value from flash
    for (int type = 0; type < 5; type++) {
        uint32_t current_value;
        memcpy(&current_value, (const uint8_t *)(XIP_BASE + flash_offsets[type]), sizeof(current_value));

        // Store the value in the corresponding global variable
        switch(type) {
            case 0:
                total_1 = (current_value == 0xFFFFFFFF) ? 0 : current_value;
                printf("Type%d: %d\n", type + 1, total_1);
                break;
            case 1:
                total_2 = (current_value == 0xFFFFFFFF) ? 0 : current_value;
                printf("Type%d: %d\n", type + 1, total_2);
                break;
            case 2:
                total_3 = (current_value == 0xFFFFFFFF) ? 0 : current_value;
                printf("Type%d: %d\n", type + 1, total_3);
                break;
            case 3:
                total_4 = (current_value == 0xFFFFFFFF) ? 0 : current_value;
                printf("Type%d: %d\n", type + 1, total_4);
                break;
            case 4:
                total_5 = (current_value == 0xFFFFFFFF) ? 0 : current_value;
                printf("Type%d: %d\n", type + 1, total_5);
                break;
            default:
                break;
        }
    }
}
/**
 * Resets the inventory. each type of product would be empty, zero quantity
 */
// Function to reset all inventory values to 0
void reset_inventory() {
    int max_prod_1_copy = max_prod_1;
    int max_prod_2_copy = max_prod_2;
    int max_prod_3_copy = max_prod_3;
    int max_prod_4_copy = max_prod_4;
    int max_prod_5_copy = max_prod_5;

    max_prod_1 = 0;
    max_prod_2 = 0;
    max_prod_3 = 0;
    max_prod_4 = 0;
    max_prod_5 = 0;

    // Reset the values in flash memory
    load_to_flash(1, 0, false);
    load_to_flash(2, 0, false);
    load_to_flash(3, 0, false);
    load_to_flash(4, 0, false);
    load_to_flash(5, 0, false);

    max_prod_1 = max_prod_1_copy;
    max_prod_2 = max_prod_2_copy;
    max_prod_3 = max_prod_3_copy;
    max_prod_4 = max_prod_4_copy;
    max_prod_5 = max_prod_5_copy;

    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, false, &button_handler);
    message[0] = "INVENTARIO VACIO";
    message[1] = " :) ";
    send_to_lcd(message, 2);

}

void button_handler() {
    // Call the function to reset the inventory
    printf("\nRESETING INVENTORY\n");
    reset_inventory();
}

/**
 * Selection of mode. This function asks via keypad if you want to add or substract from inventory, or if you want to check the current inventory or if you want to reset it. 
 * A to add, B to sobtract, C to check inventory and D to reset inventory.
 * @return mode ()
 */
bool select_mode() {
    bool mode = true;
    // Setup GPIO interrupts for the keypad columns
    pico_keypad_irq_enable(true, gpio_callback);

    message[0] = "INGRESAR: A";
    message[1] = "RETIRAR: B";

    message[2] = "CHECAR INVENT: C";
    message[3] = "VACIAR INVENT: D";
    send_to_lcd(message, 4);

    int index = 0;
    while (true) {
        // Wait until a key is pressed
        while (!key_pressed) {}

        // Reset key_pressed flag
        key_pressed = false;

        // Check if the pressed key is A or B
        if (last_key == 'A') {
            mode = true;
            break;  // Exit the loop if A is pressed
        } else if (last_key == 'B') {
            mode = false;
            break;  // Exit the loop if B is pressed
        } else if (last_key == 'D'){
            printf("\nRESET DE INVENTARIO\n");
            reset_inventory();
            mode = false;
            break;
        } else if (last_key == 'C'){
            printf("\nCHECKING INVENTORY\n");
            check_inventory();
            //mode = false;
            break;
        }
        // Do nothing if any other key is pressed
    }

    // Setup GPIO interrupts for the keypad columns
    pico_keypad_irq_enable(false, gpio_callback);
    
    return mode;
}
/**
 * Convert a integer into a string. Needed when you want to load integers into lcd display
 */
void int_to_string(int num, char *str) {
    sprintf(str, "%d", num);
}

/**
 * Sends messages to lcd, which show the capacity of the warehouse
 */
void send_capacity_to_lcd(){
    message[0] = "CAPACIDAD DE";
    message[1] = "BODEGA:";
    send_to_lcd(message, 2);

    message[0] = "TIPO 1: ";
    int_to_string(max_prod_1,str);
    message[1] = str;
    send_to_lcd(message, 2);

    message[0] = "TIPO 2: ";
    int_to_string(max_prod_2,str);
    message[1] = str;
    send_to_lcd(message, 2);

    message[0] = "TIPO 3: ";
    int_to_string(max_prod_3,str);
    message[1] = str;
    send_to_lcd(message, 2);

    message[0] = "TIPO 4: ";
    int_to_string(max_prod_4,str);
    message[1] = str;
    send_to_lcd(message, 2);

    message[0] = "TIPO 5: ";
    int_to_string(max_prod_5,str);
    message[1] = str;
    send_to_lcd(message, 2);
}

/**
 * shows the current inventory into the display
 */

void check_inventory(){
    message[0] = "CANTIDAD ACTUAL";
    message[1] = "EN BODEGA:";
    send_to_lcd(message, 2);

    message[0] = "TIPO 1: ";
    int_to_string(total_1,str);
    message[1] = str;
    send_to_lcd(message, 2);

    message[0] = "TIPO 2: ";
    int_to_string(total_2,str);
    message[1] = str;
    send_to_lcd(message, 2);

    message[0] = "TIPO 3: ";
    int_to_string(total_3,str);
    message[1] = str;
    send_to_lcd(message, 2);

    message[0] = "TIPO 4: ";
    int_to_string(total_4,str);
    message[1] = str;
    send_to_lcd(message, 2);

    message[0] = "TIPO 5: ";
    int_to_string(total_5,str);
    message[1] = str;
    send_to_lcd(message, 2);
}



#endif // INVENTORY_H