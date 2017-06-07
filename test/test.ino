
#include <EEPROM.h>


void save_entry(int pos, char* text) {
    // writes 16 bytes of text to position pos*16.
    pos *= 16;
    for (int i = 0; i < 16; ++i) {
        EEPROM.write(pos + i, text[i]);
    }
}


void read_entry(int pos, char* text) {
    // read 16 bytes of text from position pos*16.
    pos *= 16;
    for (int i = 0; i < 16; ++i) {
        text[i] = EEPROM.read(pos + i);
    }
    text[15] = 0;
}


void setup() {
    Serial.begin(9600);

    if (0) {
        char cnt = 2;
        EEPROM.write(0, cnt);
        char* text1 = "JordiPetit     ";
        char* text2 = "ArnauPetit     ";
        Serial.println("write");
        save_entry(1, text1);
        save_entry(2, text2);
        Serial.println("done");
    }

    if (1) {
        Serial.println("read");
        int cnt = int(EEPROM.read(0));
        Serial.println(cnt);
        for (int i=1; i<=cnt; ++i) {
            char text[16];
            read_entry(i, text);
            Serial.println(text);
        }
        Serial.println("done");
    }
}


void loop() {
}


