/*-------------------------------------------------------------------------------

	BARONY
	File: actpowerorb.cpp
	Desc: behavior function for power orbs

	Copyright 2013-2016 (c) Turning Wheel LLC, all rights reserved.
	See LICENSE for details.

-------------------------------------------------------------------------------*/

#include "main.hpp"
#include "game.hpp"
#include "stat.hpp"
#include "entity.hpp"
#include "monster.hpp"
#include "engine/audio/sound.hpp"
#include "items.hpp"
#include "net.hpp"
#include "collision.hpp"
#include "player.hpp"
#include "magic/magic.hpp"
#include "prng.hpp"

/*-------------------------------------------------------------------------------

	act*

	The following function describes an entity behavior. The function
	takes a pointer to the entity that uses it as an argument.

-------------------------------------------------------------------------------*/

void actPedestalBase(Entity* my)
{
	if ( !my )
	{
		return;
	}

	my->actPedestalBase();
}

void actPedestalOrb(Entity* my)
{
	if ( !my )
	{
		return;
	}

	my->actPedestalOrb();
}

void Entity::actPedestalBase()
{
	node_t* node = children.first;
	Entity* orbEntity = (Entity*)(node->element);
	if ( pedestalInit == 0 )
	{
		pedestalPowerStatus = -1;
	}
	if ( pedestalInit == 0 && !pedestalInGround )
	{
		pedestalInit = 1;
	}

#ifdef USE_FMOD
	if ( pedestalAmbience == 0 )
	{
		pedestalAmbience--;
		stopEntitySound();
		entity_sound = playSoundEntityLocal(this, 149, 64);
	}
	if ( entity_sound )
	{
		bool playing = false;
		entity_sound->isPlaying(&playing);
		if ( !playing )
		{
			entity_sound = nullptr;
		}
	}
#else
	pedestalAmbience--;
	if ( pedestalAmbience <= 0 )
	{
		pedestalAmbience = TICKS_PER_SECOND * 30;
		playSoundEntityLocal(this, 149, 64);
	}
#endif

	if ( !light )
	{
		light = addLight(x / 16, y / 16, "pedestal");
	}

	if ( ticks == 1 )
	{
		this->createWorldUITooltip();
	}

	if ( pedestalInGround )
	{
		if ( pedestalInit == 0 )
		{
			if ( this->ticks < 50 )
			{
				return;
			}
			// wait for external source to trigger the initialisation.
			if ( multiplayer != CLIENT )
			{
				node_t* node;
				for ( node = map.entities->first; node != NULL; node = node->next )
				{
					Entity* entity = (Entity*)node->element;
					if ( entity->behavior == &actMonster )
					{
						Stat* stats = entity->getStats();
						if ( stats )
						{
							if ( stats->type == LICH )
							{
								return;
							}
						}
					}
				}
				pedestalInit = 1;
				serverUpdateEntitySkill(this, 5);
			}
			return;
		}

		if ( z > 4.5 )
		{
			if ( z == 4.5 + 11 )
			{
				playSoundEntityLocal(players[clientnum]->entity, 250, 128);
			}
			vel_z = -0.1;
			z += vel_z;
			orbEntity->vel_z = vel_z;
			orbEntity->z += orbEntity->vel_z;
			// shake camera if in range.
			for (int c = 0; c < MAXPLAYERS; ++c) 
			{
				if ( !players[c]->isLocalPlayer() || !players[c]->isLocalPlayerAlive() ) 
				{
					continue;
				}
				auto& player = players[c];
				if ( player && player->entity )
				{
					real_t dist = entityDist(player->entity, this);
					if ( dist < 512 && ticks % 5 == 0 )
					{
						cameravars[c].shakex += .02;
						cameravars[c].shakey += 2;
					}
				}
			}
		}
		else
		{
			z = 4.5;
			orbEntity->z = -2;
			vel_z = 0;
			orbEntity->vel_z = 0;
			pedestalInGround = 0;
			if ( multiplayer != CLIENT )
			{
				serverUpdateEntitySkill(this, 4);
			}
		}
	}

	if ( multiplayer == CLIENT )
	{
		return;
	}

	if ( pedestalPowerStatus < SWITCH_UNPOWERED )
	{
		// set the entity to be a circuit if not already set.
		if ( !pedestalInvertedPower )
		{
			pedestalPowerStatus = SWITCH_UNPOWERED;
		}
		else
		{
			pedestalPowerStatus = SWITCH_POWERED;
		}
	}

	if ( pedestalPowerStatus == SWITCH_POWERED )
	{
		switchUpdateNeighbors();
	}

	if ( pedestalHasOrb == pedestalOrbType )
	{
		bool applyAura = false;
		// power on/off the circuit if it hasn't updated
		if ( pedestalPowerStatus == SWITCH_UNPOWERED && !pedestalInvertedPower )
		{
			//mechanismPowerOn();
			//updateCircuitNeighbors();
			toggleSwitch(8);
			if ( !strncmp(map.name, "Boss", 4) )
			{
				applyAura = true;
			}
		}
		else if ( pedestalPowerStatus == SWITCH_POWERED && pedestalInvertedPower )
		{
			//mechanismPowerOff();
			//updateCircuitNeighbors();
			toggleSwitch(8);
		}

		if ( (applyAura || ticks % 400 == 0) && pedestalOrbType != 3 && !strncmp(map.name, "Boss", 4) )
		{
			for ( int i = 0; i < MAXPLAYERS; ++i )
			{
				if ( !client_disconnected[i] )
				{
					if ( players[i] && players[i]->entity )
					{
						switch ( pedestalOrbType )
						{
							case 1: // blue
								if ( stats[i] && !stats[i]->EFFECTS[EFF_SHRINE_BLUE_BUFF] )
								{
									messagePlayer(i, MESSAGE_INTERACTION, Language::get(2910));
								}
								players[i]->entity->setEffect(EFF_SHRINE_BLUE_BUFF, true, 1000, false);
								break;
							case 2: // red
								if ( stats[i] && !stats[i]->EFFECTS[EFF_SHRINE_RED_BUFF] )
								{
									messagePlayer(i, MESSAGE_INTERACTION, Language::get(2904));
								}
								players[i]->entity->setEffect(EFF_SHRINE_RED_BUFF, true, 1000, false);
								break;
							case 3:
								break;
							case 4: // green
								if ( stats[i] && !stats[i]->EFFECTS[EFF_SHRINE_GREEN_BUFF] )
								{
									messagePlayer(i, MESSAGE_INTERACTION, Language::get(2909));
								}
								players[i]->entity->setEffect(EFF_SHRINE_GREEN_BUFF, true, 1000, false);
								break;
							default:
								break;
						}
					}
				}
			}
		}
	}

	if ( flags[PASSABLE] && pedestalInGround == 0 )
	{
		// see if any entity is currently inside, otherwise set PASSABLE to false
		bool somebodyInside = false;
		node_t* node2 = nullptr;
		for ( node2 = map.entities->first; node2 != nullptr; node2 = node2->next )
		{
			Entity* entity = (Entity*)node2->element;
			if ( entity == this || (entity->flags[PASSABLE] && entity->behavior != &actDeathGhost)
				|| entity->behavior == &actDoorFrame || entity == orbEntity )
			{
				continue;
			}
			if ( entityInsideEntity(this, entity) )
			{
				somebodyInside = true;
				break;
			}
		}
		if ( !somebodyInside )
		{
			flags[PASSABLE] = false;
			serverUpdateEntityFlag(this, PASSABLE);
		}
	}

	// handle player interaction
	for ( int i = 0; i < MAXPLAYERS; i++ )
	{
		if ( selectedEntity[i] == this || client_selected[i] == this )
		{
			if ( inrange[i] )
			{
				if ( players[i] && players[i]->entity )
				{
					if ( orbEntity && pedestalHasOrb > 0 )
					{
						if ( pedestalHasOrb == pedestalOrbType && pedestalLockOrb == 1 )
						{
							// if orb locked, then can't retreive.
							messagePlayer(i, MESSAGE_INTERACTION, Language::get(2367));
						}
						else
						{
							Item* itemOrb = newItem(static_cast<ItemType>(ARTIFACT_ORB_BLUE + pedestalHasOrb - 1), EXCELLENT, 0, 1, local_rng.rand(), true, nullptr);
							itemPickup(i, itemOrb);
							if ( pedestalHasOrb == pedestalOrbType )
							{
								// only update power when right orb is in place.
								if ( !pedestalInvertedPower && pedestalPowerStatus == SWITCH_POWERED )
								{
									//mechanismPowerOff();
									//updateCircuitNeighbors();
									toggleSwitch(8);
								}
								else if ( pedestalInvertedPower && pedestalPowerStatus == SWITCH_UNPOWERED )
								{
									//mechanismPowerOn();
									//updateCircuitNeighbors();
									toggleSwitch(8);
								}
							}
							pedestalHasOrb = 0;
							serverUpdateEntitySkill(this, 0); // update orb status.
							messagePlayer(i, MESSAGE_INTERACTION, Language::get(2374), itemOrb->getName());
						}
					}
					else
					{
						if ( players[i]->entity->getINT() < 10 )
						{
							if ( local_rng.rand() % 2 == 0 )
							{
								messagePlayer(i, MESSAGE_INTERACTION, Language::get(476));
							}
							else
							{
								messagePlayer(i, MESSAGE_INTERACTION, Language::get(2364));
							}
						}
						else if ( players[i]->entity->getINT() < 15 )
						{
							messagePlayer(i, MESSAGE_INTERACTION, Language::get(2365));
						}
						else
						{
							messagePlayer(i, MESSAGE_INTERACTION, Language::get(2366));
						}
					}
				}
			}	
		}
	}
}

void Entity::actPedestalOrb()
{
	real_t upper_z = orbStartZ - 0.4;
	real_t lower_z = orbStartZ + 0.4;
	int i = 0;

	real_t acceleration = 0.95;

	Entity* parent = uidToEntity(this->parent);
	if ( !parent )
	{
		return;
	}

	if ( !parent->pedestalInGround )
	{
		pedestalOrbInit();
	}

	if ( parent->pedestalHasOrb == 0 )
	{
		flags[INVISIBLE] = true;
		flags[UNCLICKABLE] = true;
		flags[PASSABLE] = true;
		orbTurnVelocity = 0.5; // reset the speed of the orb.
		removeLightField();
		return;
	}
	else if ( orbInitialised )
	{
		sprite = parent->pedestalHasOrb + 602 - 1;

		// handle player interaction
		if ( multiplayer != CLIENT )
		{
			for ( int i = 0; i < MAXPLAYERS; i++ )
			{
				if ( selectedEntity[i] == this || client_selected[i] == this )
				{
					if ( inrange[i] )
					{
						if ( players[i] && players[i]->entity )
						{
							if ( parent->pedestalHasOrb > 0 )
							{
								if ( parent->pedestalHasOrb == parent->pedestalOrbType && parent->pedestalLockOrb == 1 )
								{
									// if orb locked, then can't retreive.
									messagePlayer(i, MESSAGE_INTERACTION, Language::get(2367));
								}
								else
								{
									Item* itemOrb = newItem(static_cast<ItemType>(ARTIFACT_ORB_BLUE + parent->pedestalHasOrb - 1), EXCELLENT, 0, 1, local_rng.rand(), true, nullptr);
									itemPickup(i, itemOrb);
									if ( parent->pedestalHasOrb == parent->pedestalOrbType )
									{
										// only update power when right orb is in place.
										if ( !parent->pedestalInvertedPower && parent->pedestalPowerStatus == SWITCH_POWERED )
										{
											//mechanismPowerOff();
											//updateCircuitNeighbors();
											parent->toggleSwitch(8);
										}
										else if ( parent->pedestalInvertedPower && parent->pedestalPowerStatus == SWITCH_UNPOWERED )
										{
											//mechanismPowerOn();
											//updateCircuitNeighbors();
											parent->toggleSwitch(8);
										}
									}
									parent->pedestalHasOrb = 0;
									serverUpdateEntitySkill(parent, 0); // update orb status 
									messagePlayer(i, MESSAGE_INTERACTION | MESSAGE_INVENTORY, Language::get(2374), itemOrb->getName());
								}
							}
						}
					}
				}
			}
		}

		if ( parent->pedestalHasOrb != parent->pedestalOrbType )
		{
			// not properly activated - return early, no animate.
			flags[INVISIBLE] = false;
			flags[UNCLICKABLE] = false;
			flags[PASSABLE] = false;
			return;
		}
		else if ( parent->pedestalHasOrb == parent->pedestalOrbType )
		{
			flags[INVISIBLE] = false;
			flags[UNCLICKABLE] = false;
			flags[PASSABLE] = false;
			if ( !light )
			{
                switch (parent->pedestalOrbType) {
                default:
                case 1: light = addLight(x / 16, y / 16, "orb_blue"); break;
                case 2: light = addLight(x / 16, y / 16, "orb_red"); break;
                case 3: light = addLight(x / 16, y / 16, "orb_purple"); break;
                case 4: light = addLight(x / 16, y / 16, "orb_green"); break;
                }
			}
		}
	}
	else
	{
		return;
	}

	if ( orbHoverDirection == CRYSTAL_HOVER_UP ) //rise state
	{
		z -= vel_z;

		if ( z < upper_z )
		{
			z = upper_z;
			orbHoverDirection = CRYSTAL_HOVER_UP_WAIT;
		}

		if ( z < orbStartZ ) //higher than mid point
		{
			vel_z = std::max(vel_z * acceleration, orbMinZVelocity);
		}
		else if ( z > orbStartZ ) //lower than midpoint
		{
			vel_z = std::min(vel_z * (1 / acceleration), orbMaxZVelocity);
		}
	}
	else if ( orbHoverDirection == CRYSTAL_HOVER_UP_WAIT ) // wait state
	{
		orbHoverWaitTimer++;
		if ( orbHoverWaitTimer >= 1 )
		{
			orbHoverDirection = CRYSTAL_HOVER_DOWN; // advance state
			orbHoverWaitTimer = 0; // reset timer
		}
	}
	else if ( orbHoverDirection == CRYSTAL_HOVER_DOWN ) //fall state
	{
		z += vel_z;

		if ( z > lower_z )
		{
			z = lower_z;
			orbHoverDirection = CRYSTAL_HOVER_DOWN_WAIT;
		}

		if ( z < orbStartZ ) //higher than mid point, start accelerating
		{
			vel_z = std::min(vel_z * (1 / acceleration), orbMaxZVelocity);
		}
		else if ( z > orbStartZ ) //lower than midpoint, start decelerating
		{
			vel_z = std::max(vel_z * acceleration, orbMinZVelocity);
		}
	}
	else if ( orbHoverDirection == CRYSTAL_HOVER_DOWN_WAIT ) // wait state
	{
		orbHoverWaitTimer++;
		if ( orbHoverWaitTimer >= 1 )
		{
			orbHoverDirection = CRYSTAL_HOVER_UP; // advance state
			orbHoverWaitTimer = 0; // reset timer
		}
	}


	if ( z <= orbStartZ + orbMaxZVelocity && z >= orbStartZ - orbMaxZVelocity )
	{
		vel_z = orbMaxZVelocity; // reset velocity at the mid point of animation
	}

	yaw += (orbTurnVelocity * abs((vel_z / orbMaxZVelocity)) + orbTurnVelocity);
	if ( orbTurnVelocity > 0.04 )
	{
		orbTurnVelocity -= 0.01;
	}

	int particleSprite = 606;

	switch ( sprite )
	{
		case 602:
		case 603:
		case 604:
		case 605:
			particleSprite = sprite + 4;
			break;
		default:
			particleSprite = -1;
			break;
	}
	spawnAmbientParticles(40, particleSprite, 10 + local_rng.rand() % 40, 1.0, false);
}

void Entity::pedestalOrbInit()
{
	Entity* parent = uidToEntity(this->parent);

	if ( !parent )
	{
		return;
	}

	if ( !orbInitialised && !parent->pedestalInGround )
	{
		x = parent->x;
		y = parent->y;
		z = -2;
		sizex = 2;
		sizey = 2;
		if ( parent->pedestalHasOrb == parent->pedestalOrbType )
		{
			flags[UNCLICKABLE] = false;
			flags[INVISIBLE] = false;
		}
		else
		{
			flags[UNCLICKABLE] = true;
			flags[INVISIBLE] = true;
		}
		flags[PASSABLE] = true;
		if ( orbStartZ != z )
		{
			orbStartZ = z;
			z = orbStartZ - 0.4 + ((local_rng.rand() % 8) * 0.1); // start the height randomly
		}
		orbMaxZVelocity = 0.02; //max velocity
		orbMinZVelocity = 0.001; //min velocity
		vel_z = crystalMaxZVelocity * ((local_rng.rand() % 100) * 0.01); // start the velocity randomly
		orbTurnVelocity = 0.5;
		orbInitialised = 1;
		if ( multiplayer != CLIENT )
		{
			serverUpdateEntitySkill(parent, 0);
			serverUpdateEntitySkill(parent, 1);
		}
	}
}

