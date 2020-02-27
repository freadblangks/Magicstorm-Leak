/*
* Copyright (C) 2016-2017 ColossusWoW <https://colossuswow.com/>
*
* Este codigo es de uso exclusivo y privado para la red de gaming
* ColossusWoW est? totalmente prohibida su distribuci?n y
* uso en proyectos ajenos, si has obtenido o encontrado este codigo
* publicamente envianos un email a administracion@colossuswow.com
* indicandonos como y donde has obtenido este codigo
*
* Recuerda, no distribuyas, no compartas y no subas este codigo a ningun
* lugar publico, usa siempre nuestros repositorios Git!
*/

#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "SpellScript.h"
#include "ScriptedCreature.h"
#include "Unit.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "AreaTriggerTemplate.h"
#include "AreaTriggerAI.h"
#include "AreaTrigger.h"
#include "MotionMaster.h"

enum Spells
{
    NightmareBreath = 215821,
    TailLash = 215806,
    BurningEarth = 215876,
    BurningEarthAreatrigger = 215872,
    DreadFlame = 216043,
    CryOfTheTormented = 216044,
};

enum Events
{
    EventTailLash = 1,                  // Every 8 secs 
    EventNightmareBreath = 2,           // Every 18 secs
    EventDreadFlame = 3,                // Every 19-25 secs
    EventBurningEarth = 4,              // Every 28 secs
    EventCryOfTheTormented = 5,         // Every 63 secs
};

class boss_shar_thos : public CreatureScript
{
public:
    boss_shar_thos() : CreatureScript("boss_shar_thos") { }

    struct boss_shar_thosAI : public WorldBossAI
    {
        boss_shar_thosAI(Creature* creature) : WorldBossAI(creature) {}

        void Reset() override
        {
            _Reset();
            me->SetFullHealth();
            me->GetMotionMaster()->MoveTargetedHome();
            events.Reset();
        }

        void EnterCombat(Unit* who) override
        {
            if (!who)
                return;
            me->setActive(true);
            me->SetFullHealth();
             
            events.ScheduleEvent(EventTailLash, 8 * IN_MILLISECONDS);
            events.ScheduleEvent(EventDreadFlame, 11 * IN_MILLISECONDS);
            events.ScheduleEvent(EventNightmareBreath, 16 * IN_MILLISECONDS);
            events.ScheduleEvent(EventBurningEarth, 28 * IN_MILLISECONDS);
            events.ScheduleEvent(EventCryOfTheTormented, 63 * IN_MILLISECONDS);
        }

        void EnterEvadeMode(EvadeReason /*why*/) override
        {
            _EnterEvadeMode();
            Reset();
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case Events::EventTailLash:
                    DoCastAOE(Spells::TailLash);
                    events.ScheduleEvent(Events::EventTailLash, 8000);
                    break;
                case Events::EventDreadFlame:
                    me->CastSpell(me->GetVictim(), Spells::DreadFlame);
                    events.ScheduleEvent(EventDreadFlame, urand(19, 25) * IN_MILLISECONDS);
                    break;
                case Events::EventNightmareBreath:
                    DoCastAOE(Spells::NightmareBreath);
                    events.ScheduleEvent(Events::EventNightmareBreath, 18000);
                    break;
                case Events::EventBurningEarth:
                    me->GetPlayerListInGrid(nearbyPlayers, 50.0f);
                   
                    if (!nearbyPlayers.empty())
                        Trinity::Containers::RandomResize(nearbyPlayers, 1);
                    
                    for (Player* target : nearbyPlayers)
                        me->CastSpell((Unit*)target, Spells::BurningEarthAreatrigger);
                    
                    events.ScheduleEvent(EventBurningEarth, 28 * IN_MILLISECONDS);
                    break;
                case Events::EventCryOfTheTormented:
                    DoCastAOE(Spells::CryOfTheTormented);
                    events.ScheduleEvent(EventCryOfTheTormented, 63000);
                    break;
                }
            }

            DoMeleeAttackIfReady();
        }
    private:
        std::list<Player*> nearbyPlayers;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new boss_shar_thosAI(creature);
    }
};

// 215872 - BurningEarth
class at_sharthos_burning_earth : public AreaTriggerEntityScript
{
public:
    at_sharthos_burning_earth() : AreaTriggerEntityScript("at_sharthos_burning_earth") { }
    
    struct at_sharthos_burning_earthAI : AreaTriggerAI
    {
        at_sharthos_burning_earthAI(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }
    
        uint32 checkTimer = 1000;

        enum Spells
        {
            BurningEarthDOT = 215876,
        };

        // Called on each AreaTrigger update
        void OnUpdate(uint32 diff) 
        { 
            if (checkTimer <= diff)
            {
                float yards = at->GetTimeSinceCreated() / IN_MILLISECONDS * 1.1f;
                
                GuidUnorderedSet const& insideTargets = at->GetInsideUnits();

                for (ObjectGuid insideTargetGuid : insideTargets)
                    if (Unit* insideTarget = ObjectAccessor::GetUnit(*at->GetCaster(), insideTargetGuid))
                        if (insideTarget->IsPlayer())
                        {
                            if (insideTarget->GetDistance(at->GetPosition()) <= yards && !insideTarget->HasAura(Spells::BurningEarthDOT))
                                at->GetCaster()->AddAura(Spells::BurningEarthDOT, insideTarget);
                            else if (insideTarget->GetDistance(at->GetPosition()) > yards && insideTarget->HasAura(Spells::BurningEarthDOT))
                                insideTarget->RemoveAurasDueToSpell(Spells::BurningEarthDOT);
                        }
                checkTimer = 1000;
            }
            else
                checkTimer -= diff;
        }
        
        // Called when an unit exit the AreaTrigger, or when the AreaTrigger is removed
        void OnUnitExit(Unit* unit) override 
        { 
            if (unit->IsPlayer() && unit->HasAura(Spells::BurningEarthDOT))
                unit->RemoveAurasDueToSpell(Spells::BurningEarthDOT);
        }
    };

    AreaTriggerAI* GetAI(AreaTrigger* areatrigger) const override
    {
        return new at_sharthos_burning_earthAI(areatrigger);
    }
};

void AddSC_boss_sharthos()
{
    new boss_shar_thos();
    new at_sharthos_burning_earth();
}
