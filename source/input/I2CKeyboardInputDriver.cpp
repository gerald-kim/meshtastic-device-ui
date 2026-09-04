
#include "input/I2CKeyboardInputDriver.h"
#include "util/ILog.h"
#include <Arduino.h>
#include <Wire.h>
#include <cstddef>

#include "indev/lv_indev_private.h"

namespace
{
constexpr uint8_t TCA8418_REG_CFG = 0x01;
constexpr uint8_t TCA8418_REG_INT_STAT = 0x02;
constexpr uint8_t TCA8418_REG_KEY_LCK_EC = 0x03;
constexpr uint8_t TCA8418_REG_KEY_EVENT_A = 0x04;
constexpr uint8_t TCA8418_REG_GPIO_INT_EN_1 = 0x1a;
constexpr uint8_t TCA8418_REG_GPIO_INT_EN_2 = 0x1b;
constexpr uint8_t TCA8418_REG_GPIO_INT_EN_3 = 0x1c;
constexpr uint8_t TCA8418_REG_KP_GPIO_1 = 0x1d;
constexpr uint8_t TCA8418_REG_KP_GPIO_2 = 0x1e;
constexpr uint8_t TCA8418_REG_KP_GPIO_3 = 0x1f;
constexpr uint8_t TCA8418_REG_GPI_EM_1 = 0x20;
constexpr uint8_t TCA8418_REG_GPI_EM_2 = 0x21;
constexpr uint8_t TCA8418_REG_GPI_EM_3 = 0x22;
constexpr uint8_t TCA8418_REG_GPIO_DIR_1 = 0x23;
constexpr uint8_t TCA8418_REG_GPIO_DIR_2 = 0x24;
constexpr uint8_t TCA8418_REG_GPIO_DIR_3 = 0x25;
constexpr uint8_t TCA8418_REG_GPIO_INT_LVL_1 = 0x26;
constexpr uint8_t TCA8418_REG_GPIO_INT_LVL_2 = 0x27;
constexpr uint8_t TCA8418_REG_GPIO_INT_LVL_3 = 0x28;
constexpr uint8_t TCA8418_REG_DEBOUNCE_DIS_1 = 0x29;
constexpr uint8_t TCA8418_REG_DEBOUNCE_DIS_2 = 0x2a;
constexpr uint8_t TCA8418_REG_DEBOUNCE_DIS_3 = 0x2b;

bool tca8418WriteRegister(uint8_t address, uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool tca8418ReadRegister(uint8_t address, uint8_t reg, uint8_t &value)
{
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom((int)address, 1) != 1 || !Wire.available())
        return false;
    value = Wire.read();
    return true;
}

uint32_t toLvKey(const TLoraPagerKeyPolicy::Action &action)
{
    using Kind = TLoraPagerKeyPolicy::ActionKind;
    switch (action.kind) {
    case Kind::Character:
        return action.codepoint;
    case Kind::TextBackspace:
        return LV_KEY_BACKSPACE;
    case Kind::NavigateBack:
        return LV_KEY_ESC;
    case Kind::Submit:
    case Kind::Activate:
        return LV_KEY_ENTER;
    case Kind::NavigateNext:
        return LV_KEY_NEXT;
    default:
        return 0;
    }
}
} // namespace

I2CKeyboardInputDriver::KeyboardList I2CKeyboardInputDriver::i2cKeyboardList;

I2CKeyboardInputDriver::I2CKeyboardInputDriver(void) {}

void I2CKeyboardInputDriver::init(void)
{
    keyboard = lv_indev_create();
    lv_indev_set_type(keyboard, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(keyboard, keyboard_read);

    if (!inputGroup) {
        inputGroup = lv_group_create();
        lv_group_set_default(inputGroup);
    }
    lv_indev_set_group(keyboard, inputGroup);
}

bool I2CKeyboardInputDriver::registerI2CKeyboard(I2CKeyboardInputDriver *driver, std::string name, uint8_t address)
{
    auto keyboardDef = std::unique_ptr<KeyboardDefinition>(new KeyboardDefinition{driver, name, address});
    i2cKeyboardList.push_back(std::move(keyboardDef));
    ILOG_INFO("Registered I2C keyboard: %s at address 0x%02X", name.c_str(), address);
    return true;
}

void I2CKeyboardInputDriver::keyboard_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    // Read from all registered keyboards
    for (auto &keyboardDef : i2cKeyboardList) {
        keyboardDef->driver->readKeyboard(keyboardDef->address, indev, data);
        if (data->state == LV_INDEV_STATE_PRESSED) {
            // If any keyboard reports a key press, we stop reading further
            return;
        }
    }
}

// ---------- TDeckKeyboardInputDriver Implementation ----------

TDeckKeyboardInputDriver::TDeckKeyboardInputDriver(uint8_t address)
{
    registerI2CKeyboard(this, "T-Deck Keyboard", address);
}

/******************************************************************
    LV_KEY_NEXT: Focus on the next object
    LV_KEY_PREV: Focus on the previous object
    LV_KEY_ENTER: Triggers LV_EVENT_PRESSED, LV_EVENT_CLICKED, or LV_EVENT_LONG_PRESSED etc. events
    LV_KEY_UP: Increase value or move upwards
    LV_KEY_DOWN: Decrease value or move downwards
    LV_KEY_RIGHT: Increase value or move to the right
    LV_KEY_LEFT: Decrease value or move to the left
    LV_KEY_ESC: Close or exit (E.g. close a Drop down list)
    LV_KEY_DEL: Delete (E.g. a character on the right in a Text area)
    LV_KEY_BACKSPACE: Delete a character on the left (E.g. in a Text area)
    LV_KEY_HOME: Go to the beginning/top (E.g. in a Text area)
    LV_KEY_END: Go to the end (E.g. in a Text area)

    LV_KEY_UP        = 17,  // 0x11
    LV_KEY_DOWN      = 18,  // 0x12
    LV_KEY_RIGHT     = 19,  // 0x13
    LV_KEY_LEFT      = 20,  // 0x14
    LV_KEY_ESC       = 27,  // 0x1B
    LV_KEY_DEL       = 127, // 0x7F
    LV_KEY_BACKSPACE = 8,   // 0x08
    LV_KEY_ENTER     = 10,  // 0x0A, '\n'
    LV_KEY_NEXT      = 9,   // 0x09, '\t'
    LV_KEY_PREV      = 11,  // 0x0B, '
    LV_KEY_HOME      = 2,   // 0x02, STX
    LV_KEY_END       = 3,   // 0x03, ETX
*******************************************************************/

void TDeckKeyboardInputDriver::readKeyboard(uint8_t address, lv_indev_t *indev, lv_indev_data_t *data)
{
    char keyValue = 0;
    uint8_t bytes = Wire.requestFrom(address, 1);
    if (Wire.available() > 0 && bytes > 0) {
        keyValue = Wire.read();
        // ignore empty reads and keycode 224(E0, shift-0 on T-Deck) which causes internal issues
        if (keyValue != (char)0x00 && keyValue != (char)0xE0) {
            data->state = LV_INDEV_STATE_PRESSED;
            ILOG_DEBUG("key press value: %d", (int)keyValue);

            switch (keyValue) {
            case 0x0D:
                keyValue = LV_KEY_ENTER;
                break;
            default:
                break;
            }
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    }
    data->key = (uint32_t)keyValue;
}

// ---------- TCA8418KeyboardInputDriver Implementation ----------

TCA8418KeyboardInputDriver::TCA8418KeyboardInputDriver(uint8_t address) : TCA8418KeyboardInputDriver(address, "TCA8418 Keyboard")
{
}

TCA8418KeyboardInputDriver::TCA8418KeyboardInputDriver(uint8_t address, const char *name) : i2cAddress(address)
{
    registerI2CKeyboard(this, name, address);
}

void TCA8418KeyboardInputDriver::init(void)
{
    // Additional initialization for TCA8418 if needed
    I2CKeyboardInputDriver::init();
}

void TCA8418KeyboardInputDriver::readKeyboard(uint8_t address, lv_indev_t *indev, lv_indev_data_t *data)
{
    // TODO
    char keyValue = 0;
    data->state = LV_INDEV_STATE_RELEASED;
    data->key = (uint32_t)keyValue;
}

// ---------- TLoraPagerKeyboardInputDriver Implementation ----------

TLoraPagerKeyboardInputDriver::TLoraPagerKeyboardInputDriver(uint8_t address, TLoraPagerInputController &controller)
    : TCA8418KeyboardInputDriver(address, "TLora Pager Keyboard"), controller(controller)
{
}

void TLoraPagerKeyboardInputDriver::init(void)
{
    TCA8418KeyboardInputDriver::init();

    bool initialized = true;
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_GPIO_DIR_1, 0x00);
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_GPIO_DIR_2, 0x00);
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_GPIO_DIR_3, 0x00);
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_GPI_EM_1, 0xff);
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_GPI_EM_2, 0xff);
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_GPI_EM_3, 0xff);
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_GPIO_INT_LVL_1, 0x00);
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_GPIO_INT_LVL_2, 0x00);
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_GPIO_INT_LVL_3, 0x00);
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_GPIO_INT_EN_1, 0xff);
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_GPIO_INT_EN_2, 0xff);
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_GPIO_INT_EN_3, 0xff);
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_DEBOUNCE_DIS_1, 0x00);
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_DEBOUNCE_DIS_2, 0x00);
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_DEBOUNCE_DIS_3, 0x00);
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_KP_GPIO_1, 0x0f);
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_KP_GPIO_2, 0xff);
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_KP_GPIO_3, 0x03);

    uint8_t pending = 0;
    for (uint8_t i = 0; i < 10 && tca8418ReadRegister(i2cAddress, TCA8418_REG_KEY_EVENT_A, pending) && pending != 0; ++i) {
    }
    initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_INT_STAT, 0x03);
    uint8_t config = 0;
    if (tca8418ReadRegister(i2cAddress, TCA8418_REG_CFG, config))
        initialized &= tca8418WriteRegister(i2cAddress, TCA8418_REG_CFG, config | 0x01);
    else
        initialized = false;

    if (initialized)
        ILOG_INFO("T-LoRa Pager TCA8418 keyboard initialized");
    else
        ILOG_ERROR("T-LoRa Pager TCA8418 keyboard initialization failed");
}

void TLoraPagerKeyboardInputDriver::readKeyboard(uint8_t address, lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->state = LV_INDEV_STATE_RELEASED;
    data->key = 0;

    uint8_t count = 0;
    uint8_t fifoEvent = 0;
    if (!tca8418ReadRegister(address, TCA8418_REG_KEY_LCK_EC, count) || (count & 0x0f) == 0 ||
        !tca8418ReadRegister(address, TCA8418_REG_KEY_EVENT_A, fifoEvent))
        return;

    data->continue_reading = (count & 0x0f) > 1;
    if ((count & 0x0f) == 1)
        tca8418WriteRegister(address, TCA8418_REG_INT_STAT, 0x01);

    const bool textInput = controller.isTextInputActive();
    const TLoraPagerKeyPolicy::RawEvent raw{static_cast<uint8_t>(fifoEvent & 0x7f), (fifoEvent & 0x80) != 0, millis()};
    // Pager text actions are handled directly by TLoraPagerInputController and
    // therefore never reach LVGL as a pressed key. Explicitly count every
    // physical key-down as display activity so continuous typing resets the
    // screen timeout and a key that woke light sleep keeps the display awake.
    if (raw.pressed)
        lv_display_trigger_activity(nullptr);
    const auto dispatch = controller.feed(raw, {true, textInput});
    if (dispatch.consumed || !dispatch.policy.emit)
        return;

    const uint32_t lvKey = toLvKey(dispatch.policy.action);
    if (lvKey == 0)
        return;
    data->state = dispatch.policy.action.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->key = lvKey;
}

// ---------- TDeckProKeyboardInputDriver Implementation ----------

TDeckProKeyboardInputDriver::TDeckProKeyboardInputDriver(uint8_t address)
    : TCA8418KeyboardInputDriver(address, "T-Deck Pro Keyboard")
{
}

void TDeckProKeyboardInputDriver::init(void)
{
    // Additional initialization for TLora-Pager if needed
    TCA8418KeyboardInputDriver::init();
}

void TDeckProKeyboardInputDriver::readKeyboard(uint8_t address, lv_indev_t *indev, lv_indev_data_t *data)
{
    // TODO
    char keyValue = 0;
    data->state = LV_INDEV_STATE_RELEASED;
    data->key = (uint32_t)keyValue;
}

// ---------- BBQ10KeyboardInputDriver Implementation ----------

BBQ10KeyboardInputDriver::BBQ10KeyboardInputDriver(uint8_t address)
{
    registerI2CKeyboard(this, "BBQ10 Keyboard", address);
}

void BBQ10KeyboardInputDriver::init(void)
{
    I2CKeyboardInputDriver::init();
    // Additional initialization for BBQ10 if needed
}

void BBQ10KeyboardInputDriver::readKeyboard(uint8_t address, lv_indev_t *indev, lv_indev_data_t *data)
{
    char keyValue = 0;
    uint8_t bytes = Wire.requestFrom(address, 1);
    if (Wire.available() > 0 && bytes > 0) {
        keyValue = Wire.read();
        // ignore empty reads and keycode 224(E0, shift-0 on T-Deck) which causes internal issues
        if (keyValue != (char)0x00 && keyValue != (char)0xE0) {
            data->state = LV_INDEV_STATE_PRESSED;
            ILOG_DEBUG("key press value: %d", (int)keyValue);

            switch (keyValue) {
            case 0x0D:
                keyValue = LV_KEY_ENTER;
                break;
            default:
                break;
            }
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    }
    data->key = (uint32_t)keyValue;
}

// ---------- CardKBInputDriver Implementation ----------

CardKBInputDriver::CardKBInputDriver(uint8_t address, TwoWire &wire_) : wire(wire_)
{
    registerI2CKeyboard(this, "Card Keyboard", address);
}

void CardKBInputDriver::readKeyboard(uint8_t address, lv_indev_t *indev, lv_indev_data_t *data)
{
    char keyValue = 0;
    wire.requestFrom(address, 1);
    if (wire.available() > 0) {
        keyValue = wire.read();
        // ignore empty reads and keycode 224 which causes internal issues
        if (keyValue != (char)0x00 && keyValue != (char)0xE0) {
            data->state = LV_INDEV_STATE_PRESSED;
            ILOG_DEBUG("key press value: %d", (int)keyValue);

            switch (keyValue) {
            case 0x0D:
                keyValue = LV_KEY_ENTER;
                break;
            case 0xB4:
                keyValue = LV_KEY_LEFT;
                break;
            case 0xB5:
                keyValue = LV_KEY_UP;
                break;
            case 0xB6:
                keyValue = LV_KEY_DOWN;
                break;
            case 0xB7:
                keyValue = LV_KEY_RIGHT;
                break;
            case 0x99: // Fn+UP
                keyValue = LV_KEY_HOME;
                break;
            case 0xA4: // Fn+DOWN
                keyValue = LV_KEY_END;
                break;
            case 0x8B: // Fn+BS
                keyValue = LV_KEY_DEL;
                break;
            case 0x8C: // Fn+TAB
                keyValue = LV_KEY_PREV;
                break;
            case 0xA3: // Fn+ENTER
                // simulate a long press on Fn+ENTER (see indev_keypad_proc() in indev.c)
                indev->wait_until_release = 0;
                indev->pr_timestamp = lv_tick_get() - indev->long_press_time - 1;
                indev->long_pr_sent = 0;
                indev->keypad.last_state = LV_INDEV_STATE_PRESSED;
                indev->keypad.last_key = LV_KEY_ENTER;
                keyValue = LV_KEY_ENTER;
                break;
            default:
                break;
            }
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    }
    data->key = (uint32_t)keyValue;
}

// ---------- MPR121KeyboardInputDriver Implementation ----------

MPR121KeyboardInputDriver::MPR121KeyboardInputDriver(uint8_t address)
{
    registerI2CKeyboard(this, "MPR121 Keyboard", address);
}

void MPR121KeyboardInputDriver::init(void)
{
    I2CKeyboardInputDriver::init();
    // Additional initialization for MPR121 if needed
}

void MPR121KeyboardInputDriver::readKeyboard(uint8_t address, lv_indev_t *indev, lv_indev_data_t *data)
{
    // TODO
    char keyValue = 0;
    data->state = LV_INDEV_STATE_RELEASED;
    data->key = (uint32_t)keyValue;
}
