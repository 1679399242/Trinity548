/*Copyright (C) 2014 Buli.
*
* This file is NOT free software. Third-party users may NOT redistribute it or modify it :).
*/

#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "MapManager.h"
#include "Spell.h"
#include "Vehicle.h"
#include "Cell.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CreatureTextMgr.h"
#include "Unit.h"
#include "Player.h"
#include "Weather.h"

#include "heart_of_fear.h"

enum Yells
{
    // He's a bug! Has no yells, just announces.
    ANN_CRUSH              = 0, // Garalon begins to [Crush] his opponents!
    ANN_MEND               = 1, // Garalon [Mends] one of his legs!
    ANN_FURY               = 2, // Garalon fails to hit at least two players and gains [Fury]!
    ANN_DAMAGED            = 3, // Garalon's massive armor plating begins to crack and split!
    ANN_BERSERK            = 4  // Garalon becomes [Enraged] and prepares to execute a [Massive Crush]!
};

enum Spells
{
    /*** Garalon ***/
    SPELL_FURIOUS_SWIPE    = 122735, // Primary attack ability.
    SPELL_FURY             = 122754, // If he doesn't hit at least two targets with Swipe.

    SPELL_CRUSH_BODY_VIS   = 122082, // Visual for boss body, blue circle underneath. Triggers SPELL_CRUSH_TRIGGER each sec.
    SPELL_CRUSH_TRIGGER    = 117709, // Dummy effect for SPELL_CRUSH_DUMMY casted on players in 14 yards.
    SPELL_CRUSH_DUMMY      = 124068, // Dummy placed on players underneath. They take the extra damage.

    SPELL_CRUSH            = 122774, // Extra damage for players underneath (in 14 yards), on Effect 1.
    SPELL_MASSIVE_CRUSH    = 128555, // Hard enrage spell, used to wipe the raid: 10 million damage.

    SPELL_PHER_INIT_CAST   = 123808, // When boss is pulled, after 2 secs this casts SPELL_PHEROMONES_AURA on nearest player.
    SPELL_PHEROMONES_AURA  = 122835, // Trig SPELL_PHEROMONES_TAUNT, SPELL_PUNGENCY, SPELL_PHEROMONES_DMG, SPELL_PHEROMONES_FC / 2s + SPELL_PHER_CANT_BE_HIT, SPELL_PHEROMONES_DUMMY.

    SPELL_PHEROMONES_TAUNT = 123109, // Garalon Taunt / Attack Me spell.
    SPELL_PUNGENCY         = 123081, // Stacking 10% damage increase aura.
    SPELL_PHEROMONES_DMG   = 123092, // Damage spell, triggers SPELL_SUMM_PHER_TRAIL.
    SPELL_SUMM_PHER_TRAIL  = 128573, // Summon spell for NPC_PHEROMONE_TRAIL.
    SPELL_PHEROMONES_FC    = 123100, // Force Cast of SPELL_PHEROMONES_JUMP in 5 yards.
    SPELL_PHER_CANT_BE_HIT = 124056, // Some kind of dummy check.
    SPELL_PHEROMONES_DUMMY = 130662, // Special prereq dummy for SPELL_PHER_INIT_CAST.

    SPELL_DAMAGED          = 123818, // Heroic, 33%. Uses melee, increased speed, ignores Pheromones taunt.

    SPELL_BERSERK          = 120207, // Enrage, 420 seconds, or 7 minutes.

    /*** Pheromone Trail ****/
    SPELL_PHER_TRAIL_DMG_A = 123106, // Triggers SPELL_PHER_TRAIL_DMG each sec.
    SPELL_PHER_TRAIL_DMG   = 123120, // 25000 damage in 4 yards.

    /*** Garalon's Legs ****/
    // Ride Vehicle auras for each Leg.
    SPELL_RIDE_FRONT_RIGHT = 123430, // Ride Vehicle (Front Right Leg) - Triggers 122757 Weak Points aura which triggers SPELL_WEAK_POINT_VIS1 and SPELL_WEAK_POINTS_FR.
    SPELL_RIDE_FRONT_LEFT  = 123431, // Ride Vehicle (Front Left Leg)  - Triggers 123424 Weak Points aura which triggers SPELL_WEAK_POINT_VIS2 and SPELL_WEAK_POINTS_FL.
    SPELL_RIDE_BACK_LEFT   = 123432, // Ride Vehicle (Back Left Leg)   - Triggers 123425 Weak Points aura which triggers SPELL_WEAK_POINT_VIS3 and SPELL_WEAK_POINTS_BL.
    SPELL_RIDE_BACK_RIGHT  = 123433, // Ride Vehicle (Back Right Leg)  - Triggers 123427 Weak Points aura which triggers SPELL_WEAK_POINT_VIS4 and SPELL_WEAK_POINTS_BR.

    // Weak Points: 12 yard 100% proximity leg damage increase.
    SPELL_WEAK_POINTS_FR   = 123235, // Right, front.
    SPELL_WEAK_POINTS_FL   = 123423, // Left,  front.
    SPELL_WEAK_POINTS_BL   = 123426, // Left,  back.
    SPELL_WEAK_POINTS_BR   = 123428, // Right, back.

    // Weak Points: Visual triggers for Broken Leg animations. --! No scripting usage.
    SPELL_WEAK_POINT_VIS1  = 128599, // All these 4 spells trigger SPELL_BROKEN_LEG_VIS (one for each Leg).
    SPELL_WEAK_POINT_VIS2  = 128596,
    SPELL_WEAK_POINT_VIS3  = 128600,
    SPELL_WEAK_POINT_VIS4  = 128601,

    // Broken Leg: Boss 15% speed decrease stacking aura / 3 % HP damage spell. Stacks to 4 x 15% = 60% speed decrease.
    SPELL_BROKEN_LEG       = 122786, // Boss self-cast spell.

    // Broken Leg: Visual (triggered by spells from SPELL_WEAK_POINT_VIS) - "Blood" dripping out.
    SPELL_BROKEN_LEG_VIS   = 123500,

    // Mend Leg: Boss Leg heal spell. Used every 30 seconds after a leg dies.
    SPELL_MEND_LEG         = 123495  // Dummy, handled to "revive" the leg. Triggers 123796 script effect to remove SPELL_BROKEN_LEG from boss, we don't use it.
};

enum Events
{
    // Garalon
    EVENT_FURIOUS_SWIPE   = 1,       // About 8 - 11 seconds after pull. Every 8 seconds.
    EVENT_PHEROMONES,                // About 2 -  3 seconds after pull.
    EVENT_CRUSH,                     // About 30     seconds after pull. Every 37.5 seconds.
    EVENT_MEND_LEG,

    EVENT_BERSERK                    // Goes with SPELL_MASSIVE_CRASH.
};

enum Actions
{
    // Garalon
    ACTION_FUR_SWIPE_FAILED = 1,
    ACTION_PHEROMONES_JUMP_OR_PLAYERS_UNDERNEATH, // Normal Difficulty - Galaron casts Crush when Pheromones jump to another player / when he detects players underneath him.
    ACTION_LEG_IS_DEAD,

    // Garalon's Legs
    ACTION_LEG_DIED,
    ACTION_MEND_LEG          // Heal leg.
};

enum Creatures
{
    NPC_GARALON_LEG         = 63053, // 4 of them, 2 on each side.
    NPC_PHEROMONE_TRAIL     = 63021
};

class boss_garalon : public CreatureScript
{
public:
    boss_garalon() : CreatureScript("boss_garalon") { }

    struct boss_garalonAI : public BossAI
    {
        boss_garalonAI(Creature* creature) : BossAI(creature, DATA_GARALON_EVENT), vehicle(creature->GetVehicleKit()), summons(me)
        {
            ASSERT(vehicle); // Bah.
            instance = creature->GetInstanceScript();
        }

        InstanceScript* instance;
        Vehicle* vehicle;
        SummonList summons;
        EventMap events;
        bool damagedHeroic, castingCrush;

        void SummonAndAddLegs()
        {
            static uint32 LegSpells[4] =
            {
                SPELL_RIDE_FRONT_RIGHT, SPELL_RIDE_FRONT_LEFT, SPELL_RIDE_BACK_LEFT, SPELL_RIDE_BACK_RIGHT,
            };

            for (uint8 i = 0; i <= 3; ++i)
            {
                if (Creature* Leg = me->SummonCreature(NPC_GARALON_LEG, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), me->GetOrientation(), TEMPSUMMON_MANUAL_DESPAWN))
                {
                    Leg->CastSpell(me, LegSpells[i], true);
                    Leg->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
                    Leg->SetReactState(REACT_PASSIVE);
                }
            }
        }

        void DespawnCreatures(uint32 entry)
        {
            std::list<Creature*> creatures;
            GetCreatureListWithEntryInGrid(creatures, me, entry, 1000.0f);

            if (!creatures.empty())
                for (std::list<Creature*>::iterator iter = creatures.begin(); iter != creatures.end(); ++iter)
                    (*iter)->DespawnOrUnsummon();
        }

        void InitializeAI() OVERRIDE
        {
            if (!me->isDead())
                Reset();
        }

        void Reset() OVERRIDE
        {
            events.Reset();
            summons.DespawnAll();

            damagedHeroic = false;
            castingCrush  = false;

            if (instance)
                instance->SetData(DATA_GARALON_EVENT, NOT_STARTED);

            _Reset();

            SummonAndAddLegs(); // Add the legs to Garalon.
        }

        void EnterCombat(Unit* /*who*/) OVERRIDE
        {
            // First, make legs visible and attackable and set them in combat.
            for (uint8 i = 0; i <= 3; ++i)
                if (Unit* Leg = vehicle->GetPassenger(i))
                {
                    Leg->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
                    Leg->ClearUnitState(UNIT_STATE_ONVEHICLE);
                    Leg->ToCreature()->SetInCombatWithZone();

                    if (instance)
                        instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, Leg); // Add
                }

            me->AddAura(SPELL_CRUSH_BODY_VIS, me); // And add the body crush marker.

            events.ScheduleEvent(EVENT_FURIOUS_SWIPE, urand(8000, 11000));
            events.ScheduleEvent(EVENT_PHEROMONES, urand(2000, 3000));
            events.ScheduleEvent(EVENT_CRUSH, 30000); // First Crush always seems to have this timer, on any difficulty.
            events.ScheduleEvent(EVENT_BERSERK, 7 * MINUTE * IN_MILLISECONDS); // 7 min enrage timer.

            if (instance)
            {
                instance->SetData(DATA_GARALON_EVENT, IN_PROGRESS);
                instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me); // Add
            }

            _EnterCombat();
        }

        void EnterEvadeMode() OVERRIDE
        {
            // Remove all auras from the Legs and unset them in combat.
            for (uint8 i = 0; i <= 3; ++i)
                if (Unit* Leg = vehicle->GetPassenger(i))
                {
                    Leg->RemoveAllAuras();
                    Leg->DeleteThreatList();
                    Leg->CombatStop(false);

                    if (instance)
                        instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, Leg); // Remove
                }

            DespawnCreatures(NPC_PHEROMONE_TRAIL);
            me->RemoveAllAuras();
            Reset();
            me->DeleteThreatList();
            me->CombatStop(false);
            me->GetMotionMaster()->MoveTargetedHome();

            if (instance)
            {
                instance->SetData(DATA_GARALON_EVENT, FAIL);
                instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me); // Remove
                instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_PHEROMONES_AURA); // Remove Pheromones.
            }

            _EnterEvadeMode();
        }

        void JustDied(Unit* /*killer*/) OVERRIDE
        {
            summons.DespawnAll();
            DespawnCreatures(NPC_PHEROMONE_TRAIL);

            if (instance)
            {
                instance->SetData(DATA_GARALON_EVENT, DONE);
                instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me); // Remove
                instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_PHEROMONES_AURA); // Remove Pheromones.
            }

            _JustDied();
        }

        void JustSummoned(Creature* summon) OVERRIDE
        {
            summons.Summon(summon);
            summon->setActive(true);

			if (me->IsInCombat())
                summon->AI()->DoZoneInCombat();
        }

        void DoAction(int32 action) OVERRIDE
        {
            switch (action)
            {
                // Furious Swipe failed to hit at least 2 targets, Garalon gains Fury.
                case ACTION_FUR_SWIPE_FAILED:
                    Talk(ANN_FURY);
                    me->AddAura(SPELL_FURY, me);
                    break;

                // Pheromones jumped to another player / there are players underneath his body, in Normal Difficulty Garalon casts Crush.
                case ACTION_PHEROMONES_JUMP_OR_PLAYERS_UNDERNEATH:
                    if (!me->GetMap()->IsHeroic() && !castingCrush)
                    {
                        events.ScheduleEvent(EVENT_CRUSH, 3000);
                        castingCrush = true;
                    }
                    break;

                case ACTION_LEG_IS_DEAD:
                    events.ScheduleEvent(EVENT_MEND_LEG, 30000);
                    break;

                default: break;
            }
        }

        void UpdateAI(uint32 diff) OVERRIDE
        {
            if (!UpdateVictim() || me->HasUnitState(UNIT_STATE_CASTING))
                return;

            // Damaged debuff for Heroic.
            if (!damagedHeroic && me->HealthBelowPct(34) && me->GetMap()->IsHeroic())
            {
                Talk(ANN_DAMAGED);
                me->AddAura(SPELL_DAMAGED, me);
                damagedHeroic = true;
            }

            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
				{
                    case EVENT_FURIOUS_SWIPE:
                        DoCast(me, SPELL_FURIOUS_SWIPE);
                        events.ScheduleEvent(EVENT_FURIOUS_SWIPE, 8000);
                        break;

                    case EVENT_PHEROMONES:
                        DoCast(me, SPELL_PHER_INIT_CAST); // Spell script handles aura apply.
                        break;

                    case EVENT_CRUSH:
                        DoCast(me, SPELL_CRUSH);
                        if (me->GetMap()->IsHeroic()) // In Heroic Difficulty, Garalon periodically crushes foes.
                            events.ScheduleEvent(EVENT_CRUSH, 37500);
                        castingCrush = false;
                        break;

                    case EVENT_MEND_LEG:
                        DoCast(me, SPELL_MEND_LEG);
                        events.ScheduleEvent(EVENT_MEND_LEG, 30000);
                        break;

                    case EVENT_BERSERK:
                        Talk(ANN_BERSERK);
                        me->AddAura(SPELL_BERSERK, me);
                        DoCast(me, SPELL_MASSIVE_CRUSH);
                        break;

                    default: break;
				}
            }

            if (me->HasAura(SPELL_DAMAGED)) // Only on Heroic, and from 33% HP.
                DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const OVERRIDE
    {
        return new boss_garalonAI(creature);
    }
};

// Garalon's Leg: 63053.
class npc_garalon_leg : public CreatureScript
{
public:
	npc_garalon_leg() : CreatureScript("npc_garalon_leg") { }

	struct npc_garalon_legAI : public ScriptedAI
	{
		npc_garalon_legAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = creature->GetInstanceScript();
        }

        InstanceScript* instance;
        bool died;

        void IsSummonedBy(Unit* /*summoner*/) OVERRIDE
        {
            Reset();
        }

        void Reset() OVERRIDE
        {
            died = false;
        }

        void DamageTaken(Unit* /*who*/, uint32& damage) OVERRIDE
        {
            // Players cannot actually kill the legs, they damage them enough and they become unselectable etc.
            if (damage > me->GetHealth() - 1)
            {
                damage = me->GetHealth() - 1;

                // Players "kill" the leg.
                if (!died)
                {
                    DoAction(ACTION_LEG_DIED);
                    died = true;
                }
            }
        }

        void DoAction(int32 action) OVERRIDE
        {
            switch (action)
            {
                case ACTION_LEG_DIED:
                    me->AddAura(SPELL_BROKEN_LEG_VIS, me);
                    me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
                    me->AddUnitState(UNIT_STATE_ONVEHICLE);
                    if (Creature* Garalon = me->GetVehicleCreatureBase())
                        Garalon->ToCreature()->AI()->DoAction(ACTION_LEG_IS_DEAD);
                    break;

                case ACTION_MEND_LEG:
                    me->RemoveAurasDueToSpell(SPELL_BROKEN_LEG_VIS);
                    me->SetHealth(me->GetMaxHealth());
                    me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
                    me->ClearUnitState(UNIT_STATE_ONVEHICLE);
                    died = false;
                    break;

                default: break;
            }
        }

		void UpdateAI(uint32 diff) OVERRIDE { } // Override, no evade on !UpdateVictim() + no melee.
	};

	CreatureAI* GetAI(Creature* creature) const OVERRIDE
	{
		return new npc_garalon_legAI (creature);
	}
};

// Pheromone Trail: 63021.
class npc_pheromone_trail : public CreatureScript
{
public:
	npc_pheromone_trail() : CreatureScript("npc_pheromone_trail") { }

	struct npc_pheromone_trailAI : public ScriptedAI
	{
		npc_pheromone_trailAI(Creature* creature) : ScriptedAI(creature) { }

        void IsSummonedBy(Unit* /*summoner*/) OVERRIDE
        {
            me->SetInCombatWithZone();
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
            me->SetReactState(REACT_PASSIVE);

            me->AddAura(SPELL_PHER_TRAIL_DMG_A, me); // Damage aura.
            // me->DespawnOrUnsummon(30000);
        }
	};

	CreatureAI* GetAI(Creature* creature) const OVERRIDE
	{
		return new npc_pheromone_trailAI (creature);
	}
};

// Furious Swipe: 122735.
class spell_garalon_furious_swipe: public SpellScriptLoader
{
    public:
        spell_garalon_furious_swipe() : SpellScriptLoader("spell_garalon_furious_swipe") { }

        class spell_garalon_furious_swipeSpellScript: public SpellScript
        {
            PrepareSpellScript(spell_garalon_furious_swipeSpellScript);

            bool Validate(SpellInfo const* spell) OVERRIDE
            {
                if (!sSpellMgr->GetSpellInfo(spell->Id))
                    return false;

                return true;
            }

            bool Load()
            {
                return true;
            }

            void FilterTargets(std::list<WorldObject*>& targets)
            {
                // The target list size indicates how many players Garalon hits. We let him know what to do afterwards.
                if (targets.empty() || targets.size() < 2) // If he hits less than two players, it's time to go for Fury.
                    CAST_AI(boss_garalon::boss_garalonAI, GetCaster()->ToCreature()->AI())->DoAction(ACTION_FUR_SWIPE_FAILED);
            }

            void Register() OVERRIDE
            {
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_garalon_furious_swipeSpellScript::FilterTargets, EFFECT_0, TARGET_UNIT_CONE_ENEMY_104);
            }
        };

        SpellScript* GetSpellScript() const OVERRIDE
        {
            return new spell_garalon_furious_swipeSpellScript();
        }
};

// Pheromones (Force_Cast, 2 sec. cast time): 123808.
class spell_garalon_pheromones_forcecast: public SpellScriptLoader
{
    public:
        spell_garalon_pheromones_forcecast() : SpellScriptLoader("spell_garalon_pheromones_forcecast") { }

        class spell_garalon_pheromones_forcecastSpellScript: public SpellScript
        {
            PrepareSpellScript(spell_garalon_pheromones_forcecastSpellScript);

            bool Validate(SpellInfo const* spell) OVERRIDE
            {
                if (!sSpellMgr->GetSpellInfo(spell->Id))
                    return false;

                return true;
            }

            bool Load()
            {
                return true;
            }

            void FilterTargets(std::list<WorldObject*>& targets)
            {
                if (targets.empty())
                    return;

                // Find the nearest player in 100 yards, and that will be the target (done like that on off).
                WorldObject* target = GetCaster()->ToCreature()->SelectNearestPlayer(100.0f);

                targets.clear();
                targets.push_back(target);
            }

            void Register() OVERRIDE
            {
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_garalon_pheromones_forcecastSpellScript::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
            }
        };

        SpellScript* GetSpellScript() const OVERRIDE
        {
            return new spell_garalon_pheromones_forcecastSpellScript();
        }
};

// Mend Leg target check for Garalon's Legs.
class TargetCheck : public std::unary_function<Unit*, bool>
{
    public:
        explicit TargetCheck(Unit* _caster) : caster(_caster) { }

        bool operator()(WorldObject* object)
        {
            return object->GetEntry() != NPC_GARALON_LEG;
        }

    private:
        Unit* caster;
};

// Mend Leg: 123495.
class spell_garalon_mend_leg: public SpellScriptLoader
{
    public:
        spell_garalon_mend_leg() : SpellScriptLoader("spell_garalon_mend_leg") { }

        class spell_garalon_mend_legSpellScript: public SpellScript
        {
            PrepareSpellScript(spell_garalon_mend_legSpellScript);

            bool Validate(SpellInfo const* spell) OVERRIDE
            {
                if (!sSpellMgr->GetSpellInfo(spell->Id))
                    return false;

                return true;
            }

            bool Load()
            {
                return true;
            }

            void FilterTargets(std::list<WorldObject*>& targets)
            {
                if (targets.empty())
                    return;

                // We just need to filter the target entry, to check the legs only, as the spell already checks targets having SPELL_BROKEN_LEG_VIS.
                targets.remove_if(TargetCheck(GetCaster()));

                // Then, we just select one of the legs meeting the conditions and have him handle the dummy as GetHitUnit().
                WorldObject* target = Trinity::Containers::SelectRandomContainerElement(targets);

                targets.clear();
                targets.push_back(target);
            }

            void HandleDummy(SpellEffIndex effIndex)
            {
                if (!GetCaster() || !GetHitUnit()) return;

                // Now, once we made sure we are casting on a random broken leg, let's have it "respawn".
                GetHitUnit()->ToCreature()->AI()->DoAction(ACTION_MEND_LEG);

                // And remove a stack from Garalon's Broken Leg aura.
                if (GetCaster()->GetAura(SPELL_BROKEN_LEG)) // Just a crash check, this should always return true if a leg is broken.
                {
                    uint32 stack = GetCaster()->GetAura(SPELL_BROKEN_LEG)->GetStackAmount();

                    if (stack > 1) // If we have more stacks, remove 1.
                        GetCaster()->SetAuraStack(SPELL_BROKEN_LEG, GetCaster(), stack - 1);
                    else // One stack, remove the aura.
                        GetCaster()->RemoveAurasDueToSpell(SPELL_BROKEN_LEG);
                }
            }

            void HandleScriptEffect(SpellEffIndex effIndex)
            {
                if (!GetCaster() || !GetHitUnit()) return;

                // Effect 1 removes SPELL_BROKEN_LEG_VIS from the targeted leg with a trigger spell.
                // We do that from the leg script as the action implies more than just this.
                PreventHitDefaultEffect(effIndex);
            }

            void Register() OVERRIDE
            {
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_garalon_mend_legSpellScript::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
                OnEffectHit += SpellEffectFn(spell_garalon_mend_legSpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
                OnEffectLaunchTarget += SpellEffectFn(spell_garalon_mend_legSpellScript::HandleScriptEffect, EFFECT_1, SPELL_EFFECT_TRIGGER_SPELL);
            }
        };

        SpellScript* GetSpellScript() const OVERRIDE
        {
            return new spell_garalon_mend_legSpellScript();
        }
};

// Crush Trigger: 117709.
class spell_garalon_crush_trigger: public SpellScriptLoader
{
    public:
        spell_garalon_crush_trigger() : SpellScriptLoader("spell_garalon_crush_trigger") { }

        class spell_garalon_crush_triggerSpellScript: public SpellScript
        {
            PrepareSpellScript(spell_garalon_crush_triggerSpellScript);

            bool Validate(SpellInfo const* spell) OVERRIDE
            {
                if (!sSpellMgr->GetSpellInfo(spell->Id))
                    return false;

                return true;
            }

            bool Load()
            {
                return true;
            }

            void HandleDummy(SpellEffIndex effIndex)
            {
                // If there are no hit players, means there are no players underneath Garalon's body, so there's nothing to do.
                if (!GetCaster() || !GetHitUnit()) return;

                // Now, if there are players under Garalon, he will cast Crush.
                GetCaster()->ToCreature()->AI()->DoAction(ACTION_PHEROMONES_JUMP_OR_PLAYERS_UNDERNEATH);
            }

            void Register() OVERRIDE
            {
                OnEffectHitTarget += SpellEffectFn(spell_garalon_crush_triggerSpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const OVERRIDE
        {
            return new spell_garalon_crush_triggerSpellScript();
        }
};

// Target check for Pheromones  Taunt / Attack Me + Broken Leg spells.
class BossCheck : public std::unary_function<Unit*, bool>
{
    public:
        explicit BossCheck(Unit* _caster) : caster(_caster) { }

        bool operator()(WorldObject* object)
        {
            return object->GetEntry() != BOSS_GARALON;
        }

    private:
        Unit* caster;
};

// Pheromones Taunt: 123109.
class spell_garalon_pheromones_taunt: public SpellScriptLoader
{
    public:
        spell_garalon_pheromones_taunt() : SpellScriptLoader("spell_garalon_pheromones_taunt") { }

        class spell_garalon_pheromones_tauntSpellScript: public SpellScript
        {
            PrepareSpellScript(spell_garalon_pheromones_tauntSpellScript);

            bool Validate(SpellInfo const* spell) OVERRIDE
            {
                if (!sSpellMgr->GetSpellInfo(spell->Id))
                    return false;

                return true;
            }

            bool Load()
            {
                return true;
            }

            void FilterTargets(std::list<WorldObject*>& targets)
            {
                if (targets.empty())
                    return;

                // Only the boss gets taunted.
                targets.remove_if(BossCheck(GetCaster()));
            }

            void Register() OVERRIDE
            {
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_garalon_pheromones_tauntSpellScript::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_garalon_pheromones_tauntSpellScript::FilterTargets, EFFECT_1, TARGET_UNIT_SRC_AREA_ENTRY);
            }
        };

        SpellScript* GetSpellScript() const OVERRIDE
        {
            return new spell_garalon_pheromones_tauntSpellScript();
        }
};

// Broken Leg: 122786.
class spell_garalon_broken_leg : public SpellScriptLoader
{
    public:
        spell_garalon_broken_leg() : SpellScriptLoader("spell_garalon_broken_leg") { }

        class spell_garalon_broken_leg_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_garalon_broken_leg_SpellScript);

            bool Validate(SpellInfo const* spell) OVERRIDE
            {
                if (!sSpellMgr->GetSpellInfo(spell->Id))
                    return false;

                return true;
            }

            bool Load()
            {
                return true;
            }

            void FilterTargets(std::list<WorldObject*>& targets)
            {
                if (targets.empty())
                    return;

                // Only casted by boss on self.
                targets.remove_if(BossCheck(GetCaster()));
            }

            void Register() OVERRIDE
            {
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_garalon_broken_leg_SpellScript::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_garalon_broken_leg_SpellScript::FilterTargets, EFFECT_1, TARGET_UNIT_NEARBY_ENTRY);
            }
        };

        SpellScript* GetSpellScript() const OVERRIDE
        {
            return new spell_garalon_broken_leg_SpellScript();
        }
};

// Damaged: 123818
class spell_garalon_damaged : public SpellScriptLoader
{
    public:
        spell_garalon_damaged() : SpellScriptLoader("spell_garalon_damaged") { }

        class spell_garalon_damaged_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_garalon_damaged_SpellScript);

            bool Validate(SpellInfo const* spell) OVERRIDE
            {
                if (!sSpellMgr->GetSpellInfo(spell->Id))
                    return false;

                return true;
            }

            bool Load()
            {
                return true;
            }

            void HandleScript(SpellEffIndex /*effIndex*/)
            {
                if (!GetCaster() || !GetHitUnit()) return;

                // He becomes immune to the Pheromones Taunt / Attack Me spell.
                GetCaster()->ApplySpellImmune(0, IMMUNITY_ID, SPELL_PHEROMONES_TAUNT, true);
            }

            void Register() OVERRIDE
            {
                OnEffectHitTarget += SpellEffectFn(spell_garalon_damaged_SpellScript::HandleScript, EFFECT_0, SPELL_EFFECT_REMOVE_AURA);
            }
        };

        SpellScript* GetSpellScript() const OVERRIDE
        {
            return new spell_garalon_damaged_SpellScript();
        }
};

void AddSC_boss_garalon()
{
    new boss_garalon();
    new npc_garalon_leg();
    new npc_pheromone_trail();
    new spell_garalon_furious_swipe();
    new spell_garalon_pheromones_forcecast();
    new spell_garalon_mend_leg();
    new spell_garalon_crush_trigger();
    new spell_garalon_pheromones_taunt();
    new spell_garalon_broken_leg();
    new spell_garalon_damaged();
}
