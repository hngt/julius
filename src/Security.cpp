
#include "Building.h"
#include "Calc.h"
#include "CityInfo.h"
#include "PlayerMessage.h"
#include "Routing.h"
#include "SidebarMenu.h"
#include "Sound.h"
#include "Terrain.h"
#include "TerrainGraphics.h"
#include "Time.h"
#include "Walker.h"
#include "WalkerAction.h"

#include "Data/Building.h"
#include "Data/CityInfo.h"
#include "Data/Constants.h"
#include "Data/Grid.h"
#include "Data/Message.h"
#include "Data/Random.h"
#include "Data/Scenario.h"
#include "Data/Settings.h"
#include "Data/State.h"
#include "Data/Tutorial.h"
#include "Data/Walker.h"

#define EACH_BURNING_RUIN Data_BuildingList.burning.index = 0; Data_BuildingList.burning.index < Data_BuildingList.burning.size; Data_BuildingList.burning.index++

static int burningRuinSpreadDirection;

void Security_Tick_updateBurningRuins()
{
	int needsTerrainUpdate = 0;
	Data_BuildingList.burning.index = 0;
	Data_BuildingList.burning.size = 0;
	Data_BuildingList.burning.totalBurning = 0;
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		struct Data_Building *b = &Data_Buildings[i];
		if (b->inUse != 1 || b->type != Building_BurningRuin) {
			continue;
		}
		if (b->fireDuration < 0) {
			b->fireDuration = 0;
		}
		b->fireDuration++;
		if (b->fireDuration > 32) {
			Data_State.undoAvailable = 0;
			b->inUse = 4;
			TerrainGraphics_setBuildingAreaRubble(i, b->x, b->y, b->size);
			needsTerrainUpdate = 1;
			continue;
		}
		if (b->ruinHasPlague) {
			continue;
		}
		Data_BuildingList.burning.totalBurning++;
		Data_BuildingList.burning.items[Data_BuildingList.burning.size++] = i;
		if (Data_BuildingList.burning.size >= 500) {
			Data_BuildingList.burning.size = 499;
		}
		if (Data_Scenario.climate == Climate_Desert) {
			if (b->fireDuration & 3) { // check spread every 4 ticks
				continue;
			}
		} else {
			if (b->fireDuration & 7) { // check spread every 8 ticks
				continue;
			}
		}
		if ((b->houseGenerationDelay & 3) != (Data_Random.random1_7bit & 3)) {
			continue;
		}
		int dir1 = burningRuinSpreadDirection - 1;
		if (dir1 < 0) dir1 = 7;
		int dir2 = burningRuinSpreadDirection + 1;
		if (dir2 > 7) dir2 = 0;
		
		int gridOffset = b->gridOffset;
		int nextBuildingId = Data_Grid_buildingIds[gridOffset + Constant_DirectionGridOffsets[burningRuinSpreadDirection]];
		if (nextBuildingId && !Data_Buildings[nextBuildingId].fireProof) {
			Building_collapseOnFire(nextBuildingId, 0);
			Building_collapseLinked(nextBuildingId, 1);
			Sound_Effects_playChannel(SoundChannel_Explosion);
			needsTerrainUpdate = 1;
		} else {
			nextBuildingId = Data_Grid_buildingIds[gridOffset + Constant_DirectionGridOffsets[dir1]];
			if (nextBuildingId && !Data_Buildings[nextBuildingId].fireProof) {
				Building_collapseOnFire(nextBuildingId, 0);
				Building_collapseLinked(nextBuildingId, 1);
				Sound_Effects_playChannel(SoundChannel_Explosion);
				needsTerrainUpdate = 1;
			} else {
				nextBuildingId = Data_Grid_buildingIds[gridOffset + Constant_DirectionGridOffsets[dir2]];
				if (nextBuildingId && !Data_Buildings[nextBuildingId].fireProof) {
					Building_collapseOnFire(nextBuildingId, 0);
					Building_collapseLinked(nextBuildingId, 1);
					Sound_Effects_playChannel(SoundChannel_Explosion);
					needsTerrainUpdate = 1;
				}
			}
		}
	}
}

int Security_Fire_getClosestBurningRuin(int x, int y, int *distance)
{
	int minFreeBuildingId = 0;
	int minOccupiedBuildingId = 0;
	int minOccupiedDist = *distance = 10000;
	for (EACH_BURNING_RUIN) {
		int buildingId = Data_BuildingList.burning.items[Data_BuildingList.burning.index];
		struct Data_Building *b = &Data_Buildings[buildingId];
		if (b->inUse == 1 && b->type == Building_BurningRuin && !b->ruinHasPlague && b->distanceFromEntry) {
			int dist = Calc_distanceMaximum(x, y, b->x, b->y);
			if (b->walkerId4) {
				if (dist < minOccupiedDist) {
					minOccupiedDist = dist;
					minOccupiedBuildingId = buildingId;
				}
			} else if (dist < *distance) {
				*distance = dist;
				minFreeBuildingId = buildingId;
			}
		}
	}
	if (!minFreeBuildingId && minOccupiedDist <= 2) {
		minFreeBuildingId = minOccupiedBuildingId;
		*distance = minOccupiedDist;
	}
	return minFreeBuildingId;
}

static void generateRioter(int buildingId)
{
	int xRoad, yRoad;
	struct Data_Building *b = &Data_Buildings[buildingId];
	if (!Terrain_getClosestRoadWithinRadius(b->x, b->y, b->size, 2, &xRoad, &yRoad)) {
		return;
	}
	Data_CityInfo.numCriminalsThisMonth++;
	int peopleInMob;
	if (Data_CityInfo.population <= 150) {
		peopleInMob = 1;
	} else if (Data_CityInfo.population <= 300) {
		peopleInMob = 2;
	} else if (Data_CityInfo.population <= 800) {
		peopleInMob = 3;
	} else if (Data_CityInfo.population <= 1200) {
		peopleInMob = 4;
	} else if (Data_CityInfo.population <= 2000) {
		peopleInMob = 5;
	} else {
		peopleInMob = 6;
	}
	int targetX, targetY;
	int targetBuildingId = WalkerAction_Rioter_getTargetBuilding(&targetX, &targetY);
	for (int i = 0; i < peopleInMob; i++) {
		int walkerId = Walker_create(Walker_Rioter, xRoad, yRoad, 4);
		struct Data_Walker *w = &Data_Walkers[walkerId];
		w->actionState = WalkerActionState_120_RioterCreated;
		w->roamLength = 0;
		w->waitTicks = 10 + 4 * i;
		if (targetBuildingId) {
			w->destinationX = targetX;
			w->destinationY = targetY;
			w->destinationBuildingId = targetBuildingId;
		} else {
			w->state = WalkerState_Dead;
		}
	}
	Building_collapseOnFire(buildingId, 0);
	Data_CityInfo.ratingPeaceNumRiotersThisYear++;
	Data_CityInfo.riotCause = Data_CityInfo.populationEmigrationCause;
	CityInfo_Population_changeHappiness(20);
	if (!Data_Tutorial.tutorial1.crime) {
		Data_Tutorial.tutorial1.crime = 1;
		SidebarMenu_enableBuildingMenuItems();
		SidebarMenu_enableBuildingButtons();
	}
	if (Time_getMillis() <= 15000 + Data_Message.lastSoundTime.rioterGenerated) {
		PlayerMessage_disableSoundForNextMessage();
	} else {
		Data_Message.lastSoundTime.rioterGenerated = Time_getMillis();
	}
	PlayerMessage_postWithPopupDelay(MessageDelay_Riot, 11, b->type, GridOffset(xRoad, yRoad));
}

static void generateMugger(int buildingId)
{
	Data_CityInfo.numCriminalsThisMonth++;
	struct Data_Building *b = &Data_Buildings[buildingId];
	if (b->houseCriminalActive < 1) {
		b->houseCriminalActive = 1;
		int xRoad, yRoad;
		if (Terrain_getClosestRoadWithinRadius(b->x, b->y, b->size, 2, &xRoad, &yRoad)) {
			int walkerId = Walker_create(Walker_Criminal, xRoad, yRoad, 4);
			Data_Walkers[walkerId].waitTicks = 10 + (b->houseGenerationDelay & 0xf);
			Data_CityInfo.ratingPeaceNumCriminalsThisYear++;
			if (Data_CityInfo.financeTaxesThisYear > 20) {
				int moneyStolen = Data_CityInfo.financeTaxesThisYear / 4;
				if (moneyStolen > 400) {
					moneyStolen = 400 - Data_Random.random1_7bit / 2;
				}
				PlayerMessage_post(1, 52, moneyStolen, Data_Walkers[walkerId].gridOffset);
				Data_CityInfo.financeStolenThisYear += moneyStolen;
				Data_CityInfo.treasury -= moneyStolen;
				Data_CityInfo.financeSundriesThisYear += moneyStolen;
			}
		}
	}
}

static void generateProtester(int buildingId)
{
	Data_CityInfo.numProtestersThisMonth++;
	struct Data_Building *b = &Data_Buildings[buildingId];
	if (b->houseCriminalActive < 1) {
		b->houseCriminalActive = 1;
		int xRoad, yRoad;
		if (Terrain_getClosestRoadWithinRadius(b->x, b->y, b->size, 2, &xRoad, &yRoad)) {
			int walkerId = Walker_create(Walker_Protester, xRoad, yRoad, 4);
			Data_Walkers[walkerId].waitTicks = 10 + (b->houseGenerationDelay & 0xf);
			Data_CityInfo.ratingPeaceNumCriminalsThisYear++;
		}
	}
}

void Security_Tick_generateCriminal()
{
	int minBuildingId = 0;
	int minHappiness = 50;
	for (int i = 1; i <= Data_Buildings_Extra.highestBuildingIdInUse; i++) {
		struct Data_Building *b = &Data_Buildings[i];
		if (b->inUse == 1 && b->houseSize) {
			if (b->sentiment.houseHappiness >= 50) {
				b->houseCriminalActive = 0;
			} else if (b->sentiment.houseHappiness < minHappiness) {
				minHappiness = b->sentiment.houseHappiness;
				minBuildingId = i;
			}
		}
	}
	if (minBuildingId) {
		if (IsTutorial1() || IsTutorial2()) {
			return;
		}
		if (Data_CityInfo.citySentiment < 30) {
			if (Data_Random.random1_7bit >= Data_CityInfo.citySentiment + 50) {
				if (minHappiness <= 10) {
					generateRioter(minBuildingId);
				} else if (minHappiness < 30) {
					generateMugger(minBuildingId);
				} else if (minHappiness < 50) {
					generateProtester(minBuildingId);
				}
			}
		} else if (Data_CityInfo.citySentiment < 60) {
			if (Data_Random.random1_7bit >= Data_CityInfo.citySentiment + 40) {
				if (minHappiness < 30) {
					generateMugger(minBuildingId);
				} else if (minHappiness < 50) {
					generateProtester(minBuildingId);
				}
			}
		} else {
			if (Data_Random.random1_7bit >= Data_CityInfo.citySentiment + 20) {
				if (minHappiness < 50) {
					generateProtester(minBuildingId);
				}
			}
		}
	}
}

static void collapseBuilding(int buildingId, struct Data_Building *b)
{
	if (Time_getMillis() - Data_Message.lastSoundTime.collapse <= 15000) {
		PlayerMessage_disableSoundForNextMessage();
	} else {
		Data_Message.lastSoundTime.collapse = Time_getMillis();
	}
	if (Data_Tutorial.tutorial1.collapse) {
		// regular collapse
		int usePopup = 0;
		if (Data_Message.messageDelay[MessageDelay_Collapse] <= 0) {
			usePopup = 1;
			Data_Message.messageDelay[MessageDelay_Collapse] = 12;
		}
		PlayerMessage_post(usePopup, 13, b->type, b->gridOffset);
		Data_Message.messageCategoryCount[MessageDelay_Collapse]++;
	} else {
		// first collapse in tutorial
		Data_Tutorial.tutorial1.collapse = 1;
		SidebarMenu_enableBuildingMenuItems();
		SidebarMenu_enableBuildingButtons();
		/*if (UI_Window_getId() == Window_City) {
			UI_City_drawBackground(); // TODO do we need this??
		}*/
		PlayerMessage_post(1, 54, 0, 0);
	}
	
	Data_State.undoAvailable = 0;
	b->inUse = 4;
	TerrainGraphics_setBuildingAreaRubble(buildingId, b->x, b->y, b->size);
	Walker_createDustCloud(b->x, b->y, b->size);
	Building_collapseLinked(buildingId, 0);
}

static void fireBuilding(int buildingId, struct Data_Building *b)
{
	if (Time_getMillis() - Data_Message.lastSoundTime.fire <= 15000) {
		PlayerMessage_disableSoundForNextMessage();
	} else {
		Data_Message.lastSoundTime.fire = Time_getMillis();
	}
	if (Data_Tutorial.tutorial1.fire) {
		// regular collapse
		int usePopup = 0;
		if (Data_Message.messageDelay[MessageDelay_Fire] <= 0) {
			usePopup = 1;
			Data_Message.messageDelay[MessageDelay_Fire] = 12;
		}
		PlayerMessage_post(usePopup, 12, b->type, b->gridOffset);
		Data_Message.messageCategoryCount[MessageDelay_Fire]++;
	} else {
		// first collapse in tutorial
		Data_Tutorial.tutorial1.fire = 1;
		SidebarMenu_enableBuildingMenuItems();
		SidebarMenu_enableBuildingButtons();
		/*if (UI_Window_getId() == Window_City) {
			UI_City_drawBackground(); // TODO do we need this??
		}*/
		PlayerMessage_post(1, 53, 0, 0);
	}
	
	Building_collapseOnFire(buildingId, 0);
	Building_collapseLinked(buildingId, 1);
	Sound_Effects_playChannel(SoundChannel_Explosion);
}

void Security_Tick_checkFireCollapse()
{
	Data_CityInfo.numProtestersThisMonth = 0;
	Data_CityInfo.numCriminalsThisMonth = 0; // last month or this month?
	
	int recalculateTerrain = 0;
	int randomGlobal = Data_Random.random1_7bit;
	for (int i = 1; i <= Data_Buildings_Extra.highestBuildingIdInUse; i++) {
		struct Data_Building *b = &Data_Buildings[i];
		if (b->inUse != 1 || b->fireProof) {
			continue;
		}
		if (b->type == Building_Hippodrome && b->prevPartBuildingId) {
			continue;
		}
		int randomBuilding = (i + Data_Grid_random[b->gridOffset]) & 7;
		// damage
		b->damageRisk += (randomBuilding == randomGlobal) ? 3 : 1;
		if (Data_Tutorial.tutorial1.fire == 1 && !Data_Tutorial.tutorial1.collapse) {
			b->damageRisk += 5;
		}
		if (b->houseSize && b->subtype.houseLevel <= HouseLevel_LargeTent) {
			b->damageRisk = 0;
		}
		if (b->damageRisk > 200) {
			collapseBuilding(i, b);
			recalculateTerrain = 1;
			continue;
		}
		// fire
		if (randomBuilding == randomGlobal) {
			if (!b->houseSize) {
				b->fireRisk += 5;
			} else if (b->housePopulation <= 0) {
				b->fireRisk = 0;
			} else if (b->subtype.houseLevel <= HouseLevel_LargeShack) {
				b->fireRisk += 10;
			} else if (b->subtype.houseLevel <= HouseLevel_GrandInsula) {
				b->fireRisk += 5;
			} else {
				b->fireRisk += 2;
			}
			if (!Data_Tutorial.tutorial1.fire) {
				b->fireRisk += 5;
			}
			if (Data_Scenario.climate == Climate_Northern) {
				b->fireRisk = 0;
			}
			if (Data_Scenario.climate == Climate_Desert) {
				b->fireRisk += 3;
			}
		}
		if (b->fireRisk > 100) {
			fireBuilding(i, b);
			recalculateTerrain = 1;
		}
	}
	
	if (recalculateTerrain) {
		Routing_determineLandCitizen();
		Routing_determineLandNonCitizen();
	}
}