#pragma once
#include "config.h"
#include "statemachine.h"

void display_init();
void display_update(SystemContext& ctx);
void display_showMessage(const char* line1, const char* line2);