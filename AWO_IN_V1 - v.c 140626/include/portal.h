#pragma once
#include "config.h"
#include "storage.h"
#include "statemachine.h"

void portal_start();
bool portal_update(SystemContext& ctx);  // returns true when WiFi connected
void portal_stop();