#include <LiquidCrystal.h>
#include <EEPROM.h>

#include "queue.hh"


// ***************************************************************************
// The event queue and its capacity
// ***************************************************************************

Queue<10> q;



// ***************************************************************************
// Direction sticks of joysticks
// ***************************************************************************


struct Joystick {

    const unsigned alts  = 0b1010101010101010;
    const unsigned ones  = 0b1111111111111111;
    const unsigned zeros = 0b0000000000000000;

    int pin_SW, pin_X, pin_Y;
    int delay;
    unsigned shift_SW, shift_XH, shift_XL, shift_YH, shift_YL;

    Joystick(int pin_SW, int pin_X, int pin_Y, int delay=6)
    :   pin_SW(pin_SW), pin_X(pin_X), pin_Y(pin_Y),
        delay(delay),
        shift_XH(alts), shift_XL(alts),
        shift_YH(alts), shift_YL(alts)
    {   }

    static void update(Joystick* joystick) {
        int sSW = 1 - digitalRead(joystick->pin_SW);
        joystick->shift_SW = (joystick->shift_SW << 1) + sSW;

        int vX = 1024 - analogRead(joystick->pin_X);
        int sXH = vX >= 768 ? 1 : 0;
        int sXL = vX <= 256 ? 1 : 0;
        joystick->shift_XH = (joystick->shift_XH << 1) + sXH;
        joystick->shift_XL = (joystick->shift_XL << 1) + sXL;

        int vY = 1024 - analogRead(joystick->pin_Y);
        int sYH = vY >= 768 ? 1 : 0;
        int sYL = vY <= 256 ? 1 : 0;
        joystick->shift_YH = (joystick->shift_YH << 1) + sYH;
        joystick->shift_YL = (joystick->shift_YL << 1) + sYL;

        q.in(joystick->delay, [=] {update(joystick);});
    }

    bool switch_on() {
        return shift_SW == ones;
    }

    bool switch_off() {
        return shift_SW == zeros;
    }

    bool x_high() {
        return shift_XH == ones;
    }

    bool x_low() {
        return shift_XL == ones;
    }

    bool y_high() {
        return shift_YH == ones;
    }

    bool y_low() {
        return shift_YL == ones;
    }

};



// ***************************************************************************
// EPPROM access to save/read logs
// ***************************************************************************


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


// ***************************************************************************
// Constants for the application
// ***************************************************************************


// Pins for the red, yellow and green pins
const int pin_Red       = 2;
const int pin_Yellow    = 3;
const int pin_Green     = 4;

// Pins for the joystick
const int pin_X         = A0;
const int pin_Y         = A1;
const int pin_SW        = 5;

// Pin for the passive buzzer
const int pin_Buzz      = 6;

// Pin for the MQ-7 metane sensor
const int pin_MQ7       = A2;

const bool use_sound = false;


// ***************************************************************************
// Variables of the application
// ***************************************************************************

// LCD object and its pins
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// Joystick object and its pins
Joystick joystick(pin_SW, pin_X, pin_Y);

// Variables for the sensor
int mq7_cal;
int mq7_target;
int mq7_vals[5];

// Variables to read text
char text[16];
char menu[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789<!";
int mlen;
int tpos;
int mpos;




// ***************************************************************************
// Application flow
// ***************************************************************************


void screen(char* line1, char* line2) {
    beep(5);
    lcd.clear();
    lcd.print(line1);
    lcd.setCursor(0, 1);
    lcd.print(line2);
}


void beep(int time) {
    if (use_sound) {
        digitalWrite(pin_Buzz, LOW);
        q.in(time, [=] {beep_off();});
}   }


void beep_off() {
    digitalWrite(pin_Buzz, HIGH);
}


void wait_for_press(Action action) {
    if (joystick.switch_on()) wait_for_release(action);
    else q.in(1, [=] {wait_for_press(action);});
}


void wait_for_release(Action action) {
    if (joystick.switch_off()) action();
    else q.in(1, [=] {wait_for_release(action);});
}

void on_answer(Action yes, Action no, bool op=true) {
    lcd.setCursor(0, 1);
    if (op) lcd.print("<Si>  No ");
    else    lcd.print(" Si  <No>");

    if (joystick.switch_on()) {
        return q.in(250, [=] {
            if (op) yes();
            else no();
        });
    } else {
        if ((op and joystick.x_high()) or (not op and joystick.x_low())) {
            op = not op;
        }
    }
    q.in(1, [=] {on_answer(yes, no, op);});
}


void hello1() {
    digitalWrite(pin_Red,       LOW );
    digitalWrite(pin_Yellow,    HIGH);
    digitalWrite(pin_Green,     HIGH);
    screen("Benvinguts!", "Piqueu el boto.");
    wait_for_press(show_log);
}


void show_log() {
    screen("Veure visites?", "");
    on_answer([=] {show_entry(1);}, hello2);
}

void show_entry(int n) {
    int cnt = int(EEPROM.read(0));
    char text[16];
    if (n > cnt) return show_log();
    read_entry(n, text);
    lcd.clear();
    lcd.print("Numero "); lcd.print(n);
    lcd.setCursor(0, 1);
    lcd.print(text);
    wait_for_press([=] {show_entry(n+1);});
}


void hello2() {
    screen("Per signar cal", "que feu un pet.");
    wait_for_press(hello3);
}


void hello3() {
    screen("El sensor ha de", "mesurar 100%");
    wait_for_press(hello4);
}


void hello4() {
    screen("Primer cal cali-", "brar el sensor.");
    wait_for_press(calibrate);
}




void calibrate() {
    static int cal_steps = 0;
    const int max_steps = 15;
    const int min_steps = 5;

    if (cal_steps == 0) {
        lcd.clear();
        lcd.print("Calibrant...");
    }

    ++cal_steps;

    int mq7 = read_mq7();

    lcd.setCursor(0, 1);
    lcd.print("Espereu ");
    lcd.print(max_steps - cal_steps + 1); lcd.print("s ");

    if ((mq7 <= 20 and cal_steps >= min_steps) or cal_steps == max_steps) wait_for_fart(1);
    else q.in(1000, [=] {calibrate();});
}


void wait_for_fart(int init) {
    static char* msg[6] = {
        "Feu un bon pet! ",
        "No n'hi ha prou.",
        "Cal mes meta!   ",
        "Olor insuficient",
        "Seguiu provant! ",
        "Cal el 100%     "
    };
    static unsigned ctr = 0;

    int mq7 = read_mq7();

    if (init) {
        digitalWrite(pin_Red, HIGH);
        digitalWrite(pin_Yellow, LOW);
        lcd.clear();
        mq7_vals[0] = mq7_vals[1] = mq7_vals[2] = mq7_vals[3] = mq7_vals[4] = mq7_cal = mq7;
        mq7_target = mq7_cal + 6;
    }

    lcd.setCursor(0, 0); lcd.print(msg[(++ctr / 10) % 6]);

    mq7_vals[0] = mq7_vals[1];
    mq7_vals[1] = mq7_vals[2];
    mq7_vals[2] = mq7_vals[3];
    mq7_vals[3] = mq7_vals[4];
    mq7_vals[4] = mq7;

    bool success = mq7_vals[0] >= mq7_target and mq7_vals[1] >= mq7_target and mq7_vals[2] >= mq7_target and mq7_vals[3] >= mq7_target and mq7_vals[4] >= mq7_target;

    int val = min(99, max(0, map(mq7_vals[4], mq7_cal, mq7_target, 0, 99)));

    lcd.setCursor(0, 1);
    lcd.print("Nivell: "); lcd.print(val); lcd.print("%  ");

    if (success) fart_completed();
    else q.in(400, [=] {wait_for_fart(0);});
}


void fart_completed() {
    int score = (mq7_vals[0] + mq7_vals[1] + mq7_vals[2] + mq7_vals[3] + mq7_vals[4]) / 5 - mq7_cal;

    digitalWrite(pin_Yellow, HIGH);
    digitalWrite(pin_Green, LOW);
    beep(100);
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Pet detectat!");
    lcd.setCursor(0, 1); lcd.print("Puntuacio: "); lcd.print(score);

    wait_for_press(sign1);
}


void sign1() {
    screen("Entreu nom", "pel registre.");
    wait_for_press([=] {sign2();});
}


void sign2() {
    strcpy(text, "               ");
    tpos = 0;
    mpos = 0;
    mlen = strlen(menu);
    lcd.clear();
    lcd.cursor();
    refresh();
    sign();
}


void erase() {
    if (--tpos == -1) tpos = 0;
    text[tpos] = ' ';
}


void append() {
    if (tpos == 15) tpos = 14;
    text[tpos++] = menu[mpos];
}


void refresh() {
    lcd.setCursor(0, 0);
    lcd.print(text);

    lcd.setCursor(0, 1);
    if (menu[mpos] == '<') {
        lcd.print("<Esborra>");
    } else if (menu[mpos] == '!') {
        lcd.print("<Accepta>");
    } else {
        lcd.print("<");
        lcd.print(menu[mpos]);
        lcd.print(">      ");
    }
    lcd.setCursor(tpos, 0);
}


void again() {
    refresh();
    q.in(250, [=] {sign();});
}


void sign() {
    if (joystick.switch_on()) {
        if (menu[mpos] == '<') {
            erase();
        } else if (menu[mpos] == '!') {
            return q.in(250, confirm_name);
        } else {
            append();
        }
        return again();
    } else {
        if (joystick.x_high()) {
            if (++mpos >= mlen) mpos = 0;
            return again();
        } else if (joystick.x_low()) {
            if (--mpos < 0) mpos = mlen - 1;
            return again();
        } else if (joystick.y_low()) {
            append();
            return again();
        } else if (joystick.y_high()) {
            erase();
            return again();
        }
    }
    q.in(1, [=] {sign();});
}

void confirm_name() {
    lcd.noCursor();
    screen(text, "");
    on_answer(goodbye2, sign1, false);
}


void goodbye2() {
    int cnt = int(EEPROM.read(0));
    ++cnt;
    save_entry(cnt, text);
    EEPROM.write(0, cnt);

    screen("Nom desat", "al registre.");
    wait_for_press(goodbye3);
}


void goodbye3() {
    lcd.noCursor();
    screen("Gracies per", "jugar. Adeu!");
    wait_for_press(hello1);
}




int read_mq7() {
    return map(analogRead(pin_MQ7), 0, 1023, 0, 100);
}


void debug_mq7() {
    Serial.print("mq7: ");
    Serial.println(read_mq7());
    q.in(1000, [=] {debug_mq7;});
}


void setup() {
    // Initialization of the serial port
    Serial.begin(9600);
    Serial.println("=========");

    // Configuration of pins
    pinMode(pin_Red,    OUTPUT);
    pinMode(pin_Yellow, OUTPUT);
    pinMode(pin_Green,  OUTPUT);
    pinMode(pin_Buzz,   OUTPUT);
    pinMode(pin_SW,     INPUT_PULLUP);

    // Initialitzation of leds and buzzer (take into account the reverse polarity!)
    digitalWrite(pin_Red,       LOW );
    digitalWrite(pin_Yellow,    HIGH);
    digitalWrite(pin_Green,     HIGH);
    digitalWrite(pin_Buzz,      HIGH);

    // Initialization of the display
    lcd.begin(16, 2);

    // Set timeouts
    Joystick* p = &joystick;
    q.in(joystick.delay, [=] {Joystick::update(p);});
    q.in(1000, [=] {debug_mq7();});

    // Show first screen
    hello1();
}


void loop() {
    q.drain();
}


