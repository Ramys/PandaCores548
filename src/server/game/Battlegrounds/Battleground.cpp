/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
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

#include "Chat.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "Creature.h"
#include "Formulas.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Language.h"
#include "MapManager.h"
#include "Object.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "Util.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Bracket.h"
#include "BracketMgr.h"
#include "GroupMgr.h"


   uint64 CustomGUID; 
   uint32 MORPH_CUSTOM;
   uint32 SPELL_CUSTOM;
   uint32 spell_custom_2;
   uint32 spell_end;
   uint32 LANG_SELECT_ATTACK;
   uint32 LANG_SELECT_DEF;
   uint32 checktimer;
   std::string TEXT;
   const char* notification;
   std::string NewChar;
   bool custom_exists;
   Creature* ball;
   bool end;

namespace Trinity
{
    class BattlegroundChatBuilder
    {
        public:
            BattlegroundChatBuilder(ChatMsg msgtype, int32 textId, Player const* source, va_list* args = NULL)
                : _msgtype(msgtype), _textId(textId), _source(source), _args(args) { }

            void operator()(WorldPacket& data, LocaleConstant loc_idx)
            {
                char const* text = sObjectMgr->GetTrinityString(_textId, loc_idx);
                if (_args)
                {
                    // we need copy va_list before use or original va_list will corrupted
                    va_list ap;
                    va_copy(ap, *_args);

                    char str[2048];
                    vsnprintf(str, 2048, text, ap);
                    va_end(ap);

                    do_helper(data, &str[0]);
                }
                else
                    do_helper(data, text);
            }

        private:
            void do_helper(WorldPacket& data, char const* text)
            {
                Trinity::ChatData c;
                c.targetGuid = _source ? _source->GetGUID() : 0;
                c.sourceGuid = _source ? _source->GetGUID() : 0;
                c.chatTag = _source ? _source->GetChatTag() : 0;
                c.message = text;
                c.chatType = _msgtype;

                Trinity::BuildChatPacket(data, c);
            }

            ChatMsg _msgtype;
            int32 _textId;
            Player const* _source;
            va_list* _args;
    };

    class Battleground2ChatBuilder
    {
        public:
            Battleground2ChatBuilder(ChatMsg msgtype, int32 textId, Player const* source, int32 arg1, int32 arg2)
                : _msgtype(msgtype), _textId(textId), _source(source), _arg1(arg1), _arg2(arg2) {}

            void operator()(WorldPacket& data, LocaleConstant loc_idx)
            {
                char const* text = sObjectMgr->GetTrinityString(_textId, loc_idx);
                char const* arg1str = _arg1 ? sObjectMgr->GetTrinityString(_arg1, loc_idx) : "";
                char const* arg2str = _arg2 ? sObjectMgr->GetTrinityString(_arg2, loc_idx) : "";

                char str[2048];
                snprintf(str, 2048, text, arg1str, arg2str);

                Trinity::ChatData c;
                c.targetGuid = _source ? _source->GetGUID() : 0;
                c.sourceGuid = _source ? _source->GetGUID() : 0;
                c.chatTag = _source ? _source->GetChatTag() : 0;
                c.message = str;
                c.chatType = _msgtype;

                Trinity::BuildChatPacket(data, c);
            }

        private:
            ChatMsg _msgtype;
            int32 _textId;
            Player const* _source;
            int32 _arg1;
            int32 _arg2;
    };
}                                                           // namespace Trinity

template<class Do>
void Battleground::BroadcastWorker(Do& _do)
{
    for (BattlegroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
        if (Player* player = ObjectAccessor::FindPlayer(MAKE_NEW_GUID(itr->first, 0, HIGHGUID_PLAYER)))
            _do(player);
}

Battleground::Battleground()
{
    m_Guid              = MAKE_NEW_GUID(m_TypeID, 0, HIGHGUID_BATTLEGROUND);
    m_TypeID            = BATTLEGROUND_TYPE_NONE;
    m_RandomTypeID      = BATTLEGROUND_TYPE_NONE;
    m_InstanceID        = 0;
    m_Status            = STATUS_NONE;
    m_ClientInstanceID  = 0;
    m_EndTime           = 0;
    m_LastResurrectTime = 0;
    m_BracketId         = BG_BRACKET_ID_FIRST;
    m_InvitedAlliance   = 0;
    m_InvitedHorde      = 0;
    m_JoinType         = 0;
    m_IsArena           = false;
    m_needFirstUpdateVision  = true;
    m_needSecondUpdateVision = true;
    m_Winner            = 2;
    m_StartTime         = 0;
    m_ResetStatTimer    = 0;
    m_ValidStartPositionTimer = 0;
    m_Events            = 0;
    m_IsRated           = false;
    m_BuffChange        = false;
    m_Name              = "";
    m_LevelMin          = 0;
    m_LevelMax          = 0;
    m_InBGFreeSlotQueue = false;
    m_SetDeleteThis     = false;

    m_MaxPlayersPerTeam = 0;
    m_MaxPlayers        = 0;
    m_MinPlayersPerTeam = 0;
    m_MinPlayers        = 0;

    m_MapId             = 0;
    m_Map               = NULL;

    m_TeamStartLocX[BG_TEAM_ALLIANCE]   = 0;
    m_TeamStartLocX[BG_TEAM_HORDE]      = 0;

    m_TeamStartLocY[BG_TEAM_ALLIANCE]   = 0;
    m_TeamStartLocY[BG_TEAM_HORDE]      = 0;

    m_TeamStartLocZ[BG_TEAM_ALLIANCE]   = 0;
    m_TeamStartLocZ[BG_TEAM_HORDE]      = 0;

    m_TeamStartLocO[BG_TEAM_ALLIANCE]   = 0;
    m_TeamStartLocO[BG_TEAM_HORDE]      = 0;

    m_GroupIds[BG_TEAM_ALLIANCE]   = 0;
    m_GroupIds[BG_TEAM_HORDE]      = 0;

    m_StartMaxDist = 0.0f;
    m_holiday = 0;

    m_BgRaids[BG_TEAM_ALLIANCE]         = NULL;
    m_BgRaids[BG_TEAM_HORDE]            = NULL;

    m_PlayersCount[BG_TEAM_ALLIANCE]    = 0;
    m_PlayersCount[BG_TEAM_HORDE]       = 0;

    m_TeamScores[BG_TEAM_ALLIANCE]      = 0;
    m_TeamScores[BG_TEAM_HORDE]         = 0;

    m_PrematureCountDown = false;

    m_HonorMode = BG_NORMAL;

    StartDelayTimes[BG_STARTING_EVENT_FIRST]  = BG_START_DELAY_2M;
    StartDelayTimes[BG_STARTING_EVENT_SECOND] = BG_START_DELAY_1M;
    StartDelayTimes[BG_STARTING_EVENT_THIRD]  = BG_START_DELAY_30S;
    StartDelayTimes[BG_STARTING_EVENT_FOURTH] = BG_START_DELAY_NONE;
    //we must set to some default existing values
    StartMessageIds[BG_STARTING_EVENT_FIRST]  = LANG_BG_WS_START_TWO_MINUTES;
    StartMessageIds[BG_STARTING_EVENT_SECOND] = LANG_BG_WS_START_ONE_MINUTE;
    StartMessageIds[BG_STARTING_EVENT_THIRD]  = LANG_BG_WS_START_HALF_MINUTE;
    StartMessageIds[BG_STARTING_EVENT_FOURTH] = LANG_BG_WS_HAS_BEGUN;

    m_IsRBG = false;

    isRevival = false;

    m_sameBgTeamId = false;
    
    m_flagCarrierTime = FLAGS_UPDATE;

    m_StartDelayTime = 0;
    
    // for custom BG event
    spell_custom_2 = 46392;
    custom_exists = false;
    CustomGUID = 0;
    checktimer = 2000;
    switch(urand(1, 4))
    {
       case 1:
             SPELL_CUSTOM = 600000;
             MORPH_CUSTOM = 49585;
             spell_end = 146756;
             TEXT = "Я Гаррош, сын Грома, покажу вам, что значит быть АДСКИМ КРИКОМ!";
             notification = "Вы стали новым Гаррошем! Осколок Кровавого Воя Вы можете найти в своем рюкзаке. За каждое убийство Вы будете получать валюту.";
             LANG_SELECT_DEF = 30000;
             LANG_SELECT_ATTACK = 30001;
             NewChar = "%s стал новым Гаррошем!!";
             break;
       case 2:
             SPELL_CUSTOM = 600001;
             MORPH_CUSTOM = 46770;
             spell_end = 137557;
             TEXT = "Отведайте электричества!!!";
             notification = "Вы стали новым Лэй Шенем! Осколок Боевого Топора Властелина Грома Вы можете найти в своем рюкзаке. За каждое убийство Вы будете получать валюту.";
             LANG_SELECT_DEF = 30002;
             LANG_SELECT_ATTACK = 30003;       
             NewChar = "%s стал новым Лэй Шенем!!";
             break; 
       case 3:
             SPELL_CUSTOM = 600002;
             MORPH_CUSTOM = 35435;
             spell_end = 137204;
             TEXT = "Наконец-то я могу поработатить этот мир! И начну я с Вас!";
             notification = "Вы стали новым Смертокрылом! Фрагмент челюсти Смертокрыла Вы можете найти в своем рюкзаке. За каждое убийство Вы будете получать валюту.";
             LANG_SELECT_DEF = 30004;
             LANG_SELECT_ATTACK = 30005;   
             NewChar = "%s стал новым Смертокрылом!!";
             break;
       case 4:
             SPELL_CUSTOM = 600003;
             MORPH_CUSTOM = 39183;
             spell_end = 137204;
             TEXT = "Вам не устоять перед моей Демонической сущностью!";
             notification = "Вы стали новым Иллиданом ! Осколок Боевого Клинка Аззинота Вы можете найти в своем рюкзаке. За каждое убийство Вы будете получать валюту.";
             LANG_SELECT_DEF = 30006;
             LANG_SELECT_ATTACK = 30007;  
             NewChar = "%s стал новым Иллиданом!";
             break;
    }    
}

Battleground::~Battleground()
{
    // remove objects and creatures
    // (this is done automatically in mapmanager update, when the instance is reset after the reset time)
    uint32 size = uint32(BgCreatures.size());
    for (uint32 i = 0; i < size; ++i)
        DelCreature(i);

    size = uint32(BgObjects.size());
    for (uint32 i = 0; i < size; ++i)
        DelObject(i);

    sBattlegroundMgr->RemoveBattleground(GetInstanceID(), GetTypeID());
    // unload map
    if (m_Map)
    {
        m_Map->SetUnload();
        //unlink to prevent crash, always unlink all pointer reference before destruction
        m_Map->SetBG(NULL);
        m_Map = NULL;
    }
    // remove from bg free slot queue
    RemoveFromBGFreeSlotQueue();

    for (BattlegroundScoreMap::const_iterator itr = PlayerScores.begin(); itr != PlayerScores.end(); ++itr)
        delete itr->second;
}

void Battleground::Update(uint32 diff)
{
    if (!PreUpdateImpl(diff))
        return;

    if (!GetPlayersSize())
    {
        //BG is empty
        // if there are no players invited, delete BG
        // this will delete arena or bg object, where any player entered
        // [[   but if you use battleground object again (more battles possible to be played on 1 instance)
        //      then this condition should be removed and code:
        //      if (!GetInvitedCount(HORDE) && !GetInvitedCount(ALLIANCE))
        //          this->AddToFreeBGObjectsQueue(); // not yet implemented
        //      should be used instead of current
        // ]]
        // Battleground Template instance cannot be updated, because it would be deleted
        if (!GetInvitedCount(HORDE) && !GetInvitedCount(ALLIANCE))
            m_SetDeleteThis = true;
        return;
    }

    switch (GetStatus())
    {
        case STATUS_WAIT_JOIN:
        {
            if (GetPlayersSize())
                _ProcessJoin(diff);
            break;
        }
        case STATUS_IN_PROGRESS:
        {
            uint8 dumpeningTime = m_JoinType == 2 ? 6 : 11;

            _ProcessOfflineQueue();
            // after 47 minutes without one team losing, the arena closes with no winner and no rating change
            if (isArena())
            {
                if (m_StartTime >= 60200 && m_needSecondUpdateVision)
                {
                    UpdateArenaVision();
                    m_needSecondUpdateVision = false;
                }
                if (m_needFirstUpdateVision)
                {
                    UpdateArenaVision();
                    m_needFirstUpdateVision = false;
                }

                //! Patch 5.4: If neither team has won after 20 minutes, the Arena match will end in a draw.
                if (GetElapsedTime() >= 20 * MINUTE*IN_MILLISECONDS)
                {
                    UpdateArenaWorldState();
                    CheckArenaAfterTimerConditions();
                    return;
                    //! Patch 5.4: For Arena matches that last more than 10 minutes, all players in the Arena will begin to receive Dampening.
                    //! Patch 5.4.7: Dampening is now applied to an Arena match starting at the 5 minute mark (down from 10 minutes).
                }
                else if (GetElapsedTime() >= uint32(dumpeningTime * MINUTE * IN_MILLISECONDS))
                {
                    ModifyStartDelayTime(diff);
                    if (GetStartDelayTime() <= 0 || GetStartDelayTime() > 10000)
                    {
                        SetStartDelayTime(10000);
                        for (BattlegroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
                            if (Player* player = sObjectAccessor->FindPlayer(itr->first))
                                if (!player->HasAura(SPELL_ARENA_DUMPENING))
                                    player->CastSpell(player, SPELL_ARENA_DUMPENING, true);
                    }
                }
            }
            else
            {
                SendFlagsPositionsUpdate(diff);
                _ProcessRessurect(diff);
                if (sBattlegroundMgr->GetPrematureFinishTime() && (GetPlayersCountByTeam(ALLIANCE) < GetMinPlayersPerTeam() || GetPlayersCountByTeam(HORDE) < GetMinPlayersPerTeam()))
                    _ProcessProgress(diff);
                else if (m_PrematureCountDown)
                    m_PrematureCountDown = false;
            }
            break;
        }
        case STATUS_WAIT_LEAVE:
            _ProcessLeave(diff);
            break;
        default:
            break;
    }

    // Update start time and reset stats timer
    SetElapsedTime(GetElapsedTime() + diff);
    if (GetStatus() == STATUS_WAIT_JOIN)
    {
        m_ResetStatTimer += diff;
    }
    m_ValidStartPositionTimer += diff;

    PostUpdateImpl(diff);
    
    //customs update morph and scale, else can bug
    if (isBattleground() && sWorld->getBoolConfig(CONFIG_CUSTOM_BATTLEGROUND))
    { 
      if (checktimer <= diff)
      {
         for (BattlegroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
             if (Player* player = sObjectAccessor->FindPlayer(itr->first))
                if (player->GetGUID() == CustomGUID)
                {
                   if (!player->IsMounted())
                   {
                     player->SetDisplayId(MORPH_CUSTOM, true);
                     player->SetCustomDisplayId(MORPH_CUSTOM); 
                   }
                }
         checktimer = 2000; //for small lags
      }
      else checktimer -= diff;
                
    }
   if (GetJoinType() == 5 && sWorld->getBoolConfig(CONFIG_CUSTOM_FOOTBALL))  
   {
      if (ball && !end)
      {
         if (ball->GetAuraCount(58169) >= 3)
         {
            EndBattleground(HORDE);
            end = true;
         }
         if (ball->GetAuraCount(65964) >= 3)
         {
            EndBattleground(ALLIANCE);
            end = true;
         }
      }
   }
}

inline void Battleground::_ProcessOfflineQueue()
{
    // remove offline players from bg after 5 minutes
    if (!m_OfflineQueue.empty())
    {
        BattlegroundPlayerMap::iterator itr = m_Players.find(*(m_OfflineQueue.begin()));
        if (itr != m_Players.end())
        {
            if (itr->second.OfflineRemoveTime <= sWorld->GetGameTime())
            {
                RemovePlayerAtLeave(itr->first, true, true);// remove player from BG
                m_OfflineQueue.pop_front();                 // remove from offline queue
                //do not use itr for anything, because it is erased in RemovePlayerAtLeave()
            }
        }
    }

}

inline void Battleground::_ProcessRessurect(uint32 diff)
{
    // *********************************************************
    // ***        BATTLEGROUND RESSURECTION SYSTEM           ***
    // *********************************************************
    // this should be handled by spell system
    m_LastResurrectTime += diff;
    if (m_LastResurrectTime >= RESURRECTION_INTERVAL)
    {
        if (GetReviveQueueSize())
        {
            isRevival = true;
            for (std::map<uint64, std::vector<uint64> >::iterator itr = m_ReviveQueue.begin(); itr != m_ReviveQueue.end(); ++itr)
            {
                Creature* sh = NULL;
                for (std::vector<uint64>::const_iterator itr2 = (itr->second).begin(); itr2 != (itr->second).end(); ++itr2)
                {
                    Player* player = ObjectAccessor::FindPlayer(*itr2);
                    if (!player)
                        continue;

                    if (!sh && player->IsInWorld())
                    {
                        sh = player->GetMap()->GetCreature(itr->first);
                        // only for visual effect
                        if (sh)
                            // Spirit Heal, effect 117
                            sh->CastSpell(sh, SPELL_SPIRIT_HEAL, true);
                    }

                    // Resurrection visual
                    player->CastSpell(player, SPELL_RESURRECTION_VISUAL, true);
                    m_ResurrectQueue.push_back(*itr2);
                }
                (itr->second).clear();
            }

            m_ReviveQueue.clear();
            m_LastResurrectTime = 0;
            isRevival = false;
        }
        else
            // queue is clear and time passed, just update last resurrection time
            m_LastResurrectTime = 0;
    }
    else if (m_LastResurrectTime > 500)    // Resurrect players only half a second later, to see spirit heal effect on NPC
    {
        for (std::vector<uint64>::const_iterator itr = m_ResurrectQueue.begin(); itr != m_ResurrectQueue.end(); ++itr)
        {
            Player* player = ObjectAccessor::FindPlayer(*itr);
            if (!player)
                continue;
            player->ResurrectPlayer(1.0f);
            player->CastSpell(player, 6962, true);
            player->CastSpell(player, SPELL_SPIRIT_HEAL_MANA, true);
            sObjectAccessor->ConvertCorpseForPlayer(*itr);
        }
        m_ResurrectQueue.clear();
    }
}

inline void Battleground::_ProcessProgress(uint32 diff)
{
    // *********************************************************
    // ***           BATTLEGROUND BALLANCE SYSTEM            ***
    // *********************************************************
    // if less then minimum players are in on one side, then start premature finish timer
    if (!m_PrematureCountDown)
    {
        m_PrematureCountDown = true;
        m_PrematureCountDownTimer = sBattlegroundMgr->GetPrematureFinishTime();
    }
    else if (m_PrematureCountDownTimer < diff)
    {
        // time's up!
        uint32 winner = 0;
        if (GetPlayersCountByTeam(ALLIANCE) >= GetMinPlayersPerTeam())
            winner = ALLIANCE;
        else if (GetPlayersCountByTeam(HORDE) >= GetMinPlayersPerTeam())
            winner = HORDE;

        EndBattleground(winner);
        m_PrematureCountDown = false;
    }
    else if (!sBattlegroundMgr->isTesting())
    {
        uint32 newtime = m_PrematureCountDownTimer - diff;
        // announce every minute
        if (newtime > (MINUTE * IN_MILLISECONDS))
        {
            if (newtime / (MINUTE * IN_MILLISECONDS) != m_PrematureCountDownTimer / (MINUTE * IN_MILLISECONDS))
                PSendMessageToAll(LANG_BATTLEGROUND_PREMATURE_FINISH_WARNING, CHAT_MSG_SYSTEM, NULL, (uint32)(m_PrematureCountDownTimer / (MINUTE * IN_MILLISECONDS)));
        }
        else
        {
            //announce every 15 seconds
            if (newtime / (15 * IN_MILLISECONDS) != m_PrematureCountDownTimer / (15 * IN_MILLISECONDS))
                PSendMessageToAll(LANG_BATTLEGROUND_PREMATURE_FINISH_WARNING_SECS, CHAT_MSG_SYSTEM, NULL, (uint32)(m_PrematureCountDownTimer / IN_MILLISECONDS));
        }
        m_PrematureCountDownTimer = newtime;
    }
}

inline void Battleground::_ProcessJoin(uint32 diff)
{
    // *********************************************************
    // ***           BATTLEGROUND STARTING SYSTEM            ***
    // *********************************************************
    ModifyStartDelayTime(diff);

    // I know it's a too big but it's the value sent in packet, I get it from retail sniff.
    // I think it's link to the countdown when bgs start
    SetRemainingTime(300000);

    if (m_ResetStatTimer > 5000)
    {
        m_ResetStatTimer = 0;
        for (BattlegroundPlayerMap::const_iterator itr = GetPlayers().begin(); itr != GetPlayers().end(); ++itr)
            if (Player* player = ObjectAccessor::FindPlayer(itr->first))
                player->ResetAllPowers(true);
    }

    if (!(m_Events & BG_STARTING_EVENT_1))
    {
        m_Events |= BG_STARTING_EVENT_1;

        if (!FindBgMap())
        {
            TC_LOG_ERROR("bg", "Battleground::_ProcessJoin: map (map id: %u, instance id: %u) is not created!", m_MapId, m_InstanceID);
            EndNow();
            return;
        }

        // Setup here, only when at least one player has ported to the map
        if (!SetupBattleground())
        {
            EndNow();
            return;
        }

        StartingEventCloseDoors();
        SetStartDelayTime(StartDelayTimes[BG_STARTING_EVENT_FIRST]);
        // First start warning - 2 or 1 minute
        SendMessageToAll(StartMessageIds[BG_STARTING_EVENT_FIRST], CHAT_MSG_BG_SYSTEM_NEUTRAL);
    }
    // After 1 minute or 30 seconds, warning is signaled
    else if (GetStartDelayTime() <= StartDelayTimes[BG_STARTING_EVENT_SECOND] && !(m_Events & BG_STARTING_EVENT_2))
    {
        m_Events |= BG_STARTING_EVENT_2;
        SendMessageToAll(StartMessageIds[BG_STARTING_EVENT_SECOND], CHAT_MSG_BG_SYSTEM_NEUTRAL);
    }
    // After 30 or 15 seconds, warning is signaled
    else if (GetStartDelayTime() <= StartDelayTimes[BG_STARTING_EVENT_THIRD] && !(m_Events & BG_STARTING_EVENT_3))
    {
        m_Events |= BG_STARTING_EVENT_3;
        SendMessageToAll(StartMessageIds[BG_STARTING_EVENT_THIRD], CHAT_MSG_BG_SYSTEM_NEUTRAL);
    }
    // Delay expired (after 2 or 1 minute)
    else if (GetStartDelayTime() <= 0 && !(m_Events & BG_STARTING_EVENT_4))
    {
        m_Events |= BG_STARTING_EVENT_4;

        StartingEventOpenDoors();

        SendWarningToAll(StartMessageIds[BG_STARTING_EVENT_FOURTH]);
        SetStatus(STATUS_IN_PROGRESS);
        SetStartDelayTime(StartDelayTimes[BG_STARTING_EVENT_FOURTH]);

        // Remove preparation
        if (isArena())
        {
           if (GetJoinType() == 5 && sWorld->getBoolConfig(CONFIG_CUSTOM_FOOTBALL))
           {
            ball = AddCreature(250018, BgCreatures.size() - 1, 35, 984.65f, 245.60f, 0.0f, 0.0f);
            end = false;
           }
         
            // TODO : add arena sound PlaySoundToAll(SOUND_ARENA_START);
            for (BattlegroundPlayerMap::const_iterator itr = GetPlayers().begin(); itr != GetPlayers().end(); ++itr)
                if (Player* player = ObjectAccessor::FindPlayer(itr->first))
                {
                    // BG Status packet
                    WorldPacket status;
                    BattlegroundQueueTypeId bgQueueTypeId = sBattlegroundMgr->BGQueueTypeId(m_TypeID, GetJoinType());
                    uint32 queueSlot = player->GetBattlegroundQueueIndex(bgQueueTypeId);
                    sBattlegroundMgr->BuildBattlegroundStatusPacket(&status, this, player, queueSlot, STATUS_IN_PROGRESS, player->GetBattlegroundQueueJoinTime(BATTLEGROUND_AA), GetElapsedTime(), GetJoinType());
                    player->GetSession()->SendPacket(&status);

                    // After getting status plr should get updates for all players in any way
                    // Remove preparation send plr updates, but on some cases it not work
                    
                    player->RemoveAurasDueToSpell(SPELL_ARENA_PREPARATION);
                    player->ResetAllPowers();

                    // remove auras with duration lower than 30s
                    Unit::AuraApplicationMap & auraMap = player->GetAppliedAuras();
                    for (Unit::AuraApplicationMap::iterator iter = auraMap.begin(); iter != auraMap.end();)
                    {
                        AuraApplication * aurApp = iter->second;
                        Aura* aura = aurApp->GetBase();
                        if (!aura->IsPermanent()
                            && aura->GetDuration() <= 30*IN_MILLISECONDS
                            && aurApp->IsPositive()
                            && (!(aura->GetSpellInfo()->Attributes & SPELL_ATTR0_UNAFFECTED_BY_INVULNERABILITY))
                            && (!aura->HasEffectType(SPELL_AURA_MOD_INVISIBILITY)))
                            player->RemoveAura(iter);
                        else
                            ++iter;
                    }
                }

            CheckArenaWinConditions();
        }
        else
        {
            PlaySoundToAll(SOUND_BG_START);

            for (BattlegroundPlayerMap::const_iterator itr = GetPlayers().begin(); itr != GetPlayers().end(); ++itr)
                if (Player* player = ObjectAccessor::FindPlayer(itr->first))
                {
                    player->RemoveAurasDueToSpell(SPELL_PREPARATION);
                    player->ResetAllPowers();
                }
            // Announce BG starting
            if (sWorld->getBoolConfig(CONFIG_BATTLEGROUND_QUEUE_ANNOUNCER_ENABLE))
                sWorld->SendWorldText(LANG_BG_STARTED_ANNOUNCE_WORLD, GetName(), GetMinLevel(), GetMaxLevel());
        }
    }

    if (GetRemainingTime() > 0 && (m_EndTime -= diff) > 0)
        SetRemainingTime(GetRemainingTime() - diff);


    // Find if the player left our start zone; if so, teleport it back
    if (m_ValidStartPositionTimer > 1000)
    {
        m_ValidStartPositionTimer = 0;
        float maxDist = GetStartMaxDist();
        if (maxDist > 0.0f)
        {
            for (BattlegroundPlayerMap::const_iterator itr = GetPlayers().begin(); itr != GetPlayers().end(); ++itr)
            {
                if (Player *plr = ObjectAccessor::FindPlayer(itr->first))
                {
                    float x, y, z, o;
                    uint32 team = plr->GetBGTeam();
                    GetTeamStartLoc(team, x, y, z, o);

                    float dist = plr->GetDistance(x, y, z);

                    if (dist >= maxDist && !(GetJoinType() == 5 && sWorld->getBoolConfig(CONFIG_CUSTOM_FOOTBALL)))
                    {
                        TC_LOG_ERROR("bg", "BATTLEGROUND: Sending %s back to start location (map: %u) (possible exploit)", plr->GetName(), GetMapId());
                        plr->TeleportTo(GetMapId(), x, y, z, o);
                    }
                }
            }
        }
    }
}

inline void Battleground::_ProcessLeave(uint32 diff)
{
    // *********************************************************
    // ***           BATTLEGROUND ENDING SYSTEM              ***
    // *********************************************************
    // remove all players from battleground after 2 minutes
    SetRemainingTime(GetRemainingTime() - diff);
    if (GetRemainingTime() <= 0)
    {
        SetRemainingTime(0);
        BattlegroundPlayerMap::iterator itr, next;
        for (itr = m_Players.begin(); itr != m_Players.end(); itr = next)
        {
            next = itr;
            ++next;
            //itr is erased here!
            if (GetJoinType() == 5 && sWorld->getBoolConfig(CONFIG_CUSTOM_FOOTBALL))
               if (Player* player = sObjectAccessor->FindPlayer(itr->first))
                  player->ExitVehicle();
            else
               RemovePlayerAtLeave(itr->first, true, true);// remove player from BG
            // do not change any battleground's private variables
        }
    }
}

inline Player* Battleground::_GetPlayer(uint64 guid, bool offlineRemove, const char* context) const
{
    Player* player = NULL;
    if (!offlineRemove)
    {
        player = ObjectAccessor::FindPlayer(guid);
        if (!player)
            TC_LOG_ERROR("bg", "Battleground::%s: player (GUID: %u) not found for BG (map: %u, instance id: %u)!",
                context, GUID_LOPART(guid), m_MapId, m_InstanceID);
    }
    return player;
}

inline Player* Battleground::_GetPlayer(BattlegroundPlayerMap::iterator itr, const char* context)
{
    return _GetPlayer(itr->first, itr->second.OfflineRemoveTime, context);
}

inline Player* Battleground::_GetPlayer(BattlegroundPlayerMap::const_iterator itr, const char* context) const
{
    return _GetPlayer(itr->first, itr->second.OfflineRemoveTime, context);
}

inline Player* Battleground::_GetPlayerForTeam(uint32 teamId, BattlegroundPlayerMap::const_iterator itr, const char* context) const
{
    Player* player = _GetPlayer(itr, context);
    if (player)
    {
        uint32 team = itr->second.Team;
        if (!team)
            team = player->GetBGTeam();
        if (team != teamId)
            player = NULL;
    }
    return player;
}

void Battleground::SetTeamStartLoc(uint32 TeamID, float X, float Y, float Z, float O)
{
    BattlegroundTeamId idx = GetTeamIndexByTeamId(TeamID);
    m_TeamStartLocX[idx] = X;
    m_TeamStartLocY[idx] = Y;
    m_TeamStartLocZ[idx] = Z;
    m_TeamStartLocO[idx] = O;
}

void Battleground::SendPacketToAll(WorldPacket* packet)
{
    for (BattlegroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
        if (Player* player = _GetPlayer(itr, "SendPacketToAll"))
            player->GetSession()->SendPacket(packet);
}

void Battleground::SendPacketToTeam(uint32 TeamID, WorldPacket* packet, Player* sender, bool self)
{
    for (BattlegroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
        if (Player* player = _GetPlayerForTeam(TeamID, itr, "SendPacketToTeam"))
            if (self || sender != player)
                player->GetSession()->SendPacket(packet);
}

void Battleground::PlaySoundToAll(uint32 SoundID)
{
    WorldPacket data;
    sBattlegroundMgr->BuildPlaySoundPacket(&data, SoundID);
    SendPacketToAll(&data);
}

void Battleground::PlaySoundToTeam(uint32 SoundID, uint32 TeamID)
{
    WorldPacket data;
    for (BattlegroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
        if (Player* player = _GetPlayerForTeam(TeamID, itr, "PlaySoundToTeam"))
        {
            sBattlegroundMgr->BuildPlaySoundPacket(&data, SoundID);
            player->GetSession()->SendPacket(&data);
        }
}

void Battleground::CastSpellOnTeam(uint32 SpellID, uint32 TeamID)
{
    for (BattlegroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
        if (Player* player = _GetPlayerForTeam(TeamID, itr, "CastSpellOnTeam"))
            player->CastSpell(player, SpellID, true);
}

void Battleground::RemoveAuraOnTeam(uint32 SpellID, uint32 TeamID)
{
    for (BattlegroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
        if (Player* player = _GetPlayerForTeam(TeamID, itr, "RemoveAuraOnTeam"))
            player->RemoveAura(SpellID);
}

void Battleground::YellToAll(Creature* creature, const char* text, uint32 language)
{
    for (BattlegroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
        if (Player* player = _GetPlayer(itr, "YellToAll"))
        {
            WorldPacket data(SMSG_MESSAGECHAT, 200);
            creature->BuildMonsterChat(&data, CHAT_MSG_MONSTER_YELL, text, language, creature->GetName(), itr->first);
            player->GetSession()->SendPacket(&data);
        }
}

void Battleground::RewardHonorToTeam(uint32 Honor, uint32 TeamID)
{
    for (BattlegroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
        if (Player* player = _GetPlayerForTeam(TeamID, itr, "RewardHonorToTeam"))
            UpdatePlayerScore(player, SCORE_BONUS_HONOR, Honor);
}

void Battleground::RewardReputationToTeam(uint32 faction_id, uint32 Reputation, uint32 TeamID)
{
    if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(faction_id))
        for (BattlegroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
            if (Player* player = _GetPlayerForTeam(TeamID, itr, "RewardReputationToTeam"))
                player->GetReputationMgr().ModifyReputation(factionEntry, Reputation);
}

void Battleground::UpdateWorldState(uint32 Field, uint32 Value)
{
    WorldPacket data;
    sBattlegroundMgr->BuildUpdateWorldStatePacket(&data, Field, Value);
    SendPacketToAll(&data);
}

void Battleground::UpdateWorldStateForPlayer(uint32 Field, uint32 Value, Player* Source)
{
    WorldPacket data;
    sBattlegroundMgr->BuildUpdateWorldStatePacket(&data, Field, Value);
    Source->GetSession()->SendPacket(&data);
}

void Battleground::EndBattleground(uint32 winner)
{
    RemoveFromBGFreeSlotQueue();

    WorldPacket data;
    int32 winmsg_id = 0;

    BracketType bType = BattlegroundMgr::BracketByJoinType(GetJoinType());

    if (winner == ALLIANCE)
    {
        winmsg_id = isBattleground() ? LANG_BG_A_WINS : LANG_ARENA_GOLD_WINS;
        PlaySoundToAll(SOUND_ALLIANCE_WINS);                // alliance wins sound
        SetWinner(WINNER_ALLIANCE);
    }
    else if (winner == HORDE)
    {
        winmsg_id = isBattleground() ? LANG_BG_H_WINS : LANG_ARENA_GREEN_WINS;
        PlaySoundToAll(SOUND_HORDE_WINS);                   // horde wins sound
        SetWinner(WINNER_HORDE);
    }
    else
    {
        SetWinner(3);
    }

    TC_LOG_DEBUG("bg", "BATTLEGROUND: EndBattleground. JounType: %u, Bracket %u, Winer %u", GetJoinType(), bType, winner);

    SetStatus(STATUS_WAIT_LEAVE);
    //we must set it this way, because end time is sent in packet!
    SetRemainingTime(TIME_AUTOCLOSE_BATTLEGROUND);

    bool guildAwarded = false;
    uint8 aliveWinners = GetAlivePlayersCountByTeam(winner);
    std::ostringstream info;
    
    for (BattlegroundPlayerMap::iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        uint32 team = itr->second.Team;

        if (itr->second.OfflineRemoveTime)
        {
            //if rated arena match - make member lost!
            if (isArena() && isRated() && winner != WINNER_NONE)
            {
                Bracket* bracket = sBracketMgr->TryGetOrCreateBracket(itr->first, bType);
                ASSERT(bracket);    //unreal
                uint32 gain;
                if (GetJoinType() == 5 && sWorld->getBoolConfig(CONFIG_CUSTOM_FOOTBALL))
                  gain = 0;
                else
                  gain = bracket->FinishGame(team == winner, GetMatchmakerRating(team == winner ? GetOtherTeam(winner) : winner));
                
                info << " >> Plr. Not in game at the end of match. GUID: " << itr->first << " mmr gain: " << gain << " state:" << (team == winner) ? "WINER" : "LOSER";
            }
            continue;
        }

        Player* player = _GetPlayer(itr, "EndBattleground");
        if (!player)
            continue;

        Bracket* bracket = GetJoinType() ? player->getBracket(bType) : NULL;

        // should remove spirit of redemption
        if (player->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
            player->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);
         
        if (isBattleground() && sWorld->getBoolConfig(CONFIG_CUSTOM_BATTLEGROUND))
        {         
           player->DeMorph(); 
           player->ResetCustomDisplayId();
           player->SetObjectScale(1.0f);
           player->DestroyItemCount(SPELL_CUSTOM, 1, true);
           player->RemoveAura(spell_custom_2);
           custom_exists = false;
        }

        // Last standing - Rated 5v5 arena & be solely alive player
        if (team == winner && isArena() && isRated() && GetJoinType() == ARENA_TYPE_5v5 && aliveWinners == 1 && player->IsAlive())
            player->CastSpell(player, SPELL_THE_LAST_STANDING, true);

        if (!player->IsAlive())
        {
            player->ResurrectPlayer(1.0f);
            player->SpawnCorpseBones();
        }
        else
        {
            //needed cause else in av some creatures will kill the players at the end
            player->CombatStop();
            player->getHostileRefManager().deleteReferences();
        }

        // per player calculation
        if (isArena() && isRated() && winner != WINNER_NONE)
        {
            uint32 gain;
            if (GetJoinType() == 5 && sWorld->getBoolConfig(CONFIG_CUSTOM_FOOTBALL))
                  gain = 0;
                else
                  gain = bracket->FinishGame(team == winner, GetMatchmakerRating(team == winner ? GetOtherTeam(winner) : winner));
            info << " >> Plr: " << player->ToString().c_str() << " mmr gain: " << gain << " state:" << (team == winner) ? "WINER" : "LOSER";
            if (team == winner)
            {
                // update achievement BEFORE personal rating update.
                player->UpdateAchievementCriteria(CRITERIA_TYPE_WIN_RATED_ARENA, 1);
                player->UpdateAchievementCriteria(CRITERIA_TYPE_WIN_ARENA, GetMapId());
                if (GetJoinType() == 5 && sWorld->getBoolConfig(CONFIG_CUSTOM_FOOTBALL))
                  player->ModifyCurrency(CURRENCY_TYPE_HONOR_POINTS, 40);
                else
                  player->ModifyCurrency(CURRENCY_TYPE_CONQUEST_META_ARENA, sWorld->getIntConfig(CONFIG_CURRENCY_CONQUEST_POINTS_ARENA_REWARD));
            }
            else
            {
                // Arena lost => reset the win_rated_arena having the "no_lose" condition
                player->GetAchievementMgr().ResetAchievementCriteria(CRITERIA_TYPE_WIN_RATED_ARENA, CRITERIA_CONDITION_NO_LOSE);
            }
            player->UpdateAchievementCriteria(CRITERIA_TYPE_PLAY_ARENA, GetMapId());
        }

        uint32 winner_bonus = player->GetRandomWinner() ? BG_REWARD_WINNER_HONOR_LAST : BG_REWARD_WINNER_HONOR_FIRST;
        uint32 loser_bonus = player->GetRandomWinner() ? BG_REWARD_LOSER_HONOR_LAST : BG_REWARD_LOSER_HONOR_FIRST;

        // remove temporary currency bonus auras before rewarding player
        player->RemoveAura(SPELL_HONORABLE_DEFENDER_25Y);
        player->RemoveAura(SPELL_HONORABLE_DEFENDER_60Y);

        // Reward winner team
        if (team == winner)
        {
            if (isBattleground() && !IsRBG() && (player->IsInvitedForBattlegroundQueueType(BATTLEGROUND_QUEUE_RB) || BattlegroundMgr::IsBGWeekend(GetTypeID())))
            {
                UpdatePlayerScore(player, SCORE_BONUS_HONOR, winner_bonus);
                player->AddItem(185212, 1);
                if (!player->GetRandomWinner())
                {
                    // 150cp awarded for the first rated battleground won each day 
                    player->ModifyCurrency(CURRENCY_TYPE_CONQUEST_META_ARENA/*CURRENCY_TYPE_CONQUEST_META_RANDOM_BG*/, BG_REWARD_WINNER_CONQUEST_FIRST);
                    player->SetRandomWinner(true);
                }
                else
                    player->ModifyCurrency(CURRENCY_TYPE_CONQUEST_META_ARENA/*CURRENCY_TYPE_CONQUEST_META_RANDOM_BG*/, BG_REWARD_WINNER_CONQUEST_LAST);
            }

            player->UpdateAchievementCriteria(CRITERIA_TYPE_WIN_BG, 1);
            if (!guildAwarded)
            {
                guildAwarded = true;
                if (uint32 guildId = GetBgMap()->GetOwnerGuildId(player->GetTeam()))
                    if (Guild* guild = sGuildMgr->GetGuildById(guildId))
                    {
                        guild->GetAchievementMgr().UpdateAchievementCriteria(CRITERIA_TYPE_WIN_BG, 1, 0, 0, NULL, player);
                        if (isArena() && isRated() && winner != WINNER_NONE)
                        {
                            ASSERT(bracket);
                            guild->GetAchievementMgr().UpdateAchievementCriteria(CRITERIA_TYPE_WIN_RATED_ARENA, std::max<uint32>(bracket->getRating(), 1), 0, 0, NULL, player);
                        }
                    }
            }
        }
        else
        {
            if (player->IsInvitedForBattlegroundQueueType(BATTLEGROUND_QUEUE_RB) || BattlegroundMgr::IsBGWeekend(GetTypeID()))
                UpdatePlayerScore(player, SCORE_BONUS_HONOR, loser_bonus);
        }

        player->ResetAllPowers();
        player->CombatStopWithPets(true);

        BlockMovement(player);

        if (IsRBG())
        {
            player->getBracket(BRACKET_TYPE_RATED_BG)->FinishGame(team == winner, GetMatchmakerRating(team == winner ? GetOtherTeam(winner) : winner));

            if (team == winner)
                player->ModifyCurrency(CURRENCY_TYPE_CONQUEST_META_RATED_BG, sWorld->getIntConfig(CONFIG_CURRENCY_CONQUEST_POINTS_RBG_REWARD));
            
            player->UpdateAchievementCriteria(CRITERIA_TYPE_WIN_RATED_BATTLEGROUND, GetMapId());
            player->UpdateAchievementCriteria(CRITERIA_TYPE_REACH_RBG_RATING, std::max<uint32>(bracket->getRating(), 1));
        }

        BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(GetTypeID(), GetJoinType());
        sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, this, player, player->GetBattlegroundQueueIndex(bgQueueTypeId), STATUS_IN_PROGRESS, player->GetBattlegroundQueueJoinTime(isArena() ? BATTLEGROUND_AA : GetTypeID()), GetElapsedTime(), GetJoinType());
        player->GetSession()->SendPacket(&data);

        player->UpdateAchievementCriteria(CRITERIA_TYPE_COMPLETE_BATTLEGROUND, 1);
    }

    if (isArena() && isRated() && winner != WINNER_NONE)
    {
        const char* winnerTeam[10];
        const char*  loserTeam[10];
        for (uint8 i=0; i < 10; ++i)
        {
            winnerTeam[i] = "";
            loserTeam[i] = "";
        }
        uint8 w = 0;
        uint8 l = 0;

        for (std::list<const char*>::iterator itr = m_nameList[winner].begin(); itr != m_nameList[winner].end(); ++itr)
            if (const char* winnerName = (*itr))
            {
                winnerTeam[w] = winnerName;
                w++;
            }

        for (std::list<const char*>::iterator itr = m_nameList[GetOtherTeam(winner)].begin(); itr != m_nameList[GetOtherTeam(winner)].end(); ++itr)
            if (const char* loserName = (*itr))
            {
                loserTeam[l] = loserName;
                l++;
            }

        uint32 _arenaTimer = m_StartTime / 1000;
        uint32 _min = _arenaTimer / 60;
        uint32 _sec = _arenaTimer - (_min * 60);
        const char* att = "";
        if (m_StartTime < 75000)
            att = "--- ATTENTION!";

        switch (m_JoinType)
        {
            case 2:
            {
                TC_LOG_DEBUG("arena","FINISH: Arena match Type: 2v2 --- --- Winner[%s, %s]: old rating: %u --- Loser[%s, %s]: old rating: %u --- Duration: %u min. %u sec. %s",
                    winnerTeam[0], winnerTeam[1], GetMatchmakerRating(winner), loserTeam[0], loserTeam[1], GetMatchmakerRating(GetOtherTeam(winner)), _min, _sec, att);
                break;
            }
            case 3:
            {
                TC_LOG_DEBUG("arena","FINISH: Arena match Type: --- 3v3 --- Winner[%s, %s, %s]: old rating: %u --- Loser[%s, %s, %s]: old rating: %u --- Duration: %u min. %u sec. %s",
                    winnerTeam[0], winnerTeam[1], winnerTeam[2], GetMatchmakerRating(winner), loserTeam[0], loserTeam[1], loserTeam[2], GetMatchmakerRating(GetOtherTeam(winner)), _min, _sec, att);
                break;
            }
            case 5:
            {
                TC_LOG_DEBUG("arena","FINISH: Arena match Type: --- --- 5v5 Winner[%s, %s, %s, %s, %s]: old rating: %u --- Loser[%s, %s, %s, %s, %s]: old rating: %u --- Duration: %u min. %u sec. %s",
                    winnerTeam[0], winnerTeam[1], winnerTeam[2], winnerTeam[3], winnerTeam[4], GetMatchmakerRating(winner), loserTeam[0], loserTeam[1], loserTeam[2], loserTeam[3], loserTeam[4], GetMatchmakerRating(GetOtherTeam(winner)), _min, _sec, att);
                break;
            }
            default:
                TC_LOG_DEBUG("arena","FINISH: RBG --- Winner[%s, %s, %s, %s, %s, %s, %s, %s, %s, %s]: old rating: %u --- Loser[%s, %s, %s, %s, %s, %s, %s, %s, %s, %s]: old rating: %u --- Duration: %u min. %u sec. %s",
                    winnerTeam[0], winnerTeam[1], winnerTeam[2], winnerTeam[3], winnerTeam[4], winnerTeam[5], winnerTeam[6], winnerTeam[7], winnerTeam[8], winnerTeam[9], GetMatchmakerRating(winner), 
                    loserTeam[0], loserTeam[1], loserTeam[2], loserTeam[3], loserTeam[4], loserTeam[5], loserTeam[6], loserTeam[7], loserTeam[8], loserTeam[9], GetMatchmakerRating(GetOtherTeam(winner)), _min, _sec, att);
                break;
        }
        
    }

    sBattlegroundMgr->BuildPvpLogDataPacket(&data, this);
    SendPacketToAll(&data);

    if (winmsg_id)
        SendMessageToAll(winmsg_id, CHAT_MSG_BG_SYSTEM_NEUTRAL);
}

uint32 Battleground::GetBonusHonorFromKill(uint32 kills) const
{
    //variable kills means how many honorable kills you scored (so we need kills * honor_for_one_kill)
    uint32 maxLevel = std::min(GetMaxLevel(), 90U);
    return Trinity::Honor::hk_honor_at_level(maxLevel, float(kills));
}

void Battleground::BlockMovement(Player* player)
{
    player->SetClientControl(player, 0);                          // movement disabled NOTE: the effect will be automatically removed by client when the player is teleported from the battleground, so no need to send with uint8(1) in RemovePlayerAtLeave()
}

void Battleground::RemovePlayerAtLeave(uint64 guid, bool Transport, bool SendPacket)
{
    uint32 team = GetPlayerTeam(guid);
    bool participant = false;
    // Remove from lists/maps
    BattlegroundPlayerMap::iterator itr = m_Players.find(guid);
    if (itr != m_Players.end())
    {
        UpdatePlayersCountByTeam(team, true);               // -1 player
        m_Players.erase(itr);
        // check if the player was a participant of the match, or only entered through gm command (goname)
        participant = true;
    }

    RemovePlayerFromResurrectQueue(guid);

    Player* player = ObjectAccessor::FindPlayer(guid);

    // should remove spirit of redemption
    if (player)
    {
        if (isBattleground() && sWorld->getBoolConfig(CONFIG_CUSTOM_BATTLEGROUND) && (player->GetGUID() == CustomGUID))
        {         
            player->DeMorph(); 
            player->ResetCustomDisplayId();
            player->SetObjectScale(1.0f);
            player->DestroyItemCount(SPELL_CUSTOM, 1, true);
            player->RemoveAura(spell_custom_2);  
            custom_exists = false;
            CustomGUID = 0;
        }       
        
        if (player->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
            player->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);

        if (!player->IsAlive())                              // resurrect on exit
        {
            player->ResurrectPlayer(1.0f);
            player->SpawnCorpseBones();
        }

        player->RemoveAura(SPELL_BATTLE_FATIGUE);
    }

    RemovePlayer(player, guid, team);                           // BG subclass specific code

    BattlegroundTypeId bgTypeId = GetTypeID();
    BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(GetTypeID(), GetJoinType());

    if (participant) // if the player was a match participant, remove auras, calc rating, update queue
    {
        if (player)
        {
            player->ClearAfkReports();

            if (!team) team = player->GetBGTeam();

            // if arena, remove the specific arena auras
            if (isArena() || IsRBG())
            {
                // set the bg type to all arenas & rbg (it will be used for queue refreshing)
                bgTypeId = isArena() ? BATTLEGROUND_AA : BATTLEGROUND_RATED_10_VS_10;

                // unsummon current and summon old pet if there was one and there isn't a current pet
                player->RemovePet(NULL);
                player->ResummonPetTemporaryUnSummonedIfAny();

                if (isRated() && GetStatus() == STATUS_IN_PROGRESS)
                {
                    //left a rated match while the encounter was in progress, consider as loser
                    BracketType bType = BattlegroundMgr::BracketByJoinType(GetJoinType());
                    Bracket* bracket = player->getBracket(bType);
                    ASSERT(bracket);    //shouldn't happend
                    bracket->FinishGame(false/*lost*/, GetMatchmakerRating(GetOtherTeam(team)));
                }
            }
            if (SendPacket)
            {
                WorldPacket data;
                sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, this, player, player->GetBattlegroundQueueIndex(bgQueueTypeId), STATUS_NONE, player->GetBattlegroundQueueJoinTime(bgTypeId), 0, 0);
                player->GetSession()->SendPacket(&data);
            }

            // this call is important, because player, when joins to battleground, this method is not called, so it must be called when leaving bg
            player->RemoveBattlegroundQueueId(bgQueueTypeId);
        }
        else
        // removing offline participant
        {
            if (isRated() && GetStatus() == STATUS_IN_PROGRESS)
            {
                //left a rated match while the encounter was in progress, consider as loser
                BracketType bType = BattlegroundMgr::BracketByJoinType(GetJoinType());
                Bracket* bracket = sBracketMgr->TryGetOrCreateBracket(guid, bType);
                ASSERT(bracket);    //shouldn't happend
                bracket->FinishGame(false/*lost*/, GetMatchmakerRating(GetOtherTeam(team)));
            }
        }

        // remove from raid group if player is member
        if (Group* group = GetBgRaid(team))
        {
            if (!group->RemoveMember(guid))                // group was disbanded
            {
                SetBgRaid(team, NULL);
            }
        }
        DecreaseInvitedCount(team);
        //we should update battleground queue, but only if bg isn't ending
        if (isBattleground() && GetStatus() < STATUS_WAIT_LEAVE)
        {
            // a player has left the battleground, so there are free slots -> add to queue
            AddToBGFreeSlotQueue();
            sBattlegroundMgr->ScheduleQueueUpdate(0, 0, bgQueueTypeId, bgTypeId, GetBracketId());
        }
        // Let others know
        WorldPacket data;
        sBattlegroundMgr->BuildPlayerLeftBattlegroundPacket(&data, guid);
        SendPacketToTeam(team, &data, player, false);
    }

    if (player)
    {
        // Do next only if found in battleground
        player->SetBattlegroundId(0, BATTLEGROUND_TYPE_NONE);  // We're not in BG.
        // reset destination bg team
        player->SetBGTeam(0);
        player->SetByteValue(PLAYER_BYTES_3, 3, 0);
        player->RemoveBattlegroundQueueJoinTime(bgTypeId);

        if (Transport)
            player->TeleportToBGEntryPoint();

        TC_LOG_INFO("bg", "BATTLEGROUND: Removed player %s from Battleground.", player->GetName());
    }

    //battleground object will be deleted next Battleground::Update() call
}

// this method is called when no players remains in battleground
void Battleground::Reset()
{
    SetWinner(WINNER_NONE);
    SetStatus(STATUS_WAIT_QUEUE);
    SetElapsedTime(0);
    SetRemainingTime(0);
    SetLastResurrectTime(0);
    SetJoinType(0);
    SetRated(false);

    m_Events = 0;

    if (m_InvitedAlliance > 0 || m_InvitedHorde > 0)
        TC_LOG_ERROR("bg", "Battleground::Reset: one of the counters is not 0 (alliance: %u, horde: %u) for BG (map: %u, instance id: %u)!",
            m_InvitedAlliance, m_InvitedHorde, m_MapId, m_InstanceID);

    m_InvitedAlliance = 0;
    m_InvitedHorde = 0;
    m_InBGFreeSlotQueue = false;

    m_Players.clear();

    for (BattlegroundScoreMap::const_iterator itr = PlayerScores.begin(); itr != PlayerScores.end(); ++itr)
        delete itr->second;
    PlayerScores.clear();

    ResetBGSubclass();
}

void Battleground::StartBattleground()
{
    SetElapsedTime(0);
    SetLastResurrectTime(0);
    // add BG to free slot queue
    AddToBGFreeSlotQueue();

    // add bg to update list
    // This must be done here, because we need to have already invited some players when first BG::Update() method is executed
    // and it doesn't matter if we call StartBattleground() more times, because m_Battlegrounds is a map and instance id never changes
    sBattlegroundMgr->AddBattleground(GetInstanceID(), GetTypeID(), this);
}

void Battleground::AddPlayer(Player* player)
{
    // remove afk from player
    if (player->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK))
        player->ToggleAFK();

    // score struct must be created in inherited class

    uint64 guid = player->GetGUID();
    uint32 team = player->GetBGTeam();

    BattlegroundPlayer bp;
    bp.OfflineRemoveTime = 0;
    bp.Team = team;

    // Add to list/maps
    m_Players[guid] = bp;
    ASSERT(PlayerScores[guid]);                             //not add bg score???? First add score after add to bg
    PlayerScores[guid]->Team = team;

    UpdatePlayersCountByTeam(team, false);                  // +1 player

    WorldPacket data;
    sBattlegroundMgr->BuildPlayerJoinedBattlegroundPacket(&data, guid);
    SendPacketToTeam(team, &data, player, false);

    // BG Status packet
    BattlegroundQueueTypeId bgQueueTypeId = sBattlegroundMgr->BGQueueTypeId(m_TypeID, GetJoinType());
    uint32 queueSlot = player->GetBattlegroundQueueIndex(bgQueueTypeId);
    // if is BATTLEGROUND_AA or BATTLEGROUND_RB or BATTLEGROUND_RATED_10_VS_10 m_TypeID == right data
    sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, this, player, queueSlot, STATUS_IN_PROGRESS, player->GetBattlegroundQueueJoinTime(m_TypeID), GetElapsedTime(), GetJoinType());

    player->GetSession()->SendPacket(&data);

    player->Dismount();
    player->RemoveAurasByType(SPELL_AURA_MOUNTED);
    player->RemoveAurasByType(SPELL_AURA_FLY);

    if (IsRBG() || (isBattleground() && sWorld->getBoolConfig(CONFIG_CROSSFACTIONBG)))
    {
        if (!player->HasAura(SPELL_SET_FACTION_HORDE) && player->GetTeam() == ALLIANCE && player->GetBGTeam() != ALLIANCE)
            player->CastSpell(player, SPELL_SET_FACTION_HORDE, true);
        else if (!player->HasAura(SPELL_SET_FACTION_ALLIANCE) && player->GetTeam() == HORDE && player->GetBGTeam() != HORDE)
            player->CastSpell(player, SPELL_SET_FACTION_ALLIANCE, true);
        
        if (IsRBG())
            player->CastSpell(player, SPELL_BATTLE_FATIGUE, true);
    }

    // add arena specific auras
    if (isArena())
    {
        player->ResummonPetTemporaryUnSummonedIfAny();

        // Removing pet's buffs and debuffs which are not permanent on Arena enter
        if (Pet* pet = player->GetPet())
        {
            if (!pet->IsAlive())
                pet->setDeathState(ALIVE);

            // Set pet at full health
            pet->SetHealth(pet->GetMaxHealth());

            pet->RemoveAllAuras();
        }

        player->RemoveArenaEnchantments(TEMP_ENCHANTMENT_SLOT);
        if (team == ALLIANCE)                                // gold
        {
            if (player->GetTeam() == HORDE)
                player->CastSpell(player, SPELL_HORDE_GOLD_FLAG, true);
            else
                player->CastSpell(player, SPELL_ALLIANCE_GOLD_FLAG, true);
        }
        else                                                // green
        {
            if (player->GetTeam() == HORDE)
                player->CastSpell(player, SPELL_HORDE_GREEN_FLAG, true);
            else
                player->CastSpell(player, SPELL_ALLIANCE_GREEN_FLAG, true);
        }

        player->CastSpell(player, SPELL_BATTLE_FATIGUE, true);

        player->DestroyConjuredItems(true);
        player->UnsummonPetTemporaryIfAny();

        if (GetStatus() == STATUS_WAIT_JOIN)                 // not started yet
        {
            player->CastSpell(player, SPELL_ARENA_PREPARATION, true);
            player->ResetAllPowers(true);
        }

        // Set arena faction client-side to display arena unit frame
        player->SetByteValue(PLAYER_BYTES_3, 3, player->GetBGTeam() == HORDE ? 0 : 1);

        Pet* pet = player->GetPet();
        uint64 petGUID = pet ? pet->GetGUID() : 0;

        //not shure if we should to it at join.
        SendOpponentSpecialization(team);
        SendOpponentSpecialization(GetOtherTeam(team));
    }
    else
    {
        if (GetStatus() == STATUS_WAIT_JOIN)                 // not started yet
            player->CastSpell(player, SPELL_PREPARATION, true);   // reduces all mana cost of spells.
            
            if (isBattleground() && !IsRBG() && sWorld->getBoolConfig(CONFIG_CROSSFACTIONBG) && player->GetBGTeam() != player->GetTeam())
                player->SetByteValue(PLAYER_BYTES_3, 3, player->GetBGTeam() == HORDE ? 0 : 1);
    }

    if (GetStatus() == STATUS_WAIT_JOIN)
    {
        WorldPacket data(SMSG_START_TIMER, 12);
        data << uint32(0);  // timer type
        data << uint32(GetStartDelayTime() / 1000);
        data << uint32(StartDelayTimes[BG_STARTING_EVENT_FIRST] / 1000);
        SendPacketToAll(&data);
    }

    // setup BG group membership
    PlayerAddedToBGCheckIfBGIsRunning(player);
    AddOrSetPlayerToCorrectBgGroup(player, team);

    //Recal amaunt in auras
    player->ScheduleDelayedOperation(DELAYED_UPDATE_AFTER_TO_BG);

    // Log
    TC_LOG_INFO("bg", "BATTLEGROUND: Player %s joined the battle.", player->GetName());
    
    if (isBattleground() && sWorld->getBoolConfig(CONFIG_CUSTOM_BATTLEGROUND))
    {   
       if (!custom_exists) 
       {   
           player->SetDisplayId(MORPH_CUSTOM, true);
           player->SetCustomDisplayId(MORPH_CUSTOM); 
           player->Yell(TEXT, LANG_UNIVERSAL);
           if (player->GetBGTeamId() == TEAM_ALLIANCE)
           {
               SendMessage2ToAll(LANG_SELECT_DEF, CHAT_MSG_BG_SYSTEM_ALLIANCE, player);
               SendMessage2ToAll(LANG_SELECT_ATTACK, CHAT_MSG_BG_SYSTEM_HORDE, player);
           }
           else
           {
               SendMessage2ToAll(LANG_SELECT_ATTACK, CHAT_MSG_BG_SYSTEM_ALLIANCE, player);
               SendMessage2ToAll(LANG_SELECT_DEF, CHAT_MSG_BG_SYSTEM_HORDE, player);              
           }
           player->GetSession()->SendNotification(notification); //анонс
           ChatHandler(player->GetSession()).PSendSysMessage(notification); // в  чат
           player->AddItem(SPELL_CUSTOM, 1); //баф 
           player->AddAura(spell_custom_2, player);
           CustomGUID = player->GetGUID(); 
           custom_exists = true; 
       } 
    }
    if (GetJoinType() == 5 && sWorld->getBoolConfig(CONFIG_CUSTOM_FOOTBALL))
    {
       if (Creature* tank = player->SummonCreature(250017, player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation()))
       {
          player->CastSpell(tank, 65031, true); //summon 250017 and cast on vehicle 65031
          if (team == ALLIANCE)
          {
             tank->SetDisplayId(16841, true);
             tank->AddAura(65964, tank);
          }
          else
          {
             tank->SetDisplayId(47695,true);
             tank->AddAura(58169, tank);
          }
       }
    }
}

// this method adds player to his team's bg group, or sets his correct group if player is already in bg group
void Battleground::AddOrSetPlayerToCorrectBgGroup(Player* player, uint32 team)
{
    uint64 playerGuid = player->GetGUID();
    Group* group = GetBgRaid(team);
    if (!group)                                      // first player joined
    {
        group = new Group;
        SetBgRaid(team, group);
        group->Create(player);
        sGroupMgr->AddGroup(group);
    }
    else                                            // raid already exist
    {
        if (group->IsMember(playerGuid))
        {
            uint8 subgroup = group->GetMemberGroup(playerGuid);
            player->SetBattlegroundOrBattlefieldRaid(group, subgroup);
        }
        else
        {
            group->AddMember(player);
            if (Group* originalGroup = player->GetOriginalGroup())
                if (originalGroup->IsLeader(playerGuid))
                {
                    group->ChangeLeader(playerGuid);
                    group->SendUpdate();
                }
        }
    }
}

// This method should be called when player logs into running battleground
void Battleground::EventPlayerLoggedIn(Player* player)
{
    uint64 guid = player->GetGUID();
    // player is correct pointer
    for (std::deque<uint64>::iterator itr = m_OfflineQueue.begin(); itr != m_OfflineQueue.end(); ++itr)
    {
        if (*itr == guid)
        {
            m_OfflineQueue.erase(itr);
            break;
        }
    }
    m_Players[guid].OfflineRemoveTime = 0;
    
    if (IsRBG() || (isBattleground() && sWorld->getBoolConfig(CONFIG_CROSSFACTIONBG)))
    {
        if (!player->HasAura(SPELL_SET_FACTION_HORDE) && player->GetTeam() == ALLIANCE && player->GetBGTeam() != ALLIANCE)
            player->CastSpell(player, SPELL_SET_FACTION_HORDE, true);
        else if (!player->HasAura(SPELL_SET_FACTION_ALLIANCE) && player->GetTeam() == HORDE && player->GetBGTeam() != HORDE)
            player->CastSpell(player, SPELL_SET_FACTION_ALLIANCE, true);
        
        player->SetByteValue(PLAYER_BYTES_3, 3, player->GetBGTeam() == HORDE ? 0 : 1);
    }
    
    PlayerAddedToBGCheckIfBGIsRunning(player);
    // if battleground is starting, then add preparation aura
    // we don't have to do that, because preparation aura isn't removed when player logs out
}

// This method should be called when player logs out from running battleground
void Battleground::EventPlayerLoggedOut(Player* player)
{
    uint64 guid = player->GetGUID();
    if (!IsPlayerInBattleground(guid))  // Check if this player really is in battleground (might be a GM who teleported inside)
        return;

    // player is correct pointer, it is checked in WorldSession::LogoutPlayer()
    m_OfflineQueue.push_back(player->GetGUID());
    m_Players[guid].OfflineRemoveTime = sWorld->GetGameTime() + MAX_OFFLINE_TIME;
    if (GetStatus() == STATUS_IN_PROGRESS)
    {
        // drop flag and handle other cleanups
        RemovePlayer(player, guid, GetPlayerTeam(guid));

        // 1 player is logging out, if it is the last, then end arena!
        if (isArena())
            if (GetAlivePlayersCountByTeam(player->GetTeam()) <= 1 && GetPlayersCountByTeam(GetOtherTeam(player->GetTeam())))
                EndBattleground(GetOtherTeam(player->GetTeam()));
    }
    if (isBattleground() && sWorld->getBoolConfig(CONFIG_CUSTOM_BATTLEGROUND) && (player->GetGUID() == CustomGUID))
    {         
        player->DeMorph(); 
        player->ResetCustomDisplayId();
        player->SetObjectScale(1.0f);
        player->DestroyItemCount(SPELL_CUSTOM, 1, true);
        player->RemoveAura(spell_custom_2);  
        CustomGUID = 0;
        custom_exists = false;
    }
        
}

// This method should be called only once ... it adds pointer to queue
void Battleground::AddToBGFreeSlotQueue()
{
    // make sure to add only once
    if (!m_InBGFreeSlotQueue && isBattleground())
    {
        sBattlegroundMgr->BGFreeSlotQueue[m_TypeID].push_front(this);
        m_InBGFreeSlotQueue = true;
    }
}

// This method removes this battleground from free queue - it must be called when deleting battleground - not used now
void Battleground::RemoveFromBGFreeSlotQueue()
{
    // set to be able to re-add if needed
    m_InBGFreeSlotQueue = false;
    // uncomment this code when battlegrounds will work like instances
    for (BGFreeSlotQueueType::iterator itr = sBattlegroundMgr->BGFreeSlotQueue[m_TypeID].begin(); itr != sBattlegroundMgr->BGFreeSlotQueue[m_TypeID].end(); ++itr)
    {
        if ((*itr)->GetInstanceID() == m_InstanceID)
        {
            sBattlegroundMgr->BGFreeSlotQueue[m_TypeID].erase(itr);
            return;
        }
    }
}

// get the number of free slots for team
// returns the number how many players can join battleground to MaxPlayersPerTeam
uint32 Battleground::GetFreeSlotsForTeam(uint32 Team) const
{
    // if BG is starting ... invite anyone
    if (GetStatus() == STATUS_WAIT_JOIN)
        return (GetInvitedCount(Team) < GetMaxPlayersPerTeam()) ? GetMaxPlayersPerTeam() - GetInvitedCount(Team) : 0;
    // if BG is already started .. do not allow to join too much players of one faction
    uint32 otherTeam;
    uint32 otherIn;
    if (Team == ALLIANCE)
    {
        otherTeam = GetInvitedCount(HORDE);
        otherIn = GetPlayersCountByTeam(HORDE);
    }
    else
    {
        otherTeam = GetInvitedCount(ALLIANCE);
        otherIn = GetPlayersCountByTeam(ALLIANCE);
    }
    if (GetStatus() == STATUS_IN_PROGRESS)
    {
        // difference based on ppl invited (not necessarily entered battle)
        // default: allow 0
        uint32 diff = 0;
        // allow join one person if the sides are equal (to fill up bg to minplayersperteam)
        if (otherTeam == GetInvitedCount(Team))
            diff = 1;
        // allow join more ppl if the other side has more players
        else if (otherTeam > GetInvitedCount(Team))
            diff = otherTeam - GetInvitedCount(Team);

        // difference based on max players per team (don't allow inviting more)
        uint32 diff2 = (GetInvitedCount(Team) < GetMaxPlayersPerTeam()) ? GetMaxPlayersPerTeam() - GetInvitedCount(Team) : 0;
        // difference based on players who already entered
        // default: allow 0
        uint32 diff3 = 0;
        // allow join one person if the sides are equal (to fill up bg minplayersperteam)
        if (otherIn == GetPlayersCountByTeam(Team))
            diff3 = 1;
        // allow join more ppl if the other side has more players
        else if (otherIn > GetPlayersCountByTeam(Team))
            diff3 = otherIn - GetPlayersCountByTeam(Team);
        // or other side has less than minPlayersPerTeam
        else if (GetInvitedCount(Team) <= GetMinPlayersPerTeam())
            diff3 = GetMinPlayersPerTeam() - GetInvitedCount(Team) + 1;

        // return the minimum of the 3 differences

        // min of diff and diff 2
        diff = std::min(diff, diff2);
        // min of diff, diff2 and diff3
        return std::min(diff, diff3);
    }
    return 0;
}

bool Battleground::HasFreeSlots() const
{
    return GetPlayersSize() < GetMaxPlayers();
}

void Battleground::UpdatePlayerScore(Player* Source, uint32 type, uint32 value, bool doAddHonor)
{
    //this procedure is called from virtual function implemented in bg subclass
    BattlegroundScoreMap::const_iterator itr = PlayerScores.find(Source->GetGUID());
    if (itr == PlayerScores.end())                         // player not found...
        return;

    switch (type)
    {
        case SCORE_KILLING_BLOWS:                           // Killing blows
            itr->second->KillingBlows += value;
            break;
        case SCORE_DEATHS:                                  // Deaths
            itr->second->Deaths += value;
            break;
        case SCORE_HONORABLE_KILLS:                         // Honorable kills
            itr->second->HonorableKills += value;
            break;
        case SCORE_BONUS_HONOR:                             // Honor bonus
            // do not add honor in arenas
            if (isBattleground())
            {
                // reward honor instantly
                if (doAddHonor)
                    Source->RewardHonor(NULL, 1, value);    // RewardHonor calls UpdatePlayerScore with doAddHonor = false
                else
                    itr->second->BonusHonor += value;
            }
            break;
            // used only in EY, but in MSG_PVP_LOG_DATA opcode
        case SCORE_DAMAGE_DONE:                             // Damage Done
            itr->second->DamageDone += value;
            break;
        case SCORE_HEALING_DONE:                            // Healing Done
            itr->second->HealingDone += value;
            break;
        /** World of Warcraft Armory **/
        case SCORE_DAMAGE_TAKEN:
            itr->second->DamageTaken += value;              // Damage Taken
            break;
        case SCORE_HEALING_TAKEN:
            itr->second->HealingTaken += value;             // Healing Taken
            break;
        /** World of Warcraft Armory **/
        default:
            TC_LOG_ERROR("bg", "Battleground::UpdatePlayerScore: unknown score type (%u) for BG (map: %u, instance id: %u)!",
                type, m_MapId, m_InstanceID);
            break;
    }
}

void Battleground::AddPlayerToResurrectQueue(uint64 npc_guid, uint64 player_guid)
{
    if (isRevival)
        return;

    m_ReviveQueue[npc_guid].push_back(player_guid);

    Player* player = ObjectAccessor::FindPlayer(player_guid);
    if (!player)
        return;

    player->CastSpell(player, SPELL_WAITING_FOR_RESURRECT, true);
}

void Battleground::RemovePlayerFromResurrectQueue(uint64 player_guid)
{
    if (isRevival)
        return;

    for (std::map<uint64, std::vector<uint64> >::iterator itr = m_ReviveQueue.begin(); itr != m_ReviveQueue.end(); ++itr)
    {
        for (std::vector<uint64>::iterator itr2 = (itr->second).begin(); itr2 != (itr->second).end(); ++itr2)
        {
            if (*itr2 == player_guid)
            {
                (itr->second).erase(itr2);
                if (Player* player = ObjectAccessor::FindPlayer(player_guid))
                    player->RemoveAurasDueToSpell(SPELL_WAITING_FOR_RESURRECT);
                return;
            }
        }
    }
}

bool Battleground::AddObject(uint32 type, uint32 entry, Position pos, Position rotation /*= { }*/, uint32 respawnTime /*= 0*/)
{
    return AddObject(type, entry, pos.m_positionX, pos.m_positionY, pos.m_positionZ, pos.m_orientation, rotation.m_positionX, rotation.m_positionY, rotation.m_positionZ, rotation.m_orientation, respawnTime);
}

bool Battleground::AddObject(uint32 type, uint32 entry, float x, float y, float z, float o, float rotation0, float rotation1, float rotation2, float rotation3, uint32 /*respawnTime*/)
{
    // If the assert is called, means that BgObjects must be resized!
    ASSERT(type < BgObjects.size());

    Map* map = FindBgMap();
    if (!map)
        return false;
    // Must be created this way, adding to godatamap would add it to the base map of the instance
    // and when loading it (in go::LoadFromDB()), a new guid would be assigned to the object, and a new object would be created
    // So we must create it specific for this instance
    GameObject* go = new GameObject;
    if (!go->Create(sObjectMgr->GenerateLowGuid(HIGHGUID_GAMEOBJECT), entry, GetBgMap(),
        PHASEMASK_NORMAL, x, y, z, o, rotation0, rotation1, rotation2, rotation3, 100, GO_STATE_READY))
    {
        TC_LOG_ERROR("sql", "Battleground::AddObject: cannot create gameobject (entry: %u) for BG (map: %u, instance id: %u)!",
                entry, m_MapId, m_InstanceID);
        TC_LOG_ERROR("bg", "Battleground::AddObject: cannot create gameobject (entry: %u) for BG (map: %u, instance id: %u)!",
                entry, m_MapId, m_InstanceID);
        delete go;
        return false;
    }
/*
    uint32 guid = go->GetGUIDLow();

    // without this, UseButtonOrDoor caused the crash, since it tried to get go info from godata
    // iirc that was changed, so adding to go data map is no longer required if that was the only function using godata from GameObject without checking if it existed
    GameObjectData& data = sObjectMgr->NewGOData(guid);

    data.id             = entry;
    data.mapid          = GetMapId();
    data.posX           = x;
    data.posY           = y;
    data.posZ           = z;
    data.orientation    = o;
    data.rotation0      = rotation0;
    data.rotation1      = rotation1;
    data.rotation2      = rotation2;
    data.rotation3      = rotation3;
    data.spawntimesecs  = respawnTime;
    data.spawnMask      = 1;
    data.animprogress   = 100;
    data.go_state       = 1;
*/
    if (go->IsTransport())
    {
        go->SetGoState(GO_STATE_ACTIVE);
        go->SetManualAnim(true);
    }

    // Add to world, so it can be later looked up from HashMapHolder
    if (!map->AddToMap(go))
    {
        delete go;
        return false;
    }
    BgObjects[type] = go->GetGUID();
    return true;
}

// Some doors aren't despawned so we cannot handle their closing in gameobject::update()
// It would be nice to correctly implement GO_ACTIVATED state and open/close doors in gameobject code
void Battleground::DoorClose(uint32 type)
{
    if (GameObject* obj = GetBgMap()->GetGameObject(BgObjects[type]))
    {
        // If doors are open, close it
        if (obj->getLootState() == GO_ACTIVATED && obj->GetGoState() != GO_STATE_READY)
        {
            obj->SetLootState(GO_READY);
            obj->SetGoState(GO_STATE_READY);
        }
    }
    else
        TC_LOG_ERROR("bg", "Battleground::DoorClose: door gameobject (type: %u, GUID: %u) not found for BG (map: %u, instance id: %u)!",
            type, GUID_LOPART(BgObjects[type]), m_MapId, m_InstanceID);
}

void Battleground::DoorOpen(uint32 type)
{
    if (GameObject* obj = GetBgMap()->GetGameObject(BgObjects[type]))
    {
        obj->SetLootState(GO_ACTIVATED);
        obj->SetGoState(GO_STATE_ACTIVE);
    }
    else
        TC_LOG_ERROR("bg", "Battleground::DoorOpen: door gameobject (type: %u, GUID: %u) not found for BG (map: %u, instance id: %u)!",
            type, GUID_LOPART(BgObjects[type]), m_MapId, m_InstanceID);
}

GameObject* Battleground::GetBGObject(uint32 type)
{
    GameObject* obj = GetBgMap()->GetGameObject(BgObjects[type]);
    if (!obj)
        TC_LOG_ERROR("bg", "Battleground::GetBGObject: gameobject (type: %u, GUID: %u) not found for BG (map: %u, instance id: %u)!",
            type, GUID_LOPART(BgObjects[type]), m_MapId, m_InstanceID);
    return obj;
}

Creature* Battleground::GetBGCreature(uint32 type)
{
    Creature* creature = GetBgMap()->GetCreature(BgCreatures[type]);
    if (!creature)
        TC_LOG_ERROR("bg", "Battleground::GetBGCreature: creature (type: %u, GUID: %u) not found for BG (map: %u, instance id: %u)!",
            type, GUID_LOPART(BgCreatures[type]), m_MapId, m_InstanceID);
    return creature;
}

void Battleground::SpawnBGObject(uint32 type, uint32 respawntime)
{
    if (Map* map = FindBgMap())
        if (GameObject* obj = map->GetGameObject(BgObjects[type]))
        {
            if (respawntime)
                obj->SetLootState(GO_JUST_DEACTIVATED);
            else
                if (obj->getLootState() == GO_JUST_DEACTIVATED)
                    // Change state from GO_JUST_DEACTIVATED to GO_READY in case battleground is starting again
                    obj->SetLootState(GO_READY);
            obj->SetRespawnTime(respawntime);
            map->AddToMap(obj);
        }
}

Creature* Battleground::AddCreature(uint32 entry, uint32 type, uint32 teamval, Position pos, uint32 respawntime /*= 0*/)
{
    return AddCreature(entry, type, teamval, pos.m_positionX, pos.m_positionY, pos.m_positionZ, pos.m_orientation, respawntime);
}

Creature* Battleground::AddCreature(uint32 entry, uint32 type, uint32 teamval, float x, float y, float z, float o, uint32 respawntime)
{
    // If the assert is called, means that BgCreatures must be resized!
    ASSERT(type < BgCreatures.size());

    Map* map = FindBgMap();
    if (!map)
        return NULL;

    Creature* creature = new Creature;
    if (!creature->Create(sObjectMgr->GenerateLowGuid(HIGHGUID_UNIT), map, PHASEMASK_NORMAL, entry, 0, teamval, x, y, z, o))
    {
        TC_LOG_ERROR("bg", "Battleground::AddCreature: cannot create creature (entry: %u) for BG (map: %u, instance id: %u)!",
            entry, m_MapId, m_InstanceID);
        delete creature;
        return NULL;
    }

    creature->SetHomePosition(x, y, z, o);

    CreatureTemplate const* cinfo = sObjectMgr->GetCreatureTemplate(entry);
    if (!cinfo)
    {
        TC_LOG_ERROR("bg", "Battleground::AddCreature: creature template (entry: %u) does not exist for BG (map: %u, instance id: %u)!",
            entry, m_MapId, m_InstanceID);
        delete creature;
        return NULL;
    }
    // Force using DB speeds
    creature->SetSpeed(MOVE_WALK,   cinfo->speed_walk);
    creature->SetSpeed(MOVE_RUN,    cinfo->speed_run);
    creature->SetSpeed(MOVE_FLIGHT, cinfo->speed_fly);

    if (!map->AddToMap(creature))
    {
        delete creature;
        return NULL;
    }

    BgCreatures[type] = creature->GetGUID();

    if (respawntime)
        creature->SetRespawnDelay(respawntime);

    return  creature;
}

bool Battleground::DelCreature(uint32 type)
{
    if (!BgCreatures[type])
        return true;

    if (Creature* creature = GetBgMap()->GetCreature(BgCreatures[type]))
    {
        creature->AddObjectToRemoveList();
        BgCreatures[type] = 0;
        return true;
    }

    TC_LOG_ERROR("bg", "Battleground::DelCreature: creature (type: %u, GUID: %u) not found for BG (map: %u, instance id: %u)!",
        type, GUID_LOPART(BgCreatures[type]), m_MapId, m_InstanceID);
    BgCreatures[type] = 0;
    return false;
}

bool Battleground::DelObject(uint32 type)
{
    if (!BgObjects[type])
        return true;

    if (GameObject* obj = GetBgMap()->GetGameObject(BgObjects[type]))
    {
        obj->SetRespawnTime(0);                                 // not save respawn time
        obj->Delete();
        BgObjects[type] = 0;
        return true;
    }
    TC_LOG_ERROR("bg", "Battleground::DelObject: gameobject (type: %u, GUID: %u) not found for BG (map: %u, instance id: %u)!",
        type, GUID_LOPART(BgObjects[type]), m_MapId, m_InstanceID);
    BgObjects[type] = 0;
    return false;
}

bool Battleground::AddSpiritGuide(uint32 type, float x, float y, float z, float o, uint32 team)
{
    uint32 entry = (team == ALLIANCE) ?
        BG_CREATURE_ENTRY_A_SPIRITGUIDE :
        BG_CREATURE_ENTRY_H_SPIRITGUIDE;

    if (Creature* creature = AddCreature(entry, type, team, x, y, z, o))
    {
        creature->setDeathState(DEAD);
        creature->SetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT, creature->GetGUID());
        // aura
        // TODO: Fix display here
        // creature->SetVisibleAura(0, SPELL_SPIRIT_HEAL_CHANNEL);
        // casting visual effect
        creature->SetUInt32Value(UNIT_CHANNEL_SPELL, SPELL_SPIRIT_HEAL_CHANNEL);
        // correct cast speed
        creature->SetFloatValue(UNIT_MOD_CAST_SPEED, 1.0f);
        creature->SetFloatValue(UNIT_MOD_CAST_HASTE, 1.0f);
        //creature->CastSpell(creature, SPELL_SPIRIT_HEAL_CHANNEL, true);
        return true;
    }
    TC_LOG_ERROR("bg", "Battleground::AddSpiritGuide: cannot create spirit guide (type: %u, entry: %u) for BG (map: %u, instance id: %u)!",
        type, entry, m_MapId, m_InstanceID);
    EndNow();
    return false;
}

void Battleground::SendMessageToAll(int32 entry, ChatMsg type, Player const* source)
{
    if (!entry)
        return;

    Trinity::BattlegroundChatBuilder bg_builder(type, entry, source);
    Trinity::LocalizedPacketDo<Trinity::BattlegroundChatBuilder> bg_do(bg_builder);
    BroadcastWorker(bg_do);
}

void Battleground::PSendMessageToAll(int32 entry, ChatMsg type, Player const* source, ...)
{
    if (!entry)
        return;

    va_list ap;
    va_start(ap, source);

    Trinity::BattlegroundChatBuilder bg_builder(type, entry, source, &ap);
    Trinity::LocalizedPacketDo<Trinity::BattlegroundChatBuilder> bg_do(bg_builder);
    BroadcastWorker(bg_do);

    va_end(ap);
}

void Battleground::SendWarningToAll(int32 entry, ...)
{
    if (!entry)
        return;

    const char *format = sObjectMgr->GetTrinityStringForDBCLocale(entry);

    char str[1024];
    va_list ap;
    va_start(ap, entry);
    vsnprintf(str, 1024, format, ap);
    va_end(ap);
    std::string msg(str);

    WorldPacket data;

    Trinity::ChatData c;
    c.chatType = CHAT_MSG_RAID_BOSS_EMOTE;
    c.message = msg;

    Trinity::BuildChatPacket(data, c);

    for (BattlegroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
        if (Player* player = ObjectAccessor::FindPlayer(MAKE_NEW_GUID(itr->first, 0, HIGHGUID_PLAYER)))
            if (player->GetSession())
                player->GetSession()->SendPacket(&data);
}

void Battleground::SendMessage2ToAll(int32 entry, ChatMsg type, Player const* source, int32 arg1, int32 arg2)
{
    Trinity::Battleground2ChatBuilder bg_builder(type, entry, source, arg1, arg2);
    Trinity::LocalizedPacketDo<Trinity::Battleground2ChatBuilder> bg_do(bg_builder);
    BroadcastWorker(bg_do);
}

void Battleground::EndNow()
{
    RemoveFromBGFreeSlotQueue();
    SetStatus(STATUS_WAIT_LEAVE);
    SetRemainingTime(0);
}

// To be removed
const char* Battleground::GetTrinityString(int32 entry)
{
    // FIXME: now we have different DBC locales and need localized message for each target client
    return sObjectMgr->GetTrinityStringForDBCLocale(entry);
}

// IMPORTANT NOTICE:
// buffs aren't spawned/despawned when players captures anything
// buffs are in their positions when battleground starts
void Battleground::HandleTriggerBuff(uint64 go_guid)
{
    if(!GetBgMap())
        return;
    GameObject* obj = GetBgMap()->GetGameObject(go_guid);
    if (!obj || obj->GetGoType() != GAMEOBJECT_TYPE_TRAP || !obj->isSpawned())
        return;

    // Change buff type, when buff is used:
    int32 index = BgObjects.size() - 1;
    while (index >= 0 && BgObjects[index] != go_guid)
        index--;
    if (index < 0)
    {
        TC_LOG_ERROR("bg", "Battleground::HandleTriggerBuff: cannot find buff gameobject (GUID: %u, entry: %u, type: %u) in internal data for BG (map: %u, instance id: %u)!",
            GUID_LOPART(go_guid), obj->GetEntry(), obj->GetGoType(), m_MapId, m_InstanceID);
        return;
    }

    // Randomly select new buff
    uint8 buff = urand(0, 2);
    uint32 entry = obj->GetEntry();
    if (m_BuffChange && entry != Buff_Entries[buff])
    {
        // Despawn current buff
        SpawnBGObject(index, RESPAWN_ONE_DAY);
        // Set index for new one
        for (uint8 currBuffTypeIndex = 0; currBuffTypeIndex < 3; ++currBuffTypeIndex)
            if (entry == Buff_Entries[currBuffTypeIndex])
            {
                index -= currBuffTypeIndex;
                index += buff;
            }
    }

    SpawnBGObject(index, BUFF_RESPAWN_TIME);
}

void Battleground::HandleKillPlayer(Player* victim, Player* killer)
{
    // Keep in mind that for arena this will have to be changed a bit

    // Add +1 deaths
    UpdatePlayerScore(victim, SCORE_DEATHS, 1);
    // Add +1 kills to group and +1 killing_blows to killer
    if (killer)
    {
        // Don't reward credit for killing ourselves, like fall damage of hellfire (warlock)
        if (killer == victim)
        {
              if (isBattleground() && sWorld->getBoolConfig(CONFIG_CUSTOM_BATTLEGROUND))   //против магов
              {
                 killer->DeMorph(); 
                 killer->ResetCustomDisplayId();
                 killer->SetObjectScale(1.0f);
                 killer->DestroyItemCount(SPELL_CUSTOM, 1, true);
                 killer->RemoveAura(spell_custom_2);  
                 CustomGUID = 0;
                 custom_exists = false;
               }
            return;
        }

        UpdatePlayerScore(killer, SCORE_HONORABLE_KILLS, 1);
        UpdatePlayerScore(killer, SCORE_KILLING_BLOWS, 1);

        for (BattlegroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
        {
            Player* creditedPlayer = ObjectAccessor::FindPlayer(itr->first);
            if (!creditedPlayer || creditedPlayer == killer)
                continue;

            if (creditedPlayer->GetBGTeam() == killer->GetBGTeam() && creditedPlayer->IsAtGroupRewardDistance(victim))
                UpdatePlayerScore(creditedPlayer, SCORE_HONORABLE_KILLS, 1);
        }
    }

    if (!isArena())
    {
        // To be able to remove insignia -- ONLY IN Battlegrounds
        victim->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);
        RewardXPAtKill(killer, victim);
    }
    
if (isBattleground() && sWorld->getBoolConfig(CONFIG_CUSTOM_BATTLEGROUND))   
{       
       if (killer->GetGUID() == CustomGUID) 
       {
          killer->AddAura(spell_custom_2, killer);
          killer->CastSpell(killer, spell_end, true);
          killer->AddItem(37711, 1);
          // выдача валюты 37711
       }
       
       if (victim->GetGUID() == CustomGUID) 
       {          
           victim->ResetCustomDisplayId();
           victim->DeMorph();
           victim->DestroyItemCount(SPELL_CUSTOM, 1, true);
           victim->RemoveAura(spell_custom_2);
           victim->SetObjectScale(1.0f);
           killer->Yell(TEXT, LANG_UNIVERSAL);
           killer->GetSession()->SendNotification(notification);
           ChatHandler(killer->GetSession()).PSendSysMessage(notification);
           if (killer->GetBGTeamId() == TEAM_ALLIANCE)
           {
               SendMessage2ToAll(LANG_SELECT_DEF, CHAT_MSG_BG_SYSTEM_ALLIANCE, killer);
               SendMessage2ToAll(LANG_SELECT_ATTACK, CHAT_MSG_BG_SYSTEM_HORDE, killer);
           }
           else
           {
               SendMessage2ToAll(LANG_SELECT_ATTACK, CHAT_MSG_BG_SYSTEM_ALLIANCE, killer);
               SendMessage2ToAll(LANG_SELECT_DEF, CHAT_MSG_BG_SYSTEM_HORDE, killer);              
           }
           killer->SetCustomDisplayId(MORPH_CUSTOM);
           killer->SetDisplayId(MORPH_CUSTOM, true);
           killer->AddItem(SPELL_CUSTOM, 1); //баф 
           killer->AddAura(spell_custom_2, killer);
           CustomGUID = killer->GetGUID();           
       } 
       
       if (!custom_exists && CustomGUID == 0)
       {
           killer->SetDisplayId(MORPH_CUSTOM, true);
           killer->SetCustomDisplayId(MORPH_CUSTOM); 
           killer->Yell(TEXT, LANG_UNIVERSAL);
           if (killer->GetBGTeamId() == TEAM_ALLIANCE)
           {
               SendMessage2ToAll(LANG_SELECT_DEF, CHAT_MSG_BG_SYSTEM_ALLIANCE, killer);
               SendMessage2ToAll(LANG_SELECT_ATTACK, CHAT_MSG_BG_SYSTEM_HORDE, killer);
           }
           else
           {
               SendMessage2ToAll(LANG_SELECT_ATTACK, CHAT_MSG_BG_SYSTEM_ALLIANCE, killer);
               SendMessage2ToAll(LANG_SELECT_DEF, CHAT_MSG_BG_SYSTEM_HORDE, killer);              
           }
           killer->GetSession()->SendNotification(notification); //анонс
           ChatHandler(killer->GetSession()).PSendSysMessage(notification); // в  чат
           killer->AddItem(SPELL_CUSTOM, 1); //баф 
           killer->AddAura(spell_custom_2, killer);
           CustomGUID = killer->GetGUID(); 
           custom_exists = true;           
       }
 }       
}

// Return the player's team based on battlegroundplayer info
// Used in same faction arena matches mainly
uint32 Battleground::GetPlayerTeam(uint64 guid) const
{
    BattlegroundPlayerMap::const_iterator itr = m_Players.find(guid);
    if (itr != m_Players.end())
        return itr->second.Team;
    return 0;
}

uint32 Battleground::GetOtherTeam(uint32 teamId) const
{
    return teamId ? ((teamId == ALLIANCE) ? HORDE : ALLIANCE) : 0;
}

bool Battleground::IsPlayerInBattleground(uint64 guid) const
{
    BattlegroundPlayerMap::const_iterator itr = m_Players.find(guid);
    if (itr != m_Players.end())
        return true;
    return false;
}

void Battleground::PlayerAddedToBGCheckIfBGIsRunning(Player* player)
{
    if (GetStatus() != STATUS_WAIT_LEAVE)
        return;

    WorldPacket data;
    BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(GetTypeID(), GetJoinType());

    BlockMovement(player);

    sBattlegroundMgr->BuildPvpLogDataPacket(&data, this);
    player->GetSession()->SendPacket(&data);


    sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, this, player, player->GetBattlegroundQueueIndex(bgQueueTypeId), STATUS_IN_PROGRESS, player->GetBattlegroundQueueJoinTime(GetTypeID()), GetElapsedTime(), GetJoinType());
    player->GetSession()->SendPacket(&data);
}

uint32 Battleground::GetAlivePlayersCountByTeam(uint32 Team) const
{
    int count = 0;
    for (BattlegroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        if (itr->second.Team == Team)
        {
            Player* player = ObjectAccessor::FindPlayer(itr->first);
            if (player && player->IsAlive() && !player->HasByteFlag(UNIT_FIELD_BYTES_2, 3, FORM_SPIRITOFREDEMPTION))
                ++count;
        }
    }
    return count;
}

void Battleground::SetHoliday(bool is_holiday)
{
    m_HonorMode = is_holiday ? BG_HOLIDAY : BG_NORMAL;
}

int32 Battleground::GetObjectType(uint64 guid)
{
    for (uint32 i = 0; i < BgObjects.size(); ++i)
        if (BgObjects[i] == guid)
            return i;
    TC_LOG_ERROR("bg", "Battleground::GetObjectType: player used gameobject (GUID: %u) which is not in internal data for BG (map: %u, instance id: %u), cheating?",
        GUID_LOPART(guid), m_MapId, m_InstanceID);
    return -1;
}

void Battleground::HandleKillUnit(Creature* /*creature*/, Player* /*killer*/)
{
}

void Battleground::CheckArenaAfterTimerConditions()
{
   if (GetJoinType() == 5 && sWorld->getBoolConfig(CONFIG_CUSTOM_FOOTBALL))  
   {
      if (ball)
      {
         if (ball->GetAuraCount(58169) > ball->GetAuraCount(65964))
            EndBattleground(HORDE);
         if (ball->GetAuraCount(65964) > ball->GetAuraCount(58169))
            EndBattleground(ALLIANCE);
      }
   }
       
    EndBattleground(WINNER_NONE);
       
}

void Battleground::CheckArenaWinConditions()
{
    if (!GetAlivePlayersCountByTeam(ALLIANCE) && GetPlayersCountByTeam(HORDE))
        EndBattleground(HORDE);
    else if (GetPlayersCountByTeam(ALLIANCE) && !GetAlivePlayersCountByTeam(HORDE))
        EndBattleground(ALLIANCE);
}

void Battleground::UpdateArenaWorldState()
{
    UpdateWorldState(0xe10, GetAlivePlayersCountByTeam(HORDE));
    UpdateWorldState(0xe11, GetAlivePlayersCountByTeam(ALLIANCE));
}

void Battleground::SetBgRaid(uint32 TeamID, Group* bg_raid)
{
    Group*& old_raid = TeamID == ALLIANCE ? m_BgRaids[BG_TEAM_ALLIANCE] : m_BgRaids[BG_TEAM_HORDE];
    if (old_raid)
        old_raid->SetBattlegroundGroup(NULL);
    if (bg_raid)
        bg_raid->SetBattlegroundGroup(this);
    old_raid = bg_raid;
}

WorldSafeLocsEntry const* Battleground::GetClosestGraveYard(Player* player)
{
    return sObjectMgr->GetClosestGraveYard(player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetMapId(), player->GetBGTeam());
}

bool Battleground::IsTeamScoreInRange(uint32 team, uint32 minScore, uint32 maxScore) const
{
    BattlegroundTeamId teamIndex = GetTeamIndexByTeamId(team);
    uint32 score = std::max(m_TeamScores[teamIndex], 0);
    return score >= minScore && score <= maxScore;
}

void Battleground::StartTimedAchievement(CriteriaTimedTypes type, uint32 entry)
{
    for (BattlegroundPlayerMap::const_iterator itr = GetPlayers().begin(); itr != GetPlayers().end(); ++itr)
        if (Player* player = ObjectAccessor::FindPlayer(itr->first))
            player->GetAchievementMgr().StartTimedAchievement(type, entry);
}

void Battleground::SetBracket(PvPDifficultyEntry const* bracketEntry)
{
    m_BracketId = bracketEntry->GetBracketId();
    SetLevelRange(bracketEntry->minLevel, bracketEntry->maxLevel);
}

void Battleground::RewardXPAtKill(Player* killer, Player* victim)
{
    if (sWorld->getBoolConfig(CONFIG_BG_XP_FOR_KILL) && killer && victim)
        killer->RewardPlayerAndGroupAtKill(victim, true);
}

void Battleground::SendFlagsPositionsUpdate(uint32 diff)
{
    if (m_flagCarrierTime > int(diff))
    {
        m_flagCarrierTime -= diff;
        return;
    }
    m_flagCarrierTime = FLAGS_UPDATE;

    uint32 flagCarrierCount = 0;
    Player* FlagCarrier[2];

    for(uint8 i = 0; i < 2; ++i)
    {
        FlagCarrier[i] = NULL;
        if (uint64 guid = GetFlagPickerGUID(i))
        {
            FlagCarrier[TEAM_ALLIANCE] = ObjectAccessor::FindPlayer(guid);
            if (FlagCarrier[i])
                ++flagCarrierCount;
        }
    }

    WorldPacket packet(SMSG_BATTLEGROUND_PLAYER_POSITIONS);
    packet.WriteBits(flagCarrierCount, 20);

    ObjectGuid guids[2];
    for (uint8 i = 0; i < 2; ++i)
    {
        Player* player = FlagCarrier[i];
        if (!player)
            continue;

        guids[i] = player->GetGUID();
        packet.WriteGuidMask<6, 5, 4, 0, 2, 3, 7, 1>(guids[i]);
    }

    packet.FlushBits();

    for (uint8 i = 0; i < 2; ++i)
    {
        Player* player = FlagCarrier[i];
        if (!player)
            continue;

        packet << player->GetPositionY();
        packet.WriteGuidBytes<2, 3, 7, 0, 1, 6>(guids[i]);
        packet << uint8(player->GetBGTeamId() == TEAM_ALLIANCE ? 1 : 2);
        packet.WriteGuidBytes<5, 4>(guids[i]);
        packet << uint8(player->GetBGTeamId() == TEAM_ALLIANCE ? 3 : 2);
        packet << player->GetPositionX();
    }

    SendPacketToAll(&packet);
}

void Battleground::SendOpponentSpecialization(uint32 team)
{
    uint32 opCoun = 0;
    ByteBuffer dataBuffer;
    WorldPacket spec(SMSG_ARENA_OPPONENT_SPECIALIZATIONS, 65);  //send us info about opponents specID
    spec.WriteBits(opCoun, 21);

    for (BattlegroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        if (Player* opponent = _GetPlayerForTeam(team, itr, "SendOponentSpecialization"))
        {
            spec.WriteGuidMask<7, 1, 2, 3, 5, 4, 6, 0>(opponent->GetObjectGuid());
            dataBuffer << uint32(opponent->GetSpecializationId(opponent->GetActiveSpec()));
            dataBuffer.WriteGuidBytes<5, 1, 6, 3, 7, 4, 0, 2>(opponent->GetObjectGuid());
            ++opCoun;
        }
    }

    spec.FlushBits();
    spec.append(dataBuffer);
    spec.PutBits<uint32>(0, opCoun, 21);

    SendPacketToTeam(GetOtherTeam(team), &spec);
}

void Battleground::UpdateArenaVision()
{
    for (BattlegroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
        if (Player* player = sObjectAccessor->FindPlayer(itr->first))
        {
            player->AddAura(108887, player);
            player->CastSpell(player, 108888, true);
        }
}
