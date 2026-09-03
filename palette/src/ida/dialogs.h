#pragma once

// Modal dialogs reachable from the Edit > Plugins > ifred submenu.
// Both must be called on the UI thread; IDA action handlers already run there.

void show_settings_dialog();
void show_about_dialog();
