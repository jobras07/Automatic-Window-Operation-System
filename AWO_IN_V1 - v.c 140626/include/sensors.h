#pragma once
#include "config.h"
#include "DHT_Async.h"
#include "statemachine.h"

void sensors_init();
void sensors_update(SystemContext& ctx);
int  sensors_getPosition();