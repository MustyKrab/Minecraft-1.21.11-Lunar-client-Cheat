#include "render/XRay.h"
// Redirect to the real XRay implementation in src/modules/render/XRay.cpp
// Previous version was a conflicting namespace-based stub (Modules::XRay) that
// clashed with the proper class-based XRay in render/. ModuleManager uses render/XRay.
