#include "global.h"

// Headless selftest driver:
// main() deletes a stale marker, then registers a per-frame watcher that
// quits ArmorPaint once the Free Library plugin writes the marker back
// (or after ~120s as a timeout).

int frames = 0;
int seen = 0;

void driver_update() {
	frames = frames + 1;
	if (iron_file_exists("/tmp/opencode/fl_done")) {
		if (seen == 0) {
			seen = 1;
			console_info("DRIVER: marker found");
			script_quit();
		}
		return;
	}
	if (frames > 7200) {
		console_info("DRIVER: TIMEOUT waiting for marker");
		script_quit();
	}
}

void main() {
	iron_delete_file("/tmp/opencode/fl_done");
	console_info("DRIVER: started");
	script_notify_on_update(driver_update);
}
