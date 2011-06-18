/*
 * Copyright (C) 2008-2011 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "ulduar.h"

enum eSpells
{
    // Any boss
    SPELL_SUPERCHARGE                   = 61920,
    SPELL_BERSERK                       = 47008, // Hard enrage, don't know the correct ID.
    SPELL_CREDIT_MARKER                 = 65195, // spell_dbc

    // Steelbreaker
    SPELL_HIGH_VOLTAGE                  = 61890,
    SPELL_HIGH_VOLTAGE_H                = 63498,
    SPELL_FUSION_PUNCH                  = 61903,
    SPELL_FUSION_PUNCH_H                = 63493,
    SPELL_STATIC_DISRUPTION             = 44008,
    SPELL_STATIC_DISRUPTION_H           = 63494,
    SPELL_OVERWHELMING_POWER_H          = 61888,
    SPELL_OVERWHELMING_POWER            = 64637,
    SPELL_ELECTRICAL_CHARGE             = 61902,

    // Runemaster Molgeim
    SPELL_SHIELD_OF_RUNES               = 62274,
    SPELL_SHIELD_OF_RUNES_BUFF          = 62277,
    SPELL_SHIELD_OF_RUNES_H             = 63489,
    SPELL_SHIELD_OF_RUNES_H_BUFF        = 63967,
    SPELL_SUMMON_RUNE_OF_POWER          = 63513,
    SPELL_RUNE_OF_POWER                 = 61974,
    SPELL_RUNE_OF_DEATH                 = 62269,
    SPELL_RUNE_OF_SUMMONING             = 62273, // This is the spell that summons the rune
    SPELL_RUNE_OF_SUMMONING_VIS         = 62019, // Visual
    SPELL_RUNE_OF_SUMMONING_SUMMON      = 62020, // Spell that summons
    SPELL_LIGHTNING_ELEMENTAL_PASSIVE   = 62052,
    SPELL_LIGHTNING_ELEMENTAL_PASSIVE_H = 63492,

    // Stormcaller Brundir
    SPELL_CHAIN_LIGHTNING_N             = 61879,
    SPELL_CHAIN_LIGHTNING_H             = 63479,
    SPELL_OVERLOAD                      = 61869,
    SPELL_OVERLOAD_H                    = 63481,
    SPELL_LIGHTNING_WHIRL               = 61915,
    SPELL_LIGHTNING_WHIRL_H             = 63483,
    SPELL_LIGHTNING_TENDRILS            = 61887,
    SPELL_LIGHTNING_TENDRILS_H          = 63486,
    SPELL_STORMSHIELD                   = 64187
};

enum Events
{
    EVENT_UPDATEPHASE = 1,
    EVENT_ENRAGE,
    EVENT_PULSE,
    // Steelbreaker
    EVENT_FUSION_PUNCH,
    EVENT_STATIC_DISRUPTION,
    EVENT_OVERWHELMING_POWER,
    // Molgeim
    EVENT_RUNE_OF_POWER,
    EVENT_SHIELD_OF_RUNES,
    EVENT_RUNE_OF_DEATH,
    EVENT_RUNE_OF_SUMMONING,
    EVENT_LIGHTNING_BLAST,
    // Brundir
    EVENT_CHAIN_LIGHTNING,
    EVENT_OVERLOAD,
    EVENT_LIGHTNING_WHIRL,
    EVENT_LIGHTNING_TENDRILS_START,
    EVENT_LIGHTNING_TENDRILS_END,
    EVENT_THREAT_WIPE,
    EVENT_STORMSHIELD,
};

enum Yells
{
    SAY_STEELBREAKER_AGGRO                      = -1603020,
    SAY_STEELBREAKER_SLAY_1                     = -1603021,
    SAY_STEELBREAKER_SLAY_2                     = -1603022,
    SAY_STEELBREAKER_POWER                      = -1603023,
    SAY_STEELBREAKER_DEATH_1                    = -1603024,
    SAY_STEELBREAKER_DEATH_2                    = -1603025,
    SAY_STEELBREAKER_BERSERK                    = -1603026,

    SAY_MOLGEIM_AGGRO                           = -1603030,
    SAY_MOLGEIM_SLAY_1                          = -1603031,
    SAY_MOLGEIM_SLAY_2                          = -1603032,
    SAY_MOLGEIM_RUNE_DEATH                      = -1603033,
    SAY_MOLGEIM_SUMMON                          = -1603034,
    SAY_MOLGEIM_DEATH_1                         = -1603035,
    SAY_MOLGEIM_DEATH_2                         = -1603036,
    SAY_MOLGEIM_BERSERK                         = -1603037,

    SAY_BRUNDIR_AGGRO                           = -1603040,
    SAY_BRUNDIR_SLAY_1                          = -1603041,
    SAY_BRUNDIR_SLAY_2                          = -1603042,
    SAY_BRUNDIR_SPECIAL                         = -1603043,
    SAY_BRUNDIR_FLIGHT                          = -1603044,
    SAY_BRUNDIR_DEATH_1                         = -1603045,
    SAY_BRUNDIR_DEATH_2                         = -1603046,
    SAY_BRUNDIR_BERSERK                         = -1603047,
};

bool IsEncounterComplete(InstanceScript* instance, Creature* me)
{
   if (!instance || !me)
        return false;

    for (uint8 i = 0; i < 3; ++i)
    {
        uint64 guid = instance->GetData64(DATA_STEELBREAKER + i);
        if (!guid)
            return false;

        if (Creature* boss = Unit::GetCreature(*me, guid))
        {
            if (boss->isAlive())
                return false;
        }
        else
            return false;
    }
    return true;
}

void RespawnEncounter(InstanceScript* instance, Creature* me)
{
    for (uint8 i = 0; i < 3; ++i)
    {
        uint64 guid = instance->GetData64(DATA_STEELBREAKER + i);
        if (!guid)
            continue;

        if (Creature* boss = Unit::GetCreature(*me, guid))
        {
            if (!boss->isAlive())
            {
                boss->Respawn();
                boss->GetMotionMaster()->MoveTargetedHome();
            }
        }
    }
}

void StartEncounter(InstanceScript* instance, Creature* me, Unit* /*target*/)
{
    if (instance->GetBossState(TYPE_ASSEMBLY) == IN_PROGRESS)
        return;     // Prevent recursive calls

    instance->SetBossState(TYPE_ASSEMBLY, IN_PROGRESS);

    for (uint8 i = 0; i < 3; ++i)
    {
        uint64 guid = instance->GetData64(DATA_STEELBREAKER + i);
        if (!guid)
            continue;

        if (Creature* boss = Unit::GetCreature(*me, guid))
                boss->SetInCombatWithZone();
    }
}

bool UpdateSupercharge(Creature* target)
{
    if (Aura* supercharge = target->GetAura(SPELL_SUPERCHARGE))
    {
        supercharge->ModStackAmount(1);
        if (UnitAI* AI = target->GetAI())
        {
            AI->DoAction(EVENT_UPDATEPHASE);
            return true;
        }
    }

    return false;
}

class boss_steelbreaker : public CreatureScript
{
public:
    boss_steelbreaker() : CreatureScript("boss_steelbreaker") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new boss_steelbreakerAI(creature);
    }

    struct boss_steelbreakerAI : public ScriptedAI
    {
        boss_steelbreakerAI(Creature* c) : ScriptedAI(c)
        {
            pInstance = c->GetInstanceScript();
        }

        void Reset()
        {
            events.Reset();
            phase = 0;
            me->RemoveAllAuras();
            me->RemoveLootMode(LOOT_MODE_DEFAULT);
            if (pInstance)
            {
                pInstance->SetBossState(TYPE_ASSEMBLY, NOT_STARTED);
                RespawnEncounter(pInstance, me);
            }
        }

        EventMap events;
        InstanceScript* pInstance;
        uint32 phase;

        void EnterCombat(Unit* who)
        {
            StartEncounter(pInstance, me, who);
            DoScriptText(SAY_STEELBREAKER_AGGRO, me);
            DoZoneInCombat();
            DoCast(me, RAID_MODE(SPELL_HIGH_VOLTAGE, SPELL_HIGH_VOLTAGE_H));
            events.ScheduleEvent(EVENT_ENRAGE, 900000);
            events.ScheduleEvent(EVENT_PULSE, 5000);
            events.ScheduleEvent(EVENT_FUSION_PUNCH, 15000);
            DoAction(EVENT_UPDATEPHASE);
        }

        void DoAction(int32 const action)
        {
            switch (action)
            {
                case EVENT_UPDATEPHASE:
                    events.SetPhase(++phase);
                    if (phase == 2)
                        events.ScheduleEvent(EVENT_STATIC_DISRUPTION, 30000);
                    if (phase == 3)
                    {
                        me->ResetLootMode();
                        events.ScheduleEvent(EVENT_OVERWHELMING_POWER, rand()%5000);
                    }
                break;
            }
        }

        void DamageTaken(Unit* /*attacker*/, uint32 &damage)
        {
            if (damage >= me->GetHealth())
            {
                bool has_supercharge = false;

                if (Creature* Brundir = Unit::GetCreature(*me, pInstance ? pInstance->GetData64(DATA_BRUNDIR) : 0))
                {
                    if (Brundir->isAlive())
                    {
                        Brundir->SetFullHealth();
                        has_supercharge = UpdateSupercharge(Brundir);
                    }
                }

                if (Creature* Molgeim = Unit::GetCreature(*me, pInstance ? pInstance->GetData64(DATA_MOLGEIM) : 0))
                {
                    if (Molgeim->isAlive())
                    {
                        Molgeim->SetFullHealth();
                        has_supercharge = UpdateSupercharge(Molgeim);
                    }
                }

                if (!has_supercharge)
                    DoCast(SPELL_SUPERCHARGE);
            }
        }

        void JustDied(Unit* /*killer*/)
        {
            DoScriptText(RAND(SAY_STEELBREAKER_DEATH_1, SAY_STEELBREAKER_DEATH_2), me);
            if (IsEncounterComplete(pInstance, me) && pInstance)
            {
                pInstance->SetBossState(TYPE_ASSEMBLY, DONE);
                pInstance->DoUpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET, SPELL_CREDIT_MARKER);
            }
        }

        void KilledUnit(Unit * /*who*/)
        {
            DoScriptText(RAND(SAY_STEELBREAKER_SLAY_1, SAY_STEELBREAKER_SLAY_2), me);

            if (phase == 3)
                DoCast(me, SPELL_ELECTRICAL_CHARGE, true);
        }

        void SpellHit(Unit* /*from*/, const SpellEntry* spell)
        {
            if (spell->Id == SPELL_SUPERCHARGE)
                DoAction(EVENT_UPDATEPHASE);
        }

        void UpdateAI(uint32 const diff)
        {
            if (!UpdateVictim())
                return;

            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                    case EVENT_ENRAGE:
                        DoScriptText(SAY_STEELBREAKER_BERSERK, me);
                        DoCast(SPELL_BERSERK);
                        break;
                    case EVENT_PULSE:
                        if (me->getVictim() && me->getVictim()->ToPlayer())
                            DoAttackerGroupInCombat(me->getVictim()->ToPlayer());
                        events.ScheduleEvent(EVENT_PULSE, 5000);
                        break;
                    case EVENT_FUSION_PUNCH:
                        DoCastVictim(RAID_MODE(SPELL_FUSION_PUNCH, SPELL_FUSION_PUNCH_H));
                        events.ScheduleEvent(EVENT_FUSION_PUNCH, urand(13000, 22000));
                        break;
                    case EVENT_STATIC_DISRUPTION:
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM))
                            DoCast(target, RAID_MODE(SPELL_STATIC_DISRUPTION, SPELL_STATIC_DISRUPTION_H));
                        events.ScheduleEvent(EVENT_STATIC_DISRUPTION, urand(20000, 40000));
                        break;
                    case EVENT_OVERWHELMING_POWER:
                        DoScriptText(SAY_STEELBREAKER_POWER, me);
                        DoCastVictim(RAID_MODE(SPELL_OVERWHELMING_POWER, SPELL_OVERWHELMING_POWER_H));
                        events.ScheduleEvent(EVENT_OVERWHELMING_POWER, RAID_MODE(60000, 35000));
                        break;
                }
            }

            DoMeleeAttackIfReady();
        }
    };

};

class spell_meltdown : public SpellScriptLoader
{
    public:
        spell_meltdown() : SpellScriptLoader("spell_meltdown") { }

        class spell_meltdown_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_meltdown_SpellScript);

            bool Validate(SpellEntry const* /*spell*/)
            {
                if (!sSpellStore.LookupEntry(SPELL_ELECTRICAL_CHARGE))
                    return false;
                return true;
            }

            void TriggerElectricalCharge(SpellEffIndex /*effIndex*/)
            {
                if (Unit* target = GetHitUnit())
                    if (InstanceScript* instance = target->GetInstanceScript())
                        if (Creature* steelbreaker = ObjectAccessor::GetCreature(*target, instance->GetData64(DATA_STEELBREAKER)))
                            steelbreaker->CastSpell(steelbreaker, SPELL_ELECTRICAL_CHARGE, true);
            }

            void Register()
            {
                OnEffect += SpellEffectFn(spell_meltdown_SpellScript::TriggerElectricalCharge, EFFECT_1, SPELL_EFFECT_INSTAKILL);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_meltdown_SpellScript();
        }
};

class boss_runemaster_molgeim : public CreatureScript
{
public:
    boss_runemaster_molgeim() : CreatureScript("boss_runemaster_molgeim") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new boss_runemaster_molgeimAI(creature);
    }

    struct boss_runemaster_molgeimAI : public ScriptedAI
    {
        boss_runemaster_molgeimAI(Creature* c) : ScriptedAI(c)
        {
            pInstance = c->GetInstanceScript();
        }

        void Reset()
        {
            if (pInstance)
            {
                pInstance->SetBossState(TYPE_ASSEMBLY, NOT_STARTED);
                RespawnEncounter(pInstance, me);
            }

            events.Reset();
            me->RemoveAllAuras();
            me->RemoveLootMode(LOOT_MODE_DEFAULT);
            phase = 0;
        }

        InstanceScript* pInstance;
        EventMap events;
        uint32 phase;

        void EnterCombat(Unit* who)
        {
            StartEncounter(pInstance, me, who);
            DoScriptText(SAY_MOLGEIM_AGGRO, me);
            DoZoneInCombat();
            events.ScheduleEvent(EVENT_ENRAGE, 900000);
            events.ScheduleEvent(EVENT_PULSE, 5000);
            events.ScheduleEvent(EVENT_SHIELD_OF_RUNES, 27000);
            events.ScheduleEvent(EVENT_RUNE_OF_POWER, 60000);
            DoAction(EVENT_UPDATEPHASE);
        }

        void DoAction(int32 const action)
        {
            switch (action)
            {
                case EVENT_UPDATEPHASE:
                    events.SetPhase(++phase);
                    if (phase == 2)
                        events.ScheduleEvent(EVENT_RUNE_OF_DEATH, 30000);
                    if (phase == 3)
                    {
                        me->ResetLootMode();
                        events.ScheduleEvent(EVENT_RUNE_OF_SUMMONING, urand(20000, 30000));
                    }
                break;
            }
        }

        void DamageTaken(Unit* /*attacker*/, uint32 &damage)
        {
            if (damage >= me->GetHealth())
            {
                bool has_supercharge = false;

                if (Creature* Steelbreaker = Unit::GetCreature(*me, pInstance ? pInstance->GetData64(DATA_STEELBREAKER) : 0))
                {
                    if (Steelbreaker->isAlive())
                    {
                        Steelbreaker->SetFullHealth();
                        has_supercharge = UpdateSupercharge(Steelbreaker);
                    }
                }

                if (Creature* Brundir = Unit::GetCreature((*me), pInstance ? pInstance->GetData64(DATA_BRUNDIR) : 0))
                {
                    if (Brundir->isAlive())
                    {
                        Brundir->SetFullHealth();
                        has_supercharge = UpdateSupercharge(Brundir);
                    }
                }

                if (!has_supercharge)
                    DoCast(me, SPELL_SUPERCHARGE);
            }
        }

        void JustDied(Unit* /*killer*/)
        {
            DoScriptText(RAND(SAY_MOLGEIM_DEATH_1, SAY_MOLGEIM_DEATH_2), me);
            if (IsEncounterComplete(pInstance, me) && pInstance)
            {
                pInstance->SetBossState(TYPE_ASSEMBLY, DONE);
                pInstance->DoUpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET, SPELL_CREDIT_MARKER);
            }
        }

        void KilledUnit(Unit* /*who*/)
        {
            DoScriptText(RAND(SAY_MOLGEIM_SLAY_1, SAY_MOLGEIM_SLAY_2), me);
        }

        void SpellHit(Unit* /*from*/, const SpellEntry* spell)
        {
            if (spell->Id == SPELL_SUPERCHARGE)
                DoAction(EVENT_UPDATEPHASE);
        }

        void UpdateAI(uint32 const diff)
        {
            if (!UpdateVictim())
                return;

            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                    case EVENT_ENRAGE:
                        DoScriptText(SAY_MOLGEIM_BERSERK, me);
                        DoCast(SPELL_BERSERK);
                        break;
                    case EVENT_PULSE:
                        if (me->getVictim() && me->getVictim()->ToPlayer())
                            DoAttackerGroupInCombat(me->getVictim()->ToPlayer());
                        events.ScheduleEvent(EVENT_PULSE, 5000);
                        break;
                    case EVENT_RUNE_OF_POWER: // Improve target selection; random alive friendly
                    {
                        Unit* target = DoSelectLowestHpFriendly(60);
                        if (!target || !target->isAlive())
                            target = me;
                        DoCast(target, SPELL_SUMMON_RUNE_OF_POWER);
                        events.ScheduleEvent(EVENT_RUNE_OF_POWER, 60000);
                        break;
                    }
                    case EVENT_SHIELD_OF_RUNES:
                        DoCast(me, RAID_MODE(SPELL_SHIELD_OF_RUNES, SPELL_SHIELD_OF_RUNES_H));
                        events.ScheduleEvent(EVENT_SHIELD_OF_RUNES, urand(27000, 34000));
                        break;
                    case EVENT_RUNE_OF_DEATH:
                        DoScriptText(SAY_MOLGEIM_RUNE_DEATH, me);
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM))
                            DoCast(target, SPELL_RUNE_OF_DEATH);
                        events.ScheduleEvent(EVENT_RUNE_OF_DEATH, urand(30000, 40000));
                        break;
                    case EVENT_RUNE_OF_SUMMONING:
                        DoScriptText(SAY_MOLGEIM_SUMMON, me);
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM))
                            DoCast(target, SPELL_RUNE_OF_SUMMONING);
                        events.ScheduleEvent(EVENT_RUNE_OF_SUMMONING, urand(20000, 30000));
                        break;
                }
            }

            DoMeleeAttackIfReady();
        }
    };
};

class mob_rune_of_power : public CreatureScript
{
public:
    mob_rune_of_power() : CreatureScript("mob_rune_of_power") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new mob_rune_of_powerAI(creature);
    }

    struct mob_rune_of_powerAI : public ScriptedAI
    {
        mob_rune_of_powerAI(Creature* c) : ScriptedAI(c)
        {
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
            me->setFaction(16); // Same faction as bosses
            DoCast(SPELL_RUNE_OF_POWER);

            me->DespawnOrUnsummon(60000);
        }
    };
};

class mob_lightning_elemental : public CreatureScript
{
public:
    mob_lightning_elemental() : CreatureScript("mob_lightning_elemental") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new mob_lightning_elementalAI(creature);
    }

    struct mob_lightning_elementalAI : public ScriptedAI
    {
        mob_lightning_elementalAI(Creature* c) : ScriptedAI(c)
        {
            me->SetInCombatWithZone();
            me->AddAura(RAID_MODE(SPELL_LIGHTNING_ELEMENTAL_PASSIVE, SPELL_LIGHTNING_ELEMENTAL_PASSIVE_H), me);
        }

        // Nothing to do here, just let the creature chase players and procflags == 2 on the applied aura will trigger explosion
    };
};

class mob_rune_of_summoning : public CreatureScript
{
public:
    mob_rune_of_summoning() : CreatureScript("mob_rune_of_summoning") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new mob_rune_of_summoningAI(creature);
    }

    struct mob_rune_of_summoningAI : public ScriptedAI
    {
        mob_rune_of_summoningAI(Creature* c) : ScriptedAI(c)
        {
            me->AddAura(SPELL_RUNE_OF_SUMMONING_VIS, me);
            summonCount = 0;
            summonTimer = 2000;
        }

        uint32 summonCount;
        uint32 summonTimer;

        void UpdateAI(uint32 const diff)
        {
            if (summonTimer <= diff)
                SummonLightningElemental();
            else
                summonTimer -= diff;
        }

        void SummonLightningElemental()
        {
            me->CastSpell(me, SPELL_RUNE_OF_SUMMONING_SUMMON, false);
            if (++summonCount == 10)                        // TODO: Find out if this amount is right
                me->DespawnOrUnsummon();
            else
                summonTimer = 2000;                         // TODO: Find out of timer is right
        }
    };
};

class boss_stormcaller_brundir : public CreatureScript
{
public:
    boss_stormcaller_brundir() : CreatureScript("boss_stormcaller_brundir") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new boss_stormcaller_brundirAI(creature);
    }

    struct boss_stormcaller_brundirAI : public ScriptedAI
    {
        boss_stormcaller_brundirAI(Creature* c) : ScriptedAI(c)
        {
            pInstance = c->GetInstanceScript();
        }

        void Reset()
        {
            if (pInstance)
            {
                pInstance->SetBossState(TYPE_ASSEMBLY, NOT_STARTED);
                RespawnEncounter(pInstance, me);
            }

            me->RemoveAllAuras();
            me->RemoveLootMode(LOOT_MODE_DEFAULT);
            events.Reset();
            phase = 0;
        }

        EventMap events;
        InstanceScript* pInstance;
        uint32 phase;

        void EnterCombat(Unit* who)
        {
            StartEncounter(pInstance, me, who);
            DoScriptText(SAY_BRUNDIR_AGGRO, me);
            DoZoneInCombat();
            events.ScheduleEvent(EVENT_ENRAGE, 900000);
            events.ScheduleEvent(EVENT_PULSE, 5000);
            events.ScheduleEvent(EVENT_CHAIN_LIGHTNING, urand(9000, 17000), 1);
            events.ScheduleEvent(EVENT_OVERLOAD, urand(60000, 80000), 1);
            DoAction(EVENT_UPDATEPHASE);
        }

        void DoAction(int32 const action)
        {
            switch (action)
            {
                case EVENT_UPDATEPHASE:
                    events.SetPhase(++phase);
                    if (phase == 2)
                        events.ScheduleEvent(EVENT_LIGHTNING_WHIRL, urand(20000, 40000), 1);
                    if (phase == 3)
                    {
                        me->ResetLootMode();
                        DoCast(me, SPELL_STORMSHIELD, true);
                        events.ScheduleEvent(EVENT_LIGHTNING_TENDRILS_START, urand(40000, 80000));
                    }
                break;

            }
        }

        void DamageTaken(Unit* /*attacker*/, uint32 &damage)
        {
            if (damage >= me->GetHealth())
            {
                bool has_supercharge = false;

                if (Creature* Steelbreaker = Unit::GetCreature(*me, pInstance ? pInstance->GetData64(DATA_STEELBREAKER) : 0))
                {
                    if (Steelbreaker->isAlive())
                    {
                        Steelbreaker->SetFullHealth();
                        has_supercharge = UpdateSupercharge(Steelbreaker);
                    }
                }

                if (Creature* Molgeim = Unit::GetCreature(*me, pInstance ? pInstance->GetData64(DATA_MOLGEIM) : 0))
                {
                    if (Molgeim->isAlive())
                    {
                        Molgeim->SetFullHealth();
                        has_supercharge = UpdateSupercharge(Molgeim);
                    }
                }

                if (!has_supercharge)
                    DoCast(SPELL_SUPERCHARGE);
            }
        }

        void JustDied(Unit* /*killer*/)
        {
            DoScriptText(RAND(SAY_BRUNDIR_DEATH_1, SAY_BRUNDIR_DEATH_2), me);
            if (IsEncounterComplete(pInstance, me) && pInstance)
            {
                pInstance->SetBossState(TYPE_ASSEMBLY, DONE);
                pInstance->DoUpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET, SPELL_CREDIT_MARKER);
            }
        }

        void KilledUnit(Unit* /*who*/)
        {
            DoScriptText(RAND(SAY_BRUNDIR_SLAY_1, SAY_BRUNDIR_SLAY_2), me);
        }

        void SpellHit(Unit* /*from*/, const SpellEntry *spell)
        {
            if (spell->Id == SPELL_SUPERCHARGE)
                DoAction(EVENT_UPDATEPHASE);
        }

        void UpdateAI(uint32 const diff)
        {
            if (!UpdateVictim())
                return;

            events.Update(diff);

            if (me->HasUnitState(UNIT_STAT_CASTING))
                return;

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch(eventId)
                {
                    case EVENT_ENRAGE:
                        DoScriptText(SAY_BRUNDIR_BERSERK, me);
                        DoCast(SPELL_BERSERK);
                        break;
                    case EVENT_PULSE:
                        if (me->getVictim() && me->getVictim()->ToPlayer())
                            DoAttackerGroupInCombat(me->getVictim()->ToPlayer());
                        events.ScheduleEvent(EVENT_PULSE, 5000);
                        break;
                    case EVENT_CHAIN_LIGHTNING:
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM))
                            DoCast(target, RAID_MODE(SPELL_CHAIN_LIGHTNING_N, SPELL_CHAIN_LIGHTNING_H));
                        events.ScheduleEvent(EVENT_CHAIN_LIGHTNING, urand(3000, 5000), 1);
                        break;
                    case EVENT_OVERLOAD:
                        DoCast(RAID_MODE(SPELL_OVERLOAD, SPELL_OVERLOAD_H));
                        events.ScheduleEvent(EVENT_OVERLOAD, urand(60000, 80000), 1);
                        break;
                    case EVENT_LIGHTNING_WHIRL:
                        DoCast(RAID_MODE(SPELL_LIGHTNING_WHIRL, SPELL_LIGHTNING_WHIRL_H));
                        events.ScheduleEvent(EVENT_LIGHTNING_WHIRL, urand(20000, 40000), 1);
                        break;
                    case EVENT_THREAT_WIPE:
                        DoResetThreat();
                        events.ScheduleEvent(EVENT_THREAT_WIPE, 5000);
                        break;
                    case EVENT_LIGHTNING_TENDRILS_START:
                        me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_TAUNT, true);
                        me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_ATTACK_ME, true);
                        DoCast(RAID_MODE(SPELL_LIGHTNING_TENDRILS, SPELL_LIGHTNING_TENDRILS_H));
                        me->AddUnitMovementFlag(MOVEMENTFLAG_LEVITATING);
                        me->SendMovementFlagUpdate();
                        events.DelayEvents(35000, 1);
                        events.ScheduleEvent(EVENT_LIGHTNING_TENDRILS_END, 30000);
                        events.ScheduleEvent(EVENT_THREAT_WIPE, 0);
                        break;
                    case EVENT_LIGHTNING_TENDRILS_END:
                        me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_TAUNT, false);
                        me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_ATTACK_ME, false);
                        me->RemoveUnitMovementFlag(MOVEMENTFLAG_LEVITATING);
                        me->SendMovementFlagUpdate();
                        me->RemoveAurasDueToSpell(RAID_MODE(SPELL_LIGHTNING_TENDRILS, SPELL_LIGHTNING_TENDRILS_H));
                        events.ScheduleEvent(EVENT_LIGHTNING_TENDRILS_START, urand(40000, 80000));
                        events.CancelEvent(EVENT_THREAT_WIPE);
                        break;
                }
            }

            if (!me->HasAura(RAID_MODE(SPELL_LIGHTNING_TENDRILS, SPELL_LIGHTNING_TENDRILS_H)))
                DoMeleeAttackIfReady();
        }
    };

};

class spell_shield_of_runes : public SpellScriptLoader
{
    public:
        spell_shield_of_runes() : SpellScriptLoader("spell_shield_of_runes") { }

        class spell_shield_of_runes_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_shield_of_runes_AuraScript);

            void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
            {
                if (Unit* caster = GetCaster())
                    if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
                        caster->CastSpell(caster, SPELL_SHIELD_OF_RUNES_BUFF, false);
            }

            void Register()
            {
                 AfterEffectRemove += AuraEffectRemoveFn(spell_shield_of_runes_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_shield_of_runes_AuraScript();
        }
};

void AddSC_boss_assembly_of_iron()
{
    new boss_steelbreaker();
    new spell_meltdown();
    new boss_runemaster_molgeim();
    new boss_stormcaller_brundir();
    new mob_lightning_elemental();
    new mob_rune_of_summoning();
    new mob_rune_of_power();
    new spell_shield_of_runes();
}