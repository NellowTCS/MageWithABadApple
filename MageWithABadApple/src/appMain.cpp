// PocketMage V3.0
// @Ashtf 2025

#include <pocketmage.h>
#include <algorithm>

static constexpr const char* TAG = "MAIN";

int x = 0;

// ADD PROCESS/KEYBOARD APP SCRIPTS HERE
void processKB() {
  
  // Draw a progress bar across the screen and then return to PocketMage OS
  u8g2.clearBuffer();
  u8g2.drawBox(0,0,x,u8g2.getDisplayHeight());
  
  x+=5;
  
  if (x > u8g2.getDisplayWidth()) {
    // Return to pocketMage OS
    rebootToPocketMage();
  }

  u8g2.sendBuffer();
  delay(10);
}

void applicationEinkHandler() {
  // Playback state (persistent across calls)
  static bool initialized = false;
  static std::vector<String> frames;
  static size_t idx = 0;
  static unsigned long lastMs = 0;
  static unsigned long frameDelayMs = 80; // adjust: lower -> faster (but e-ink may ghost)
  static uint8_t* buf = nullptr;
  static size_t expectedSize = 0;
  static bool warnedSize = false;

  const String folder = "/screensavers/badapple";

  // Get runtime display dimensions to compute expected size and show info
  const int dispW = display.width();
  const int dispH = display.height();
  const size_t expected = (size_t)(dispW) * (size_t)(dispH) / 8;

  if (!initialized) {
    initialized = true;
    frames.clear();
    idx = 0;
    lastMs = millis();
    warnedSize = false;

    // Try to open directory on SD card
    File dir = SD_MMC.open(folder.c_str());
    if (!dir) {
      OLED().oledWord("No BadApple folder");
      return;
    }

    // iterate directory and collect files
    while (true) {
      File entry = dir.openNextFile();
      if (!entry) break;
      if (!entry.isDirectory()) {
        String name = String(entry.name());
        // Some SD implementations return full path, some return relative name.
        // Normalize to absolute path under folder.
        if (!name.startsWith(folder)) {
          String base = name;
          // if name begins with '/', strip it for concatenation
          if (base.startsWith("/")) base = base.substring(1);
          String full = folder;
          if (!full.endsWith("/")) full += "/";
          full += base;
          name = full;
        }
        frames.push_back(name);
      }
      entry.close();
    }
    dir.close();

    if (frames.empty()) {
      OLED().oledWord("No BadApple frames");
      return;
    }

    // Sort lexicographically so zero-padded filenames play in order
    std::sort(frames.begin(), frames.end(), [](const String &a, const String &b){
      return a < b;
    });

    // Allocate buffer once
    expectedSize = expected;
    buf = (uint8_t*)malloc(expectedSize);
    if (!buf) {
      OLED().oledWord("No RAM for BadApple");
      frames.clear();
      return;
    }

    // Inform user
    OLED().oledWord("BadApple loaded");
    lastMs = millis();
    idx = 0;
  }

  if (frames.empty()) return;

  // Only proceed at the target frame rate
  unsigned long now = millis();
  if (now - lastMs < frameDelayMs) return;
  lastMs = now;

  // Open current frame
  const String path = frames[idx];
  File f = SD_MMC.open(path.c_str(), FILE_READ);
  if (!f) {
    // unreadable: skip to next
    idx = (idx + 1) % frames.size();
    return;
  }

  // Validate size once (warn if mismatch)
  if (f.size() != expectedSize) {
    if (!warnedSize) {
      // Show warning once
      OLED().oledWord("Frame size mismatch");
      warnedSize = true;
    }
    f.close();
    idx = (idx + 1) % frames.size();
    return;
  }

  // Read file fully into buffer (seek/read)
  size_t readBytes = f.read(buf, expectedSize);
  f.close();
  if (readBytes != expectedSize) {
    idx = (idx + 1) % frames.size();
    return;
  }

  // Draw the bitmap and refresh via Pocketmage EINK helper
  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);
  // drawBitmap expects a 1-bit-per-pixel buffer packed 8 horizontal pixels per byte (MSB = leftmost)
  display.drawBitmap(0, 0, buf, dispW, dispH, GxEPD_BLACK, GxEPD_WHITE);
  EINK().refresh();

  // advance frame index
  idx = (idx + 1) % frames.size();
}

/////////////////////////////////////////////////////////////
//  ooo        ooooo       .o.       ooooo ooooo      ooo  //
//  `88.       .888'      .888.      `888' `888b.     `8'  //
//   888b     d'888      .8"888.      888   8 `88b.    8   //
//   8 Y88. .P  888     .8' `888.     888   8   `88b.  8   //
//   8  `888'   888    .88ooo8888.    888   8     `88b.8   //
//   8    Y     888   .8'     `888.   888   8       `888   //
//  o8o        o888o o88o     o8888o o888o o8o        `8   //
/////////////////////////////////////////////////////////////
// SETUP
void setup() {
  PocketMage_INIT();
}

void loop() {
  // Check battery
  pocketmage::power::updateBattState();
  
  // Run KB loop
  processKB();

  // Yield to watchdog
  vTaskDelay(50 / portTICK_PERIOD_MS);
  yield();
}

// migrated from einkFunc.cpp
void einkHandler(void* parameter) {
  vTaskDelay(pdMS_TO_TICKS(250)); 
  for (;;) {
    applicationEinkHandler();

    vTaskDelay(pdMS_TO_TICKS(50));
    yield();
  }
}