#include "ESP.h"
#include "../../core/JNIHelper.h"
#include <iostream>

ESP::ESP() : Module("ESP") {}

void ESP::OnTick() {
    // TODO: Implement JNI ESP logic
    // We will use GDI+ or DirectX overlay here, ripping the matrices from Lunar Client
    // exactly like we did in the Java version, but entirely externally.
}
