#pragma once
// Redirect to the real XRay header.
// Previous version declared a conflicting Modules::XRay namespace with static bEnabled
// that clashed with the proper XRay class in render/XRay.h used by ModuleManager.
#include "render/XRay.h"
