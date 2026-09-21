#include "input.h"
#include "console.h"
#include <raylib.h>
#include <cstring>

bool inputs[INPUT_COUNT];
bool lastinputs[INPUT_COUNT];
in_action mappings[INPUT_COUNT];
in_action last_input_action = { -1, -1 };

int ACCEPT_BUTTON = JUMPKEY;
int DECLINE_BUTTON = FIREKEY;

bool input_init(void)
{
  std::memset(inputs, 0, sizeof(inputs));
  std::memset(lastinputs, 0, sizeof(lastinputs));

  // Default Keyboard and gamepad bindings
  mappings[LEFTKEY]      = { KEY_LEFT,   GAMEPAD_BUTTON_LEFT_FACE_LEFT };
  mappings[RIGHTKEY]     = { KEY_RIGHT,  GAMEPAD_BUTTON_LEFT_FACE_RIGHT };
  mappings[UPKEY]        = { KEY_UP,     GAMEPAD_BUTTON_LEFT_FACE_UP };
  mappings[DOWNKEY]      = { KEY_DOWN,   GAMEPAD_BUTTON_LEFT_FACE_DOWN };
  mappings[JUMPKEY]      = { KEY_Z,      GAMEPAD_BUTTON_RIGHT_FACE_DOWN };
  mappings[FIREKEY]      = { KEY_X,      GAMEPAD_BUTTON_RIGHT_FACE_RIGHT };
  mappings[STRAFEKEY]    = { KEY_C,      GAMEPAD_BUTTON_LEFT_TRIGGER_1 };
  mappings[PREVWPNKEY]   = { KEY_A,      GAMEPAD_BUTTON_LEFT_TRIGGER_1 };
  mappings[NEXTWPNKEY]   = { KEY_S,      GAMEPAD_BUTTON_RIGHT_TRIGGER_1 };
  mappings[INVENTORYKEY] = { KEY_Q,      GAMEPAD_BUTTON_RIGHT_FACE_LEFT };
  mappings[MAPSYSTEMKEY] = { KEY_W,      GAMEPAD_BUTTON_RIGHT_FACE_UP };

  mappings[ESCKEY]   = { KEY_ESCAPE, GAMEPAD_BUTTON_MIDDLE_LEFT };
  mappings[ENTERKEY] = { KEY_ENTER,  GAMEPAD_BUTTON_MIDDLE_RIGHT };

  mappings[F1KEY] = { KEY_F1, -1 };
  mappings[F2KEY] = { KEY_F2, -1 };
  mappings[F3KEY] = { KEY_F3, -1 };
  mappings[F4KEY] = { KEY_F4, -1 };
  mappings[F5KEY] = { KEY_F5, -1 };
  mappings[F6KEY] = { KEY_F6, -1 };
  mappings[F7KEY] = { KEY_F7, -1 };
  mappings[F8KEY] = { KEY_F8, -1 };
  mappings[F9KEY] = { KEY_F9, -1 };

  mappings[FREEZE_FRAME_KEY]  = { KEY_SPACE, -1 };
  mappings[FRAME_ADVANCE_KEY] = { KEY_B, -1 };
  mappings[DEBUG_FLY_KEY]     = { KEY_V, -1 };

  return true;
}

void input_close(void) {}

void input_poll(void)
{
  std::memcpy(lastinputs, inputs, sizeof(inputs));

  // Handle debug console toggle
  if (IsKeyPressed(KEY_GRAVE))
  {
    console.SetVisible(!console.IsVisible());
  }

  // If console is visible, route text and navigation keys to console
  if (console.IsVisible())
  {
    int charPressed = GetCharPressed();
    while (charPressed > 0)
    {
      console.HandleKey(charPressed);
      charPressed = GetCharPressed();
    }

    int keyPressed = GetKeyPressed();
    while (keyPressed > 0)
    {
      console.HandleKey(keyPressed);
      keyPressed = GetKeyPressed();
    }
    return;
  }

  int gamepad = 0;
  bool hasGamepad = IsGamepadAvailable(gamepad);
  float axisX = hasGamepad ? GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X) : 0.0f;
  float axisY = hasGamepad ? GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_Y) : 0.0f;

  for (int i = 0; i < INPUT_COUNT; i++)
  {
    bool down = false;

    // Keyboard
    if (mappings[i].key != -1 && IsKeyDown(mappings[i].key))
      down = true;

    // Gamepad buttons
    if (hasGamepad && mappings[i].gamepad_button != -1)
    {
      if (IsGamepadButtonDown(gamepad, mappings[i].gamepad_button))
        down = true;
    }

    // Left analog stick thresholding
    if (hasGamepad)
    {
      if (i == LEFTKEY  && axisX < -0.5f) down = true;
      if (i == RIGHTKEY && axisX >  0.5f) down = true;
      if (i == UPKEY    && axisY < -0.5f) down = true;
      if (i == DOWNKEY  && axisY >  0.5f) down = true;
    }

    #if defined(PLATFORM_ANDROID)
    int touchCount = GetTouchPointCount();
    for (int t = 0; t < touchCount; t++)
    {
      Vector2 pos = GetTouchPosition(t);
      float sw = (float)GetScreenWidth();
      float sh = (float)GetScreenHeight();

      if (pos.x < sw * 0.35f)
      {
        if (i == LEFTKEY  && pos.x < sw * 0.15f) down = true;
        if (i == RIGHTKEY && pos.x > sw * 0.15f) down = true;
        if (i == UPKEY    && pos.y < sh * 0.65f) down = true;
        if (i == DOWNKEY  && pos.y > sh * 0.75f) down = true;
      }
      if (pos.x > sw * 0.65f)
      {
        if (i == JUMPKEY && pos.y > sh * 0.6f) down = true;
        if (i == FIREKEY && pos.y < sh * 0.6f) down = true;
      }
    }
    #endif

    inputs[i] = down;
  }
}

bool justpushed(int k)
{
  return (inputs[k] && !lastinputs[k]);
}

bool buttondown(void)
{
  return (inputs[JUMPKEY] || inputs[FIREKEY] || inputs[STRAFEKEY]);
}

bool buttonjustpushed(void)
{
  return (justpushed(JUMPKEY) || justpushed(FIREKEY) || justpushed(STRAFEKEY));
}

void rumble(float str, uint32_t len)
{
  // Reserved for platform haptic implementation
  (void)str; (void)len;
}

void input_remap(int index, in_action act) { if (index < INPUT_COUNT) mappings[index] = act; }
in_action input_get_mapping(int index)     { return mappings[index]; }
void input_set_mappings(in_action *array)  { std::memcpy(mappings, array, sizeof(mappings)); }

const std::string input_get_name(int index)
{
  static const char *names[] = {
    "Left", "Right", "Up", "Down", "Jump", "Fire", "Strafe", "Wpn Prev", "Wpn Next",
    "Inventory", "Map", "Pause", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8",
    "F9", "F10", "F11", "F12", "Freeze", "Step", "Fly", "Enter"
  };
  if (index >= 0 && index < INPUT_COUNT) return names[index];
  return "Unknown";
}

const char *get_key_name(int key)
{
  switch (key)
  {
    case KEY_SPACE: return "Space";
    case KEY_ESCAPE: return "Esc";
    case KEY_ENTER: return "Enter";
    case KEY_TAB: return "Tab";
    case KEY_BACKSPACE: return "Backspace";
    case KEY_UP: return "Up";
    case KEY_DOWN: return "Down";
    case KEY_LEFT: return "Left";
    case KEY_RIGHT: return "Right";
    case KEY_LEFT_SHIFT: return "LShift";
    case KEY_RIGHT_SHIFT: return "RShift";
    case KEY_LEFT_CONTROL: return "LCtrl";
    case KEY_RIGHT_CONTROL: return "RCtrl";
    case KEY_LEFT_ALT: return "LAlt";
    case KEY_RIGHT_ALT: return "RAlt";
    default:
      if (key >= 32 && key <= 126)
      {
        static char ch[2] = {0, 0};
        ch[0] = (char)key;
        return ch;
      }
      return "Unknown";
  }
}