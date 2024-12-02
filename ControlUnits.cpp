#include "BasicSc2Bot.h"

void BasicSc2Bot::ControlUnits() {
	ControlSCVs();
	ControlBattlecruisers();
	ControlSiegeTanks();
	ControlMarines();
}