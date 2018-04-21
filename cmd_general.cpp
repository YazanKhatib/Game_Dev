#include "stdafx.h"
#ifdef __FreeBSD__
#include <md5.h>
#else
#include <string>
#include "../../libthecore/include/xmd5.h"
#endif
#include "utils.h"
#include "config.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "char.h"
#include "char_manager.h"
#include "motion.h"
#include "packet.h"
#include "affect.h"
#include "pvp.h"
#include "start_position.h"
#include "party.h"
#include "guild_manager.h"
#include "p2p.h"
#include "dungeon.h"
#include "messenger_manager.h"
#include "war_map.h"
#include "questmanager.h"
#include "item_manager.h"
#include "monarch.h"
#include "mob_manager.h"
#include "dev_log.h"
#include "item.h"
#include "arena.h"
#include "buffer_manager.h"
#include "unique_item.h"
#include "threeway_war.h"
#include "log.h"
#if defined(WJ_COMBAT_ZONE)	
#include "combat_zone.h"
#endif
#include "../../common/VnumHelper.h"
#ifdef __AUCTION__
#include "auction_manager.h"
#endif
#ifdef ENABLE_MOUNT_SYSTEM
#include "MountSystem.h"
#endif
#include "offlineshop_manager.h"
#ifdef NEW_PET_SYSTEM
#include "New_PetSystem.h"
#endif

extern int g_server_id;
extern int g_nPortalLimitTime;

ACMD(do_user_horse_ride)
{
	if (ch->IsObserverMode())
		return;

	if (ch->IsDead() || ch->IsStun())
		return;

	if (ch->IsHorseRiding() == false)
	{
		// ∏ª¿Ã æ∆¥— ¥Ÿ∏•≈ª∞Õ¿ª ≈∏∞Ì¿÷¥Ÿ.
		if (ch->GetMountVnum())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("¿ÃπÃ ≈ª∞Õ¿ª ¿ÃøÎ¡ﬂ¿‘¥œ¥Ÿ."));
			return;
		}

		if (ch->GetHorse() == NULL)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("∏ª¿ª ∏’¿˙ º“»Ø«ÿ¡÷ººø‰."));
			return;
		}

		ch->StartRiding();
	}
	else
	{
		ch->StopRiding();
	}
}

ACMD(do_user_horse_back)
{
	if (ch->GetHorse() != NULL)
	{
		ch->HorseSummon(false);
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("∏ª¿ª µπ∑¡∫∏≥¬Ω¿¥œ¥Ÿ."));
	}
	else if (ch->IsHorseRiding() == true)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("∏ªø°º≠ ∏’¿˙ ≥ª∑¡æﬂ «’¥œ¥Ÿ."));
	}
	else
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("∏ª¿ª ∏’¿˙ º“»Ø«ÿ¡÷ººø‰."));
	}
}

ACMD(do_user_horse_feed)
{
	// ∞≥¿ŒªÛ¡°¿ª ø¨ ªÛ≈¬ø°º≠¥¬ ∏ª ∏‘¿Ã∏¶ ¡Ÿ ºˆ æ¯¥Ÿ.
	if (ch->GetMyShop())
		return;

	if (ch->GetHorse() == NULL)
	{
		if (ch->IsHorseRiding() == false)
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("∏ª¿ª ∏’¿˙ º“»Ø«ÿ¡÷ººø‰."));
		else
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("∏ª¿ª ≈∫ ªÛ≈¬ø°º≠¥¬ ∏‘¿Ã∏¶ ¡Ÿ ºˆ æ¯Ω¿¥œ¥Ÿ."));
		return;
	}

	DWORD dwFood = ch->GetHorseGrade() + 50054 - 1;

	if (ch->CountSpecifyItem(dwFood) > 0)
	{
		ch->RemoveSpecifyItem(dwFood, 1);
		ch->FeedHorse();
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("∏ªø°∞‘ %s%s ¡÷æ˙Ω¿¥œ¥Ÿ."), 
				ITEM_MANAGER::instance().GetTable(dwFood)->szLocaleName,
				g_iUseLocale ? "" : under_han(ITEM_MANAGER::instance().GetTable(dwFood)->szLocaleName) ? LC_TEXT("¿ª") : LC_TEXT("∏¶"));
	}
	else
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s æ∆¿Ã≈€¿Ã « ø‰«’¥œ¥Ÿ"), ITEM_MANAGER::instance().GetTable(dwFood)->szLocaleName);
	}
}

#define MAX_REASON_LEN		128

EVENTINFO(TimedEventInfo)
{
	DynamicCharacterPtr ch;
	int		subcmd;
	int         	left_second;
	char		szReason[MAX_REASON_LEN];

	TimedEventInfo() 
	: ch()
	, subcmd( 0 )
	, left_second( 0 )
	{
		::memset( szReason, 0, MAX_REASON_LEN );
	}
};

struct SendDisconnectFunc
{
	void operator () (LPDESC d)
	{
		if (d->GetCharacter())
		{
			if (d->GetCharacter()->GetGMLevel() == GM_PLAYER)
				d->GetCharacter()->ChatPacket(CHAT_TYPE_COMMAND, "quit Shutdown(SendDisconnectFunc)");
		}
	}
};

struct DisconnectFunc
{
	void operator () (LPDESC d)
	{
		if (d->GetType() == DESC_TYPE_CONNECTOR)
			return;

		if (d->IsPhase(PHASE_P2P))
			return;

		if (d->GetCharacter())
			d->GetCharacter()->Disconnect("Shutdown(DisconnectFunc)");

		d->SetPhase(PHASE_CLOSE);
	}
};

EVENTINFO(shutdown_event_data)
{
	int seconds;

	shutdown_event_data()
	: seconds( 0 )
	{
	}
};

EVENTFUNC(shutdown_event)
{
	shutdown_event_data* info = dynamic_cast<shutdown_event_data*>( event->info );

	if ( info == NULL )
	{
		sys_err( "shutdown_event> <Factor> Null pointer" );
		return 0;
	}

	int * pSec = & (info->seconds);

	if (*pSec < 0)
	{
		sys_log(0, "shutdown_event sec %d", *pSec);

		if (--*pSec == -10)
		{
			const DESC_MANAGER::DESC_SET & c_set_desc = DESC_MANAGER::instance().GetClientSet();
			std::for_each(c_set_desc.begin(), c_set_desc.end(), DisconnectFunc());
			return passes_per_sec;
		}
		else if (*pSec < -10)
			return 0;

		return passes_per_sec;
	}
	else if (*pSec == 0)
	{
		const DESC_MANAGER::DESC_SET & c_set_desc = DESC_MANAGER::instance().GetClientSet();
		std::for_each(c_set_desc.begin(), c_set_desc.end(), SendDisconnectFunc());
		g_bNoMoreClient = true;
		--*pSec;
		return passes_per_sec;
	}
	else
	{
		char buf[64];
		snprintf(buf, sizeof(buf), LC_TEXT("ºÀ¥ŸøÓ¿Ã %d√  ≥≤æ“Ω¿¥œ¥Ÿ."), *pSec);
		SendNotice(buf);

		--*pSec;
		return passes_per_sec;
	}
}

void Shutdown(int iSec)
{
	if (g_bNoMoreClient)
	{
		thecore_shutdown();
		return;
	}

	CWarMapManager::instance().OnShutdown();

	char buf[64];
	snprintf(buf, sizeof(buf), LC_TEXT("%d√  »ƒ ∞‘¿”¿Ã ºÀ¥ŸøÓ µÀ¥œ¥Ÿ."), iSec);

	SendNotice(buf);

	shutdown_event_data* info = AllocEventInfo<shutdown_event_data>();
	info->seconds = iSec;

	event_create(shutdown_event, info, 1);
}

ACMD(do_shutdown)
{
	if (NULL == ch)
	{
		sys_err("Accept shutdown command from %s.", ch->GetName());
	}
	TPacketGGShutdown p;
	p.bHeader = HEADER_GG_SHUTDOWN;
	P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGShutdown));

	Shutdown(30);
}

EVENTFUNC(timed_event)
{
	TimedEventInfo * info = dynamic_cast<TimedEventInfo *>( event->info );
	
	if ( info == NULL )
	{
		sys_err( "timed_event> <Factor> Null pointer" );
		return 0;
	}

	LPCHARACTER	ch = info->ch;
	if (ch == NULL) { // <Factor>
		return 0;
	}
	LPDESC d = ch->GetDesc();

	if (info->left_second <= 0)
	{
		ch->m_pkTimedEvent = NULL;

		if (true == LC_IsEurope() || true == LC_IsYMIR() || true == LC_IsKorea())
		{
			switch (info->subcmd)
			{
				case SCMD_LOGOUT:
				case SCMD_QUIT:
				case SCMD_PHASE_SELECT:
					{
						TPacketNeedLoginLogInfo acc_info;
						acc_info.dwPlayerID = ch->GetDesc()->GetAccountTable().id;

						db_clientdesc->DBPacket( HEADER_GD_VALID_LOGOUT, 0, &acc_info, sizeof(acc_info) );

						LogManager::instance().DetailLoginLog( false, ch );
					}
					break;
			}
		}

		switch (info->subcmd)
		{
			case SCMD_LOGOUT:
				if (d)
					d->SetPhase(PHASE_CLOSE);
				break;

			case SCMD_QUIT:
				ch->ChatPacket(CHAT_TYPE_COMMAND, "quit");
				break;

			case SCMD_PHASE_SELECT:
				{
					ch->Disconnect("timed_event - SCMD_PHASE_SELECT");

					if (d)
					{
						d->SetPhase(PHASE_SELECT);
					}
				}
				break;
		}

		return 0;
	}
	else
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%d√  ≥≤æ“Ω¿¥œ¥Ÿ."), info->left_second);
		--info->left_second;
	}

	return PASSES_PER_SEC(1);
}

ACMD(do_cmd)
{
	/* RECALL_DELAY
	   if (ch->m_pkRecallEvent != NULL)
	   {
	   ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("√Îº“ µ«æ˙Ω¿¥œ¥Ÿ."));
	   event_cancel(&ch->m_pkRecallEvent);
	   return;
	   }
	// END_OF_RECALL_DELAY */

	if (ch->m_pkTimedEvent)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("√Îº“ µ«æ˙Ω¿¥œ¥Ÿ."));
		event_cancel(&ch->m_pkTimedEvent);
		return;
	}

	switch (subcmd)
	{
		case SCMD_LOGOUT:
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("∑Œ±◊¿Œ »≠∏È¿∏∑Œ µπæ∆ ∞©¥œ¥Ÿ. ¿·Ω√∏∏ ±‚¥Ÿ∏Æººø‰."));
			break;

		case SCMD_QUIT:
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("∞‘¿”¿ª ¡æ∑· «’¥œ¥Ÿ. ¿·Ω√∏∏ ±‚¥Ÿ∏Æººø‰."));
			break;

		case SCMD_PHASE_SELECT:
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ƒ≥∏Ø≈Õ∏¶ ¿¸»Ø «’¥œ¥Ÿ. ¿·Ω√∏∏ ±‚¥Ÿ∏Æººø‰."));
			break;
	}

	int nExitLimitTime = 10;

	if (ch->IsHack(false, true, nExitLimitTime) &&
		false == CThreeWayWar::instance().IsSungZiMapIndex(ch->GetMapIndex()) &&
	   	(!ch->GetWarMap() || ch->GetWarMap()->GetType() == GUILD_WAR_TYPE_FLAG))
	{
		return;
	}
	
	switch (subcmd)
	{
		case SCMD_LOGOUT:
		case SCMD_QUIT:
		case SCMD_PHASE_SELECT:
			{
				TimedEventInfo* info = AllocEventInfo<TimedEventInfo>();

				{
					if (ch->IsPosition(POS_FIGHTING))
						info->left_second = 10;
					else
						info->left_second = 3;
				}

				info->ch		= ch;
				info->subcmd		= subcmd;
				strlcpy(info->szReason, argument, sizeof(info->szReason));

				ch->m_pkTimedEvent	= event_create(timed_event, info, 1);
			}
			break;
	}
}

ACMD(do_mount)
{
	/*
	   char			arg1[256];
	   struct action_mount_param	param;

	// ¿ÃπÃ ≈∏∞Ì ¿÷¿∏∏È
	if (ch->GetMountingChr())
	{
	char arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
	return;

	param.x		= atoi(arg1);
	param.y		= atoi(arg2);
	param.vid	= ch->GetMountingChr()->GetVID();
	param.is_unmount = true;

	float distance = DISTANCE_SQRT(param.x - (DWORD) ch->GetX(), param.y - (DWORD) ch->GetY());

	if (distance > 600.0f)
	{
	ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("¡ª ¥ı ∞°±Ó¿Ã ∞°º≠ ≥ª∏Æººø‰."));
	return;
	}

	action_enqueue(ch, ACTION_TYPE_MOUNT, &param, 0.0f, true);
	return;
	}

	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	return;

	LPCHARACTER tch = CHARACTER_MANAGER::instance().Find(atoi(arg1));

	if (!tch->IsNPC() || !tch->IsMountable())
	{
	ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("∞≈±‚ø°¥¬ ≈ª ºˆ æ¯æÓø‰."));
	return;
	}

	float distance = DISTANCE_SQRT(tch->GetX() - ch->GetX(), tch->GetY() - ch->GetY());

	if (distance > 600.0f)
	{
	ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("¡ª ¥ı ∞°±Ó¿Ã ∞°º≠ ≈∏ººø‰."));
	return;
	}

	param.vid		= tch->GetVID();
	param.is_unmount	= false;

	action_enqueue(ch, ACTION_TYPE_MOUNT, &param, 0.0f, true);
	 */
}

ACMD(do_fishing)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	ch->SetRotation(atof(arg1));
	ch->fishing();
}

ACMD(do_console)
{
	ch->ChatPacket(CHAT_TYPE_COMMAND, "ConsoleEnable");
}

ACMD(do_restart)
{
	if (false == ch->IsDead())
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "CloseRestartWindow");
		ch->StartRecoveryEvent();
		return;
	}

	if (NULL == ch->m_pkDeadEvent)
		return;

	int iTimeToDead = (event_time(ch->m_pkDeadEvent) / passes_per_sec);
#if defined(WJ_COMBAT_ZONE)	
	if (CCombatZoneManager::Instance().IsCombatZoneMap(ch->GetMapIndex()))
	{
		CCombatZoneManager::Instance().OnRestart(ch, subcmd);
		return;
	}
#endif
	if (subcmd != SCMD_RESTART_TOWN && (!ch->GetWarMap() || ch->GetWarMap()->GetType() == GUILD_WAR_TYPE_FLAG))
	{
		if (!test_server)
		{
			if (ch->IsHack())
			{
				//º∫¡ˆ ∏ ¿œ∞ÊøÏø°¥¬ √º≈© «œ¡ˆ æ ¥¬¥Ÿ.
				if (false == CThreeWayWar::instance().IsSungZiMapIndex(ch->GetMapIndex()))
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("æ∆¡˜ ¿ÁΩ√¿€ «“ ºˆ æ¯Ω¿¥œ¥Ÿ. (%d√  ≥≤¿Ω)"), iTimeToDead - (180 - g_nPortalLimitTime));
					return;
				}
			}

			if (iTimeToDead > 170)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("æ∆¡˜ ¿ÁΩ√¿€ «“ ºˆ æ¯Ω¿¥œ¥Ÿ. (%d√  ≥≤¿Ω)"), iTimeToDead - 170);
				return;
			}
		}
	}

	//PREVENT_HACK
	//DESC : √¢∞Ì, ±≥»Ø √¢ »ƒ ∆˜≈ª¿ª ªÁøÎ«œ¥¬ πˆ±◊ø° ¿ÃøÎµ…ºˆ ¿÷æÓº≠
	//		ƒ≈∏¿”¿ª √ﬂ∞° 
	if (subcmd == SCMD_RESTART_TOWN)
	{
		if (ch->IsHack())
		{
			//±ÊµÂ∏ , º∫¡ˆ∏ ø°º≠¥¬ √º≈© «œ¡ˆ æ ¥¬¥Ÿ.
			if ((!ch->GetWarMap() || ch->GetWarMap()->GetType() == GUILD_WAR_TYPE_FLAG) ||
			   	false == CThreeWayWar::instance().IsSungZiMapIndex(ch->GetMapIndex()))
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("æ∆¡˜ ¿ÁΩ√¿€ «“ ºˆ æ¯Ω¿¥œ¥Ÿ. (%d√  ≥≤¿Ω)"), iTimeToDead - (180 - g_nPortalLimitTime));
				return;
			}
		}

		if (iTimeToDead > 173)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("æ∆¡˜ ∏∂¿ªø°º≠ ¿ÁΩ√¿€ «“ ºˆ æ¯Ω¿¥œ¥Ÿ. (%d √  ≥≤¿Ω)"), iTimeToDead - 173);
			return;
		}
	}
	//END_PREVENT_HACK

	ch->ChatPacket(CHAT_TYPE_COMMAND, "CloseRestartWindow");

	ch->GetDesc()->SetPhase(PHASE_GAME);
	ch->SetPosition(POS_STANDING);
	ch->StartRecoveryEvent();

	//FORKED_LOAD
	//DESC: ªÔ∞≈∏Æ ¿¸≈ıΩ√ ∫Œ»∞¿ª «“∞ÊøÏ ∏ ¿« ¿‘±∏∞° æ∆¥— ªÔ∞≈∏Æ ¿¸≈ı¿« Ω√¿€¡ˆ¡°¿∏∑Œ ¿Ãµø«œ∞‘ µ»¥Ÿ.
	if (1 == quest::CQuestManager::instance().GetEventFlag("threeway_war"))
	{
		if (subcmd == SCMD_RESTART_TOWN || subcmd == SCMD_RESTART_HERE)
		{
			if (true == CThreeWayWar::instance().IsThreeWayWarMapIndex(ch->GetMapIndex()) &&
					false == CThreeWayWar::instance().IsSungZiMapIndex(ch->GetMapIndex()))
			{
				ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));

				ch->ReviveInvisible(5);
#ifdef __MOUNT_SYSTEM__
				ch->CheckMount();
#endif
				ch->PointChange(POINT_HP, ch->GetMaxHP() - ch->GetHP());
				ch->PointChange(POINT_SP, ch->GetMaxSP() - ch->GetSP());

				return;
			}

			//º∫¡ˆ 
			if (true == CThreeWayWar::instance().IsSungZiMapIndex(ch->GetMapIndex()))
			{
				if (CThreeWayWar::instance().GetReviveTokenForPlayer(ch->GetPlayerID()) <= 0)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("º∫¡ˆø°º≠ ∫Œ»∞ ±‚»∏∏¶ ∏µŒ ¿“æ˙Ω¿¥œ¥Ÿ! ∏∂¿ª∑Œ ¿Ãµø«’¥œ¥Ÿ!"));
					ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));
				}
				else
				{
					ch->Show(ch->GetMapIndex(), GetSungziStartX(ch->GetEmpire()), GetSungziStartY(ch->GetEmpire()));
				}

				ch->PointChange(POINT_HP, ch->GetMaxHP() - ch->GetHP());
				ch->PointChange(POINT_SP, ch->GetMaxSP() - ch->GetSP());
				ch->ReviveInvisible(5);
#ifdef __MOUNT_SYSTEM__
				ch->CheckMount();
#endif

				return;
			}
		}
	}
	//END_FORKED_LOAD

	if (ch->GetDungeon())
		ch->GetDungeon()->UseRevive(ch);

	if (ch->GetWarMap() && !ch->IsObserverMode())
	{
		CWarMap * pMap = ch->GetWarMap();
		DWORD dwGuildOpponent = pMap ? pMap->GetGuildOpponent(ch) : 0;

		if (dwGuildOpponent)
		{
			switch (subcmd)
			{
				case SCMD_RESTART_TOWN:
					sys_log(0, "do_restart: restart town");
					PIXEL_POSITION pos;

					if (CWarMapManager::instance().GetStartPosition(ch->GetMapIndex(), ch->GetGuild()->GetID() < dwGuildOpponent ? 0 : 1, pos))
						ch->Show(ch->GetMapIndex(), pos.x, pos.y);
					else
						ch->ExitToSavedLocation();

					ch->PointChange(POINT_HP, ch->GetMaxHP() - ch->GetHP());
					ch->PointChange(POINT_SP, ch->GetMaxSP() - ch->GetSP());
					ch->ReviveInvisible(5);
#ifdef __MOUNT_SYSTEM__
					ch->CheckMount();
#endif
					break;

				case SCMD_RESTART_HERE:
					sys_log(0, "do_restart: restart here");
					ch->RestartAtSamePos();
					//ch->Show(ch->GetMapIndex(), ch->GetX(), ch->GetY());
					ch->PointChange(POINT_HP, ch->GetMaxHP() - ch->GetHP());
					ch->PointChange(POINT_SP, ch->GetMaxSP() - ch->GetSP());
					ch->ReviveInvisible(5);
#ifdef __MOUNT_SYSTEM__
					ch->CheckMount();
#endif
					break;
			}

			return;
		}
	}

	switch (subcmd)
	{
		case SCMD_RESTART_TOWN:
			sys_log(0, "do_restart: restart town");
			PIXEL_POSITION pos;
			


	//	if (ch->GetEmpire() == 1) 
	///	ch->WarpSet(464100,906100,1);
	//	else if (ch->GetEmpire() == 2)
	///	ch->WarpSet(472000,940000,1);	
	////	else if (ch->GetEmpire() == 3)
	//	ch->WarpSet(450600,949700,1);
	//	ch->ChatPacket(CHAT_TYPE_INFO, " %d : %d  ",pos.x,pos.y);

	//	ch->ChatPacket(CHAT_TYPE_INFO, " %d : %d  ",EMPIRE_START_X(ch->GetEmpire()),EMPIRE_START_Y(ch->GetEmpire()));
		
			if (SECTREE_MANAGER::instance().GetRecallPositionByEmpire(ch->GetMapIndex(), ch->GetEmpire(), pos))
			ch->WarpSet(pos.x, pos.y);
		//	ch->ChatPacket(CHAT_TYPE_INFO, " %d : %d  ",pos.x,pos.y);

		else
				ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));

	




			ch->PointChange(POINT_HP, 50 - ch->GetHP());
			ch->DeathPenalty(1);
			break;

		case SCMD_RESTART_HERE:
			sys_log(0, "do_restart: restart here");
			ch->RestartAtSamePos();
			//ch->Show(ch->GetMapIndex(), ch->GetX(), ch->GetY());
			ch->PointChange(POINT_HP, ch->GetMaxHP() - ch->GetHP());
			ch->PointChange(POINT_SP, ch->GetMaxSP() - ch->GetSP());
			ch->ReviveInvisible(5);
#ifdef __MOUNT_SYSTEM__
			ch->CheckMount();
#endif
			break;
	}
}
// Statuler kaca kadar Á˝kacak
#define MAX_STAT 90

ACMD(do_stat_reset)
{
	ch->PointChange(POINT_STAT_RESET_COUNT, 12 - ch->GetPoint(POINT_STAT_RESET_COUNT));
}

ACMD(do_stat_minus)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	if (ch->IsPolymorphed())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("µ–∞© ¡ﬂø°¥¬ ¥…∑¬¿ª ø√∏± ºˆ æ¯Ω¿¥œ¥Ÿ."));
		return;
	}

	if (ch->GetPoint(POINT_STAT_RESET_COUNT) <= 0)
		return;

	if (!strcmp(arg1, "st"))
	{
		if (ch->GetRealPoint(POINT_ST) <= JobInitialPoints[ch->GetJob()].st)
			return;

		ch->SetRealPoint(POINT_ST, ch->GetRealPoint(POINT_ST) - 1);
		ch->SetPoint(POINT_ST, ch->GetPoint(POINT_ST) - 1);
		ch->ComputePoints();
		ch->PointChange(POINT_ST, 0);
	}
	else if (!strcmp(arg1, "dx"))
	{
		if (ch->GetRealPoint(POINT_DX) <= JobInitialPoints[ch->GetJob()].dx)
			return;

		ch->SetRealPoint(POINT_DX, ch->GetRealPoint(POINT_DX) - 1);
		ch->SetPoint(POINT_DX, ch->GetPoint(POINT_DX) - 1);
		ch->ComputePoints();
		ch->PointChange(POINT_DX, 0);
	}
	else if (!strcmp(arg1, "ht"))
	{
		if (ch->GetRealPoint(POINT_HT) <= JobInitialPoints[ch->GetJob()].ht)
			return;

		ch->SetRealPoint(POINT_HT, ch->GetRealPoint(POINT_HT) - 1);
		ch->SetPoint(POINT_HT, ch->GetPoint(POINT_HT) - 1);
		ch->ComputePoints();
		ch->PointChange(POINT_HT, 0);
		ch->PointChange(POINT_MAX_HP, 0);
	}
	else if (!strcmp(arg1, "iq"))
	{
		if (ch->GetRealPoint(POINT_IQ) <= JobInitialPoints[ch->GetJob()].iq)
			return;

		ch->SetRealPoint(POINT_IQ, ch->GetRealPoint(POINT_IQ) - 1);
		ch->SetPoint(POINT_IQ, ch->GetPoint(POINT_IQ) - 1);
		ch->ComputePoints();
		ch->PointChange(POINT_IQ, 0);
		ch->PointChange(POINT_MAX_SP, 0);
	}
	else
		return;

	ch->PointChange(POINT_STAT, +1);
	ch->PointChange(POINT_STAT_RESET_COUNT, -1);
	ch->ComputePoints();
}

ACMD(do_stat)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	if (ch->IsPolymorphed())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("µ–∞© ¡ﬂø°¥¬ ¥…∑¬¿ª ø√∏± ºˆ æ¯Ω¿¥œ¥Ÿ."));
		return;
	}

	if (ch->GetPoint(POINT_STAT) <= 0)
		return;

	BYTE idx = 0;
	
	if (!strcmp(arg1, "st"))
		idx = POINT_ST;
	else if (!strcmp(arg1, "dx"))
		idx = POINT_DX;
	else if (!strcmp(arg1, "ht"))
		idx = POINT_HT;
	else if (!strcmp(arg1, "iq"))
		idx = POINT_IQ;
	else
		return;

	if (ch->GetRealPoint(idx) >= MAX_STAT)
		return;

	ch->SetRealPoint(idx, ch->GetRealPoint(idx) + 1);
	ch->SetPoint(idx, ch->GetPoint(idx) + 1);
	ch->ComputePoints();
	ch->PointChange(idx, 0);

	if (idx == POINT_IQ)
	{
		ch->PointChange(POINT_MAX_HP, 0);
	}
	else if (idx == POINT_HT)
	{
		ch->PointChange(POINT_MAX_SP, 0);
	}

	ch->PointChange(POINT_STAT, -1);
	ch->ComputePoints();
}


#ifdef ENABLE_PVP_ADVANCED
#include <string>
#include <algorithm>
#include <boost/algorithm/string/replace.hpp>
const char* m_nDuelTranslate[] = /* «‰  can translate here */
{
	"[«‰ ] ÌÃ» «‰ ÌﬂÊ‰ —ﬁ„ .",
	"[«‰ ] «·—ﬁ„ «·„Ê÷Ê⁄ «ﬁ· „‰ 0.", 
	"[«‰ ] «·—ﬁ„ «·„Ê÷Ê⁄ «ﬂ»— „‰ ﬁÌ„… «·⁄Ÿ„Ï ··Ì«‰€.",
	"[«‰ ] «·„»·€ „Ê÷Ê⁄ «ﬁ· „‰ „«  „·ﬂ.",
	"[«‰ ]  «·„»·€ «·„Ê÷Ê⁄ «ﬂ»— „‰  „Ã„Ê⁄ —’Ìœ «·Õ«·Ì + „»·€ «·„Ê÷Ê⁄.",
	"[«·Œ’„] «·„»·€ «·„Ê÷Ê⁄ «ﬂ»— „‰  „Ã„Ê⁄ —’Ìœ «·Õ«·Ì + „»·€ «·„Ê÷Ê⁄.",
	"[«·Œ’„] «·„»·€ «·–Ì ﬁ„  »Ê÷⁄Â ·«Ì„·ﬂÂ «·Œ’„.",
	" √‰  «·√‰ »„»«—“….",
	"«·Œ’„ »„»«—“… «·√‰ ·«Ì„ﬂ‰ﬂ «—”«· œ⁄Ê….",
	"—ƒÌ… «·⁄ «œ Êÿ·» «·„»«—“…  „ ÕŸ—Â »· ›⁄·.",
	" „ ÕŸ— —ƒÌ… «·⁄ «œ Êÿ·» «·„»«—“… √’»Õ „ÕŸÊ— ·«Ì„ﬂ‰ ·√Õœ ÿ·» „»«—“… „‰ﬂ ”Ì’·ﬂ «‘⁄«— ·Ê  „ ÿ·» „»«—“… „‰ﬂ.",
	"—ƒÌ… «·⁄ «œ Êÿ·» «·„»«—“… √’»Õ €Ì— „ÕŸÊ— Ì„ﬂ‰ ··«⁄»Ì‰ ÿ·» „»«—“… Ê—ƒÌ… ⁄ «œ Œ«’ »ﬂ.",
	"—ƒÌ… «·⁄ «œ Êÿ·» «·„»«—“… €Ì— „ÕŸÊ—."	
}; 


const char* szTableStaticPvP[] = {BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT};

void Duel_SendMessage(LPCHARACTER ch, const char* msg)
{
	if (!ch)
		return;

	std::string textLine;
	
	textLine = msg;
	boost::algorithm::replace_all(textLine, " ", "$");
				 
	char buf[512+1];
	snprintf(buf, sizeof(buf), "BINARY_Duel_SendMessage %s", textLine.c_str());
	ch->ChatPacket(CHAT_TYPE_COMMAND, buf);
}	

ACMD(do_pvp)
{

	if (!ch)
		return;

#if defined(WJ_COMBAT_ZONE)	
	if (CCombatZoneManager::Instance().IsCombatZoneMap(ch->GetMapIndex()))
		return;
#endif

if (ch->GetMapIndex() == 4) 
return;

if (ch->GetMapIndex() == 27) 
return;
	
	if (ch->GetArena() != NULL || CArenaManager::instance().IsArenaMap(ch->GetMapIndex()) == true)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("????? ???? ? ????."));
		return;
	}

	char arg1[256], arg2[256], arg3[256], arg4[256], arg5[256], arg6[256], arg7[256], arg8[256], arg9[256], arg10[256];
	
	pvp_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2), arg3, sizeof(arg3), arg4, sizeof(arg4), arg5, sizeof(arg5), arg6, sizeof(arg6), arg7, sizeof(arg7), arg8, sizeof(arg8), arg9, sizeof(arg9), arg10, sizeof(arg10));	
	
	DWORD vid = 0;
	str_to_number(vid, arg1);	
	LPCHARACTER pkVictim = CHARACTER_MANAGER::instance().Find(vid);

	if (!pkVictim)
		return;

	if (pkVictim->IsNPC())
		return;

	if (pkVictim->GetArena() != NULL)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("???? ??????."));
		return;
	}
	
	if (ch->GetExchange() || pkVictim->GetExchange())
	{
		CPVPManager::instance().Decline(ch, pkVictim);
		CPVPManager::instance().Decline(pkVictim, ch);
		return;
	}
	
	if (*arg2 && !strcmp(arg2, "accept"))
	{	
		int chA_nBetMoney = ch->GetQuestFlag(szTableStaticPvP[8]);
		int chB_nBetMoney = pkVictim->GetQuestFlag(szTableStaticPvP[8]);

		if ((ch->GetGold() < chA_nBetMoney) || (pkVictim->GetGold() < chB_nBetMoney))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "<PvP> Can't start duel because something is wrong with bet money.");
			pkVictim->ChatPacket(CHAT_TYPE_INFO, "<PvP> Can't start duel because something is wrong with bet money.");
			CPVPManager::instance().Decline(ch, pkVictim);
			CPVPManager::instance().Decline(pkVictim, ch);
			return;
		}

		ch->SetDuel("IsFight", 1);
		pkVictim->SetDuel("IsFight", 1);
		
		if (chA_nBetMoney > 0 && chA_nBetMoney > 0)
		{
			ch->PointChange(POINT_GOLD, - chA_nBetMoney, true);	
			pkVictim->PointChange(POINT_GOLD, - chB_nBetMoney, true);	
		}
		
		CPVPManager::instance().Insert(ch, pkVictim);
		return;
	}
	
	int m_BlockChangeItem = 0, m_BlockBuff = 0, m_BlockPotion = 0, m_BlockRide = 0, m_BlockPet = 0, m_BlockPoly = 0, m_BlockParty = 0, m_BlockExchange = 0, m_BetMoney = 0;
	
	str_to_number(m_BlockChangeItem, arg2);
	str_to_number(m_BlockBuff, arg3);
	str_to_number(m_BlockPotion, arg4);
	str_to_number(m_BlockRide, arg5);
	str_to_number(m_BlockPet, arg6);
	str_to_number(m_BlockPoly, arg7);
	str_to_number(m_BlockParty, arg8);
	str_to_number(m_BlockExchange, arg9);
	str_to_number(m_BetMoney, arg10);
	
	if (!isdigit(*arg2) && !isdigit(*arg3) && !isdigit(*arg4) && !isdigit(*arg5) && !isdigit(*arg6) && !isdigit(*arg7) && !isdigit(*arg8) && !isdigit(*arg9) && !isdigit(*arg10))
	{
		Duel_SendMessage(ch, m_nDuelTranslate[0]);
		return;
	}	
	
	if (m_BetMoney < 0)
	{
		Duel_SendMessage(ch, m_nDuelTranslate[1]);
		return;
	}	
	
	if (m_BetMoney >= GOLD_MAX)
	{
		Duel_SendMessage(ch, m_nDuelTranslate[2]);
		return;
	}
	
	if (ch->GetGold() < m_BetMoney)
	{
		Duel_SendMessage(ch, m_nDuelTranslate[3]);
		return;
	}
	
	if ((ch->GetGold() + m_BetMoney) > GOLD_MAX)
	{
		Duel_SendMessage(ch, m_nDuelTranslate[4]);
		return;
	}
	
	if ((pkVictim->GetGold() + m_BetMoney) > GOLD_MAX)
	{
		Duel_SendMessage(ch, m_nDuelTranslate[5]);		
		return;
	}
	
	if (pkVictim->GetGold() < m_BetMoney)
	{
		Duel_SendMessage(ch, m_nDuelTranslate[6]);
		return;
	}
	
	if (*arg1 && *arg2 && *arg3 && *arg4 && *arg5 && *arg6 && *arg7 && *arg8 && *arg9 && *arg10)
	{
		ch->SetDuel("BlockChangeItem", m_BlockChangeItem);			ch->SetDuel("BlockBuff", m_BlockBuff);
		ch->SetDuel("BlockPotion", m_BlockPotion);					ch->SetDuel("BlockRide", m_BlockRide);
		ch->SetDuel("BlockPet", m_BlockPet);						ch->SetDuel("BlockPoly", m_BlockPoly);	
		ch->SetDuel("BlockParty", m_BlockParty);					ch->SetDuel("BlockExchange", m_BlockExchange);
		ch->SetDuel("BetMoney", m_BetMoney);

		pkVictim->SetDuel("BlockChangeItem", m_BlockChangeItem);	pkVictim->SetDuel("BlockBuff", m_BlockBuff);
		pkVictim->SetDuel("BlockPotion", m_BlockPotion);			pkVictim->SetDuel("BlockRide", m_BlockRide);
		pkVictim->SetDuel("BlockPet", m_BlockPet);					pkVictim->SetDuel("BlockPoly", m_BlockPoly);	
		pkVictim->SetDuel("BlockParty", m_BlockParty);				pkVictim->SetDuel("BlockExchange", m_BlockExchange);
		pkVictim->SetDuel("BetMoney", m_BetMoney);
			
		CPVPManager::instance().Insert(ch, pkVictim); 
	}	
}

ACMD(do_pvp_advanced)
{   

	if (!ch)
		return;


#if defined(WJ_COMBAT_ZONE)	
	if (CCombatZoneManager::Instance().IsCombatZoneMap(ch->GetMapIndex()))
		return;
#endif
if (ch->GetMapIndex() == 4) 
return;

if (ch->GetMapIndex() == 27) 
return;

	if (ch->GetArena() != NULL || CArenaManager::instance().IsArenaMap(ch->GetMapIndex()) == true)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("????? ???? ? ????."));
		return;
	}
	
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	DWORD vid = 0;
	str_to_number(vid, arg1);
	LPCHARACTER pkVictim = CHARACTER_MANAGER::instance().Find(vid);
	
	if(pkVictim->GetQuestFlag(BLOCK_EQUIPMENT_) < 1)
	{

    if (!pkVictim)
        return;

    if (pkVictim->IsNPC())
        return;

	if (pkVictim->GetArena() != NULL)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("???? ??????."));
		return;
	}
	
	if (ch->GetQuestFlag(szTableStaticPvP[9]) > 0)
	{
		Duel_SendMessage(ch, m_nDuelTranslate[7]);
		return;
	}
	
	if (pkVictim->GetQuestFlag(szTableStaticPvP[9]) > 0)
	{
		Duel_SendMessage(ch, m_nDuelTranslate[8]);
		return;
	}
	
	int statusEq = pkVictim->GetQuestFlag(BLOCK_EQUIPMENT_);
	
	CGuild * g = pkVictim->GetGuild();

	const char* m_Name = pkVictim->GetName();	
	const char* m_GuildName = "-";
		
	int m_Vid = pkVictim->GetVID();	
	int m_Level = pkVictim->GetLevel();
	int m_PlayTime = pkVictim->GetRealPoint(POINT_PLAYTIME);
	int m_MaxHP = pkVictim->GetMaxHP();
	int m_MaxSP = pkVictim->GetMaxSP();
	
	DWORD m_Race = pkVictim->GetRaceNum();
	
	if (g)
	{ 
		ch->ChatPacket(CHAT_TYPE_COMMAND, "BINARY_Duel_GetInfo %d %s %s %d %d %d %d %d", m_Vid, m_Name, g->GetName(), m_Level, m_Race, m_PlayTime, m_MaxHP, m_MaxSP);
		
		if (statusEq < 1)
			pkVictim->SendEquipment(ch);
	}
	else
		ch->ChatPacket(CHAT_TYPE_COMMAND, "BINARY_Duel_GetInfo %d %s %s %d %d %d %d %d", m_Vid, m_Name, m_GuildName, m_Level, m_Race, m_PlayTime, m_MaxHP, m_MaxSP);
		
		if (statusEq < 1)
			pkVictim->SendEquipment(ch);
			}
			else
			{
				pkVictim->ChatPacket(CHAT_TYPE_INFO, " ﬁ«„ «·«⁄» %s »„Õ«Ê·… ÿ·» „»«—“… „‰ﬂ ·ﬂ‰ﬂ ﬁ„  »«€·«ﬁ ‰Ÿ«„ «·„»«—“… < „»«—“… > " , ch->GetName());
				ch->ChatPacket(CHAT_TYPE_INFO, "  ﬁ«„ «·«⁄» %s »«€·«ﬁ ‰Ÿ«„ „»«—“… Œ«’… »Â <„»«—“…> ",pkVictim->GetName());
			}
}

ACMD(do_decline_pvp)
{
	if (!ch)
		return;

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	
	if (!*arg1)
		return;
	
	DWORD vid = 0;
	str_to_number(vid, arg1);
	
	LPCHARACTER pkVictim = CHARACTER_MANAGER::instance().Find(vid);
	
	if (!pkVictim)
		return;
	
	if (pkVictim->IsNPC())
		return;
	
	CPVPManager::instance().Decline(ch, pkVictim);
}

ACMD(do_block_equipment)
{
	if (!ch)
		return;

	char arg1[256];
	one_argument (argument, arg1, sizeof(arg1));
	
	if (!ch->IsPC() || NULL == ch)
		return;
	
	int statusEq = ch->GetQuestFlag(BLOCK_EQUIPMENT_);
	
	if (!strcmp(arg1, "BLOCK"))
	{	
		if (statusEq > 0)
		{	
			Duel_SendMessage(ch, m_nDuelTranslate[9]);
			return;
		}	
		else
			ch->SetQuestFlag(BLOCK_EQUIPMENT_, 1);
			Duel_SendMessage(ch, m_nDuelTranslate[10]);
	}
	
	if (!strcmp(arg1, "UNBLOCK"))
	{
		if (statusEq == 0)
		{	
			Duel_SendMessage(ch, m_nDuelTranslate[12]);
			return;
		}	
		else	
			ch->SetQuestFlag(BLOCK_EQUIPMENT_, 0);
			Duel_SendMessage(ch, m_nDuelTranslate[11]);
	}
}
#endif


ACMD(do_guildskillup)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	if (!ch->GetGuild())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ±ÊµÂø° º”«ÿ¿÷¡ˆ æ Ω¿¥œ¥Ÿ."));
		return;
	}

	CGuild* g = ch->GetGuild();
	TGuildMember* gm = g->GetMember(ch->GetPlayerID());
	if (gm->grade == GUILD_LEADER_GRADE)
	{
		DWORD vnum = 0;
		str_to_number(vnum, arg1);
		g->SkillLevelUp(vnum);
	}
	else
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ±ÊµÂ Ω∫≈≥ ∑π∫ß¿ª ∫Ø∞Ê«“ ±««—¿Ã æ¯Ω¿¥œ¥Ÿ."));
	}
}

ACMD(do_skillup)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	DWORD vnum = 0;
	str_to_number(vnum, arg1);

	if (true == ch->CanUseSkill(vnum))
	{
		ch->SkillLevelUp(vnum);
	}
	else
	{
		switch(vnum)
		{
			case SKILL_HORSE_WILDATTACK:
			case SKILL_HORSE_CHARGE:
			case SKILL_HORSE_ESCAPE:
			case SKILL_HORSE_WILDATTACK_RANGE:
			case SKILL_ADD_HP:
			case SKILL_RESIST_PENETRATE:
#ifdef __7AND8TH_SKILLS__
			case SKILL_ANTI_PALBANG:
			case SKILL_ANTI_AMSEOP:
			case SKILL_ANTI_SWAERYUNG:
			case SKILL_ANTI_YONGBI:
			case SKILL_ANTI_GIGONGCHAM:
			case SKILL_ANTI_HWAJO:
			case SKILL_ANTI_MARYUNG:
			case SKILL_ANTI_BYEURAK:
#ifdef __WOLFMAN_CHARACTER__
			case SKILL_ANTI_SALPOONG:
#endif
			case SKILL_HELP_PALBANG:
			case SKILL_HELP_AMSEOP:
			case SKILL_HELP_SWAERYUNG:
			case SKILL_HELP_YONGBI:
			case SKILL_HELP_GIGONGCHAM:
			case SKILL_HELP_HWAJO:
			case SKILL_HELP_MARYUNG:
			case SKILL_HELP_BYEURAK:
#ifdef __WOLFMAN_CHARACTER__
			case SKILL_HELP_SALPOONG:
#endif
#endif
				ch->SkillLevelUp(vnum);
				break;
		}
	}
}

//
// @version	05/06/20 Bang2ni - ƒø∏«µÂ √≥∏Æ Delegate to CHARACTER class
//
ACMD(do_safebox_close)
{
	ch->CloseSafebox();
}

//
// @version	05/06/20 Bang2ni - ƒø∏«µÂ √≥∏Æ Delegate to CHARACTER class
//
ACMD(do_safebox_password)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	ch->ReqSafeboxLoad(arg1);
}

ACMD(do_safebox_change_password)
{
	char arg1[256];
	char arg2[256];

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || strlen(arg1)>6)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<√¢∞Ì> ¿ﬂ∏¯µ» æœ»£∏¶ ¿‘∑¬«œºÃΩ¿¥œ¥Ÿ."));
		return;
	}

	if (!*arg2 || strlen(arg2)>6)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<√¢∞Ì> ¿ﬂ∏¯µ» æœ»£∏¶ ¿‘∑¬«œºÃΩ¿¥œ¥Ÿ."));
		return;
	}

	if (LC_IsBrazil() == true)
	{
		for (int i = 0; i < 6; ++i)
		{
			if (arg2[i] == '\0')
				break;

			if (isalpha(arg2[i]) == false)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<√¢∞Ì> ∫Òπ–π¯»£¥¬ øµπÆ¿⁄∏∏ ∞°¥…«’¥œ¥Ÿ."));
				return;
			}
		}
	}

	TSafeboxChangePasswordPacket p;

	p.dwID = ch->GetDesc()->GetAccountTable().id;
	strlcpy(p.szOldPassword, arg1, sizeof(p.szOldPassword));
	strlcpy(p.szNewPassword, arg2, sizeof(p.szNewPassword));

	db_clientdesc->DBPacket(HEADER_GD_SAFEBOX_CHANGE_PASSWORD, ch->GetDesc()->GetHandle(), &p, sizeof(p));
}

ACMD(do_mall_password)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1 || strlen(arg1) > 6)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<√¢∞Ì> ¿ﬂ∏¯µ» æœ»£∏¶ ¿‘∑¬«œºÃΩ¿¥œ¥Ÿ."));
		return;
	}

	int iPulse = thecore_pulse();

	if (ch->GetMall())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<√¢∞Ì> √¢∞Ì∞° ¿ÃπÃ ø≠∑¡¿÷Ω¿¥œ¥Ÿ."));
		return;
	}

	if (iPulse - ch->GetMallLoadTime() < passes_per_sec * 10) // 10√ ø° «—π¯∏∏ ø‰√ª ∞°¥…
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<√¢∞Ì> √¢∞Ì∏¶ ¥›¿∫¡ˆ 10√  æ»ø°¥¬ ø≠ ºˆ æ¯Ω¿¥œ¥Ÿ."));
		return;
	}

	ch->SetMallLoadTime(iPulse);

	TSafeboxLoadPacket p;
	p.dwID = ch->GetDesc()->GetAccountTable().id;
	strlcpy(p.szLogin, ch->GetDesc()->GetAccountTable().login, sizeof(p.szLogin));
	strlcpy(p.szPassword, arg1, sizeof(p.szPassword));

	db_clientdesc->DBPacket(HEADER_GD_MALL_LOAD, ch->GetDesc()->GetHandle(), &p, sizeof(p));
}

ACMD(do_mall_close)
{
	if (ch->GetMall())
	{
		ch->SetMallLoadTime(thecore_pulse());
		ch->CloseMall();
		ch->Save();
	}
}

ACMD(do_ungroup)
{
	if (!ch->GetParty())
		return;

	if (!CPartyManager::instance().IsEnablePCParty())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<∆ƒ∆º> º≠πˆ πÆ¡¶∑Œ ∆ƒ∆º ∞¸∑√ √≥∏Æ∏¶ «“ ºˆ æ¯Ω¿¥œ¥Ÿ."));
		return;
	}

	if (ch->GetDungeon())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<∆ƒ∆º> ¥¯¿¸ æ»ø°º≠¥¬ ∆ƒ∆ºø°º≠ ≥™∞• ºˆ æ¯Ω¿¥œ¥Ÿ."));
		return;
	}

	LPPARTY pParty = ch->GetParty();

	if (pParty->GetMemberCount() == 2)
	{
		// party disband
		CPartyManager::instance().DeleteParty(pParty);
	}
	else
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<∆ƒ∆º> ∆ƒ∆ºø°º≠ ≥™∞°ºÃΩ¿¥œ¥Ÿ."));
		//pParty->SendPartyRemoveOneToAll(ch);
		pParty->Quit(ch->GetPlayerID());
		//pParty->SendPartyRemoveAllToOne(ch);
	}
}

ACMD(do_close_shop)
{
	if (ch->GetMyShop())
	{
		ch->CloseMyShop();
		return;
	}
}

ACMD(do_set_walk_mode)
{
	ch->SetNowWalking(true);
	ch->SetWalking(true);
}

ACMD(do_set_run_mode)
{
	ch->SetNowWalking(false);
	ch->SetWalking(false);
}

ACMD(do_war)
{
	//≥ª ±ÊµÂ ¡§∫∏∏¶ æÚæÓø¿∞Ì
	CGuild * g = ch->GetGuild();

	if (!g)
		return;

	//¿¸¿Ô¡ﬂ¿Œ¡ˆ √º≈©«—π¯!
	if (g->UnderAnyWar())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ¿ÃπÃ ¥Ÿ∏• ¿¸¿Ôø° ¬¸¿¸ ¡ﬂ ¿‘¥œ¥Ÿ."));
		return;
	}

	//∆ƒ∂Û∏ﬁ≈Õ∏¶ µŒπË∑Œ ≥™¥©∞Ì
	char arg1[256], arg2[256];
	int type = GUILD_WAR_TYPE_FIELD;
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
		return;

	if (*arg2)
	{
		str_to_number(type, arg2);

	// War loncaadi -1111111 server cokme fix - Yakup
	if (type >= GUILD_WAR_TYPE_MAX_NUM || type < 0)
		type = GUILD_WAR_TYPE_FIELD;
	}

	//±ÊµÂ¿« ∏∂Ω∫≈Õ æ∆¿Ãµ∏¶ æÚæÓø¬µ⁄
	DWORD gm_pid = g->GetMasterPID();

	//∏∂Ω∫≈Õ¿Œ¡ˆ √º≈©(±Ê¿¸¿∫ ±ÊµÂ¿Â∏∏¿Ã ∞°¥…)
	if (gm_pid != ch->GetPlayerID())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ±ÊµÂ¿¸ø° ¥Î«— ±««—¿Ã æ¯Ω¿¥œ¥Ÿ."));
		return;
	}

	//ªÛ¥Î ±ÊµÂ∏¶ æÚæÓø¿∞Ì
	CGuild * opp_g = CGuildManager::instance().FindGuildByName(arg1);

	if (!opp_g)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ±◊∑± ±ÊµÂ∞° æ¯Ω¿¥œ¥Ÿ."));
		return;
	}

	//ªÛ¥Î±ÊµÂøÕ¿« ªÛ≈¬ √º≈©
	switch (g->GetGuildWarState(opp_g->GetID()))
	{
		case GUILD_WAR_NONE:
			{
				if (opp_g->UnderAnyWar())
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ªÛ¥ÎπÊ ±ÊµÂ∞° ¿ÃπÃ ¿¸¿Ô ¡ﬂ ¿‘¥œ¥Ÿ."));
					return;
				}

				int iWarPrice = KOR_aGuildWarInfo[type].iWarPrice;

				if (g->GetGuildMoney() < iWarPrice)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ¿¸∫Ò∞° ∫Œ¡∑«œø© ±ÊµÂ¿¸¿ª «“ ºˆ æ¯Ω¿¥œ¥Ÿ."));
					return;
				}

				if (opp_g->GetGuildMoney() < iWarPrice)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ªÛ¥ÎπÊ ±ÊµÂ¿« ¿¸∫Ò∞° ∫Œ¡∑«œø© ±ÊµÂ¿¸¿ª «“ ºˆ æ¯Ω¿¥œ¥Ÿ."));
					return;
				}
			}
			break;

		case GUILD_WAR_SEND_DECLARE:
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("¿ÃπÃ º±¿¸∆˜∞Ì ¡ﬂ¿Œ ±ÊµÂ¿‘¥œ¥Ÿ."));
				return;
			}
			break;

		case GUILD_WAR_RECV_DECLARE:
			{
				if (opp_g->UnderAnyWar())
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ªÛ¥ÎπÊ ±ÊµÂ∞° ¿ÃπÃ ¿¸¿Ô ¡ﬂ ¿‘¥œ¥Ÿ."));
					g->RequestRefuseWar(opp_g->GetID());
					return;
				}
			}
			break;

		case GUILD_WAR_RESERVE:
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ¿ÃπÃ ¿¸¿Ô¿Ã øπæ‡µ» ±ÊµÂ ¿‘¥œ¥Ÿ."));
				return;
			}
			break;

		case GUILD_WAR_END:
			return;

		default:
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ¿ÃπÃ ¿¸¿Ô ¡ﬂ¿Œ ±ÊµÂ¿‘¥œ¥Ÿ."));
			g->RequestRefuseWar(opp_g->GetID());
			return;
	}

	if (!g->CanStartWar(type))
	{
		// ±ÊµÂ¿¸¿ª «“ ºˆ ¿÷¥¬ ¡∂∞«¿ª ∏∏¡∑«œ¡ˆæ ¥¬¥Ÿ.
		if (g->GetLadderPoint() == 0)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ∑π¥ı ¡°ºˆ∞° ∏¿⁄∂Ûº≠ ±ÊµÂ¿¸¿ª «“ ºˆ æ¯Ω¿¥œ¥Ÿ."));
			sys_log(0, "GuildWar.StartError.NEED_LADDER_POINT");
		}
		else if (g->GetMemberCount() < GUILD_WAR_MIN_MEMBER_COUNT)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ±ÊµÂ¿¸¿ª «œ±‚ ¿ß«ÿº± √÷º“«— %d∏Ì¿Ã ¿÷æÓæﬂ «’¥œ¥Ÿ."), GUILD_WAR_MIN_MEMBER_COUNT);
			sys_log(0, "GuildWar.StartError.NEED_MINIMUM_MEMBER[%d]", GUILD_WAR_MIN_MEMBER_COUNT);
		}
		else
		{
			sys_log(0, "GuildWar.StartError.UNKNOWN_ERROR");
		}
		return;
	}

	// « µÂ¿¸ √º≈©∏∏ «œ∞Ì ºººº«— √º≈©¥¬ ªÛ¥ÎπÊ¿Ã Ω¬≥´«“∂ß «—¥Ÿ.
	if (!opp_g->CanStartWar(GUILD_WAR_TYPE_FIELD))
	{
		if (opp_g->GetLadderPoint() == 0)
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ªÛ¥ÎπÊ ±ÊµÂ¿« ∑π¥ı ¡°ºˆ∞° ∏¿⁄∂Ûº≠ ±ÊµÂ¿¸¿ª «“ ºˆ æ¯Ω¿¥œ¥Ÿ."));
		else if (opp_g->GetMemberCount() < GUILD_WAR_MIN_MEMBER_COUNT)
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ªÛ¥ÎπÊ ±ÊµÂ¿« ±ÊµÂø¯ ºˆ∞° ∫Œ¡∑«œø© ±ÊµÂ¿¸¿ª «“ ºˆ æ¯Ω¿¥œ¥Ÿ."));
		return;
	}

	do
	{
		if (g->GetMasterCharacter() != NULL)
			break;

		CCI *pCCI = P2P_MANAGER::instance().FindByPID(g->GetMasterPID());

		if (pCCI != NULL)
			break;

		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ªÛ¥ÎπÊ ±ÊµÂ¿« ±ÊµÂ¿Â¿Ã ¡¢º”¡ﬂ¿Ã æ∆¥’¥œ¥Ÿ."));
		g->RequestRefuseWar(opp_g->GetID());
		return;

	} while (false);

	do
	{
		if (opp_g->GetMasterCharacter() != NULL)
			break;
		
		CCI *pCCI = P2P_MANAGER::instance().FindByPID(opp_g->GetMasterPID());
		
		if (pCCI != NULL)
			break;

		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ªÛ¥ÎπÊ ±ÊµÂ¿« ±ÊµÂ¿Â¿Ã ¡¢º”¡ﬂ¿Ã æ∆¥’¥œ¥Ÿ."));
		g->RequestRefuseWar(opp_g->GetID());
		return;

	} while (false);

	g->RequestDeclareWar(opp_g->GetID(), type);
}

ACMD(do_nowar)
{
	CGuild* g = ch->GetGuild();
	if (!g)
		return;

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	DWORD gm_pid = g->GetMasterPID();

	if (gm_pid != ch->GetPlayerID())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ±ÊµÂ¿¸ø° ¥Î«— ±««—¿Ã æ¯Ω¿¥œ¥Ÿ."));
		return;
	}

	CGuild* opp_g = CGuildManager::instance().FindGuildByName(arg1);

	if (!opp_g)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<±ÊµÂ> ±◊∑± ±ÊµÂ∞° æ¯Ω¿¥œ¥Ÿ."));
		return;
	}

	g->RequestRefuseWar(opp_g->GetID());
}

ACMD(do_detaillog)
{
	ch->DetailLog();
}

ACMD(do_monsterlog)
{
	ch->ToggleMonsterLog();
}

ACMD(do_pkmode)
{




	if (ch->GetMapIndex() == 4 || ch->GetMapIndex() == 27 ){


	  ch->SetPKMode(PK_MODE_FREE);
  	  return;

}


#ifdef ENABLE_PVP_ADVANCED
const char* szTableStaticPvP[] = {BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT};
if (ch->GetQuestFlag(szTableStaticPvP[9]) == 1 )
return;
#endif


	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	BYTE mode = 0;
	str_to_number(mode, arg1);

	if (mode == PK_MODE_PROTECT)
		return;

	if (ch->GetLevel() < PK_PROTECT_LEVEL && mode != 0)
		return;
#if defined(WJ_COMBAT_ZONE)	
	if (CCombatZoneManager::Instance().IsCombatZoneMap(ch->GetMapIndex()))
		return;
#endif
	ch->SetPKMode(mode);
}

ACMD(do_messenger_auth)
{
	if (ch->GetArena())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("¥Î∑√¿Âø°º≠ ªÁøÎ«œΩ« ºˆ æ¯Ω¿¥œ¥Ÿ."));
		return;
	}

	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
		return;

	char answer = LOWER(*arg1);

	if (answer != 'y')
	{
		LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(arg2);

		if (tch)
			tch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s ¥‘¿∏∑Œ ∫Œ≈Õ ƒ£±∏ µÓ∑œ¿ª ∞≈∫Œ ¥Á«ﬂΩ¿¥œ¥Ÿ."), ch->GetName());
	}

	MessengerManager::instance().AuthToAdd(ch->GetName(), arg2, answer == 'y' ? false : true); // DENY
}

ACMD(do_setblockmode)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (*arg1)
	{
		BYTE flag = 0;
		str_to_number(flag, arg1);
		ch->SetBlockMode(flag);
	}
}

ACMD(do_unmount)
{
#ifdef __MOUNT_SYSTEM__
	short sCheck = ch->UnEquipMountUniqueItem();
	if (sCheck == 2)
		ch->Dismount();
	else if (ch->UnEquipSpecialRideUniqueItem())
#else
	if (ch->UnEquipSpecialRideUniqueItem())
#endif
	{
		ch->RemoveAffect(AFFECT_MOUNT);
		ch->RemoveAffect(AFFECT_MOUNT_BONUS);
		if (ch->IsHorseRiding())
		{
			ch->StopRiding();
		}
	}
#ifdef __MOUNT_SYSTEM__
	else if (sCheck == 0 && ch->GetPoint(POINT_MOUNT) > 0)
	{
		ch->Dismount();
	}
#endif
}

ACMD(do_observer_exit)
{
	if (ch->IsObserverMode())
	{
		if (ch->GetWarMap())
			ch->SetWarMap(NULL);

		if (ch->GetArena() != NULL || ch->GetArenaObserverMode() == true)
		{
			ch->SetArenaObserverMode(false);

			if (ch->GetArena() != NULL)
				ch->GetArena()->RemoveObserver(ch->GetPlayerID());

			ch->SetArena(NULL);
			ch->WarpSet(ARENA_RETURN_POINT_X(ch->GetEmpire()), ARENA_RETURN_POINT_Y(ch->GetEmpire()));
		}
		else
		{
			ch->ExitToSavedLocation();
		}
		ch->SetObserverMode(false);
	}
}

ACMD(do_view_equip)
{


	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (*arg1)
	{
		DWORD vid = 0;
		str_to_number(vid, arg1);
		LPCHARACTER tch = CHARACTER_MANAGER::instance().Find(vid);

		if (!tch)
			return;

		if (!tch->IsPC())
			return;
		/*

		

		   int iSPCost = ch->GetMaxSP() / 3;

		   if (ch->GetSP() < iSPCost)
		   {
		   ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("¡§Ω≈∑¬¿Ã ∫Œ¡∑«œø© ¥Ÿ∏• ªÁ∂˜¿« ¿Â∫Ò∏¶ ∫º ºˆ æ¯Ω¿¥œ¥Ÿ."));
		   return;
		   }
		   ch->PointChange(POINT_SP, -iSPCost);
		 */





		int cheecker = tch->GetQuestFlag("block_abojad.tmp");
		if (cheecker == 1 ) 
		{
		
		ch->ChatPacket(CHAT_TYPE_INFO, "ﬁ«„ «·«⁄» »«€·«ﬁ œ—Ê⁄Â");


		return;
		}




int	st = tch -> GetPoint(POINT_ST);
int	ht = tch -> GetPoint(POINT_HT);
int	dx = tch -> GetPoint(POINT_DX);
int	iq = tch -> GetPoint(POINT_IQ);


ch->ChatPacket(CHAT_TYPE_INFO, " --------------------------------- ");

ch->ChatPacket(CHAT_TYPE_INFO, " %d : ‰ﬁ«ÿ Ê÷⁄ Œ’„ ÕÌÊÌ…  ",ht);
ch->ChatPacket(CHAT_TYPE_INFO, " %d : ‰ﬁ«ÿ Ê÷⁄ Œ’„ –ﬂ«¡  ",iq);
ch->ChatPacket(CHAT_TYPE_INFO, " %d : ‰ﬁ«ÿ Ê÷⁄ Œ’„ „‰«Ê—…  ",dx);
ch->ChatPacket(CHAT_TYPE_INFO, " %d : ‰ﬁ«ÿ Ê÷⁄ Œ’„ ﬁÊ…  ",st);


if (tch->FindAffect(AFFECT_DRAGON_SOUL_DECK_0))
ch->ChatPacket(CHAT_TYPE_INFO, "  Â–« ·«⁄» „›⁄· «·ﬂÌ„Ì«∆Ì Œ«‰… —ﬁ„ 1  ");
else if (tch->FindAffect(AFFECT_DRAGON_SOUL_DECK_1))
ch->ChatPacket(CHAT_TYPE_INFO, " Â–« ·«⁄» „›⁄· «·ﬂÌ„Ì«∆Ì Œ«‰… —ﬁ„ 2  ");
else 
ch->ChatPacket(CHAT_TYPE_INFO, " Â–«  ·«⁄» €Ì— „›⁄· ﬂÌ„Ì«∆Ì ");


if (tch->FindAffect(AFFECT_BLEND))
ch -> ChatPacket(CHAT_TYPE_INFO, "  Â–« ·«⁄» Ì” ⁄„· Õ»· Ê«Õœ ⁄·Ï «ﬁ·  ");


if (tch->FindAffect(AFFECT_STR))
ch->ChatPacket(CHAT_TYPE_INFO, "  Â–« ·«⁄» Ì” ⁄„·  «Õœ «· ›⁄Ì·«  «· Ì  ﬁÊ„ »“Ì«œ… ‰ﬁ«ÿ ﬁÊ…  „À«· «”„«ﬂ ");
if (tch->FindAffect(AFFECT_DEX))
ch->ChatPacket(CHAT_TYPE_INFO, "  Â–« ·«⁄» Ì” ⁄„·  «Õœ «· ›⁄Ì·«  «· Ì  ﬁÊ„ »“Ì«œ… ‰ﬁ«ÿ „‰«Ê—…  „À«· «”„«ﬂ  ");
if (tch->FindAffect(AFFECT_INT))
ch->ChatPacket(CHAT_TYPE_INFO, "  Â–« ·«⁄» Ì” ⁄„·  «Õœ «· ›⁄Ì·«  «· Ì  ﬁÊ„ »“Ì«œ… ‰ﬁ«ÿ –ﬂ«¡  „À«· «”„«ﬂ  ");
if (tch->FindAffect(AFFECT_CON))
ch->ChatPacket(CHAT_TYPE_INFO, "  Â–« ·«⁄» Ì” ⁄„·  «Õœ «· ›⁄Ì·«  «· Ì  ﬁÊ„ »“Ì«œ… ‰ﬁ«ÿ ÕÌÊÌ…  „À«· «”„«ﬂ  ");
if (tch->FindAffect(AFFECT_CAST_SPEED))
ch->ChatPacket(CHAT_TYPE_INFO, "  Â–« ·«⁄» Ì” ⁄„·  «Õœ «· ›⁄Ì·«  «· Ì  ﬁÊ„ »“Ì«œ… ”—⁄… ”Õ—  ");
if (tch->FindAffect(AFFECT_ATT_SPEED))
ch->ChatPacket(CHAT_TYPE_INFO, "  Â–« ·«⁄» Ì” ⁄„·  «Õœ «· ›⁄Ì·«  «· Ì  ﬁÊ„ »“Ì«œ… ”—⁄… ÂÃÊ„ „À«· «”„«ﬂ «Ê ”—⁄… ÂÃÊ„  ");



//int arryofskills[SKILL_COUNT_INDEX];
//const DWORD * matrixArraySkill = CCombatZoneManager::instance().GetSkillList(ch);
//	arryofskills[0] = ch->GetQuestFlag("combat_zone_skills_cache.dwSkillLevel1");
//	arryofskills[1] = ch->GetQuestFlag("combat_zone_skills_cache.dwSkillLevel2");
//	arryofskills[2] = ch->GetQuestFlag("combat_zone_skills_cache.dwSkillLevel3");
//	arryofskills[3] = ch->GetQuestFlag("combat_zone_skills_cache.dwSkillLevel4");	
//	arryofskills[4] = ch->GetQuestFlag("combat_zone_skills_cache.dwSkillLevel5");
//	arryofskills[5] = ch->GetQuestFlag("combat_zone_skills_cache.dwSkillLevel6");
//for (int i = 0; i < SKILL_COUNT_INDEX; ++i)
//{
//	ch->SetSkillLevel(matrixArraySkill[i], arryofskills[i]);
//	ch->ChatPacket(CHAT_TYPE_INFO, "  test ");
//}

	//ch->ComputePoints();
	//ch->SkillLevelPacket();

		


if (ch->GetGMLevel() <= GM_PLAYER)
{
tch->ChatPacket(CHAT_TYPE_INFO, " √‘⁄«— : ﬁ«„ » ›Õ’ «·Õ«·… Œ«’… »ﬂ «··«⁄» %s " , ch -> GetName() );
}


		tch->SendEquipment(ch);

	}
}

ACMD(do_party_request)
{
	if (ch->GetArena())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("¥Î∑√¿Âø°º≠ ªÁøÎ«œΩ« ºˆ æ¯Ω¿¥œ¥Ÿ."));
		return;
	}

	if (ch->GetParty())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("¿ÃπÃ ∆ƒ∆ºø° º”«ÿ ¿÷¿∏π«∑Œ ∞°¿‘Ω≈√ª¿ª «“ ºˆ æ¯Ω¿¥œ¥Ÿ."));
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	DWORD vid = 0;
	str_to_number(vid, arg1);
	LPCHARACTER tch = CHARACTER_MANAGER::instance().Find(vid);

	if (tch)
		if (!ch->RequestToParty(tch))
			ch->ChatPacket(CHAT_TYPE_COMMAND, "PartyRequestDenied");
}

ACMD(do_party_request_accept)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	DWORD vid = 0;
	str_to_number(vid, arg1);
	LPCHARACTER tch = CHARACTER_MANAGER::instance().Find(vid);

	if (tch)
		ch->AcceptToParty(tch);
}

ACMD(do_party_request_deny)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	DWORD vid = 0;
	str_to_number(vid, arg1);
	LPCHARACTER tch = CHARACTER_MANAGER::instance().Find(vid);

	if (tch)
		ch->DenyToParty(tch);
}

ACMD(do_monarch_warpto)
{
	if (true == LC_IsYMIR() || true == LC_IsKorea())
		return;

	if (!CMonarch::instance().IsMonarch(ch->GetPlayerID(), ch->GetEmpire()))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("±∫¡÷∏∏¿Ã ªÁøÎ ∞°¥…«— ±‚¥…¿‘¥œ¥Ÿ"));
		return;
	}

	//±∫¡÷ ƒ≈∏¿” ∞ÀªÁ
	if (!ch->IsMCOK(CHARACTER::MI_WARP))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%d √ ∞£ ƒ≈∏¿”¿Ã ¿˚øÎ¡ﬂ¿‘¥œ¥Ÿ."), ch->GetMCLTime(CHARACTER::MI_WARP));
		return;
	}

	//±∫¡÷ ∏˜ º“»Ø ∫ÒøÎ 
	const int WarpPrice = 10000;

	//±∫¡÷ ±π∞Ì ∞ÀªÁ 
	if (!CMonarch::instance().IsMoneyOk(WarpPrice, ch->GetEmpire()))
	{
		int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("±π∞Ìø° µ∑¿Ã ∫Œ¡∑«’¥œ¥Ÿ. «ˆ¿Á : %u « ø‰±›æ◊ : %u"), NationMoney, WarpPrice);
		return;	
	}

	int x = 0, y = 0;
	char arg1[256];

	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ªÁøÎπ˝: warpto <character name>"));
		return;
	}

	LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(arg1);

	if (!tch)
	{
		CCI * pkCCI = P2P_MANAGER::instance().Find(arg1);

		if (pkCCI)
		{
			if (pkCCI->bEmpire != ch->GetEmpire())
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("≈∏¡¶±π ¿Ø¿˙ø°∞‘¥¬ ¿Ãµø«“ºˆ æ¯Ω¿¥œ¥Ÿ"));
				return;
			}

			if (pkCCI->bChannel != g_bChannel)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("«ÿ¥Á ¿Ø¿˙¥¬ %d √§≥Œø° ¿÷Ω¿¥œ¥Ÿ. («ˆ¿Á √§≥Œ %d)"), pkCCI->bChannel, g_bChannel);
				return;
			}
			if (!IsMonarchWarpZone(pkCCI->lMapIndex))
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("«ÿ¥Á ¡ˆø™¿∏∑Œ ¿Ãµø«“ ºˆ æ¯Ω¿¥œ¥Ÿ."));
				return;
			}

			PIXEL_POSITION pos;
	
			if (!SECTREE_MANAGER::instance().GetCenterPositionOfMap(pkCCI->lMapIndex, pos))
				ch->ChatPacket(CHAT_TYPE_INFO, "Cannot find map (index %d)", pkCCI->lMapIndex);
			else
			{
				//ch->ChatPacket(CHAT_TYPE_INFO, "You warp to (%d, %d)", pos.x, pos.y);
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s ø°∞‘∑Œ ¿Ãµø«’¥œ¥Ÿ"), arg1);
				ch->WarpSet(pos.x, pos.y);

				//±∫¡÷ µ∑ ªË∞®	
				CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);

				//ƒ≈∏¿” √ ±‚»≠ 
				ch->SetMC(CHARACTER::MI_WARP);
			}
		}
		else if (NULL == CHARACTER_MANAGER::instance().FindPC(arg1))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "There is no one by that name");
		}

		return;
	}
	else
	{
		if (tch->GetEmpire() != ch->GetEmpire())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("≈∏¡¶±π ¿Ø¿˙ø°∞‘¥¬ ¿Ãµø«“ºˆ æ¯Ω¿¥œ¥Ÿ"));
			return;
		}
		if (!IsMonarchWarpZone(tch->GetMapIndex()))
		{
			ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("«ÿ¥Á ¡ˆø™¿∏∑Œ ¿Ãµø«“ ºˆ æ¯Ω¿¥œ¥Ÿ."));
			return;
		}
		x = tch->GetX();
		y = tch->GetY();
	}

	ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s ø°∞‘∑Œ ¿Ãµø«’¥œ¥Ÿ"), arg1);
	ch->WarpSet(x, y);
	ch->Stop();

	//±∫¡÷ µ∑ ªË∞®	
	CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);

	//ƒ≈∏¿” √ ±‚»≠ 
	ch->SetMC(CHARACTER::MI_WARP);
}

ACMD(do_monarch_transfer)
{
	if (true == LC_IsYMIR() || true == LC_IsKorea())
		return;

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ªÁøÎπ˝: transfer <name>"));
		return;
	}

	if (!CMonarch::instance().IsMonarch(ch->GetPlayerID(), ch->GetEmpire()))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("±∫¡÷∏∏¿Ã ªÁøÎ ∞°¥…«— ±‚¥…¿‘¥œ¥Ÿ"));
		return;
	}

	//±∫¡÷ ƒ≈∏¿” ∞ÀªÁ
	if (!ch->IsMCOK(CHARACTER::MI_TRANSFER))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%d √ ∞£ ƒ≈∏¿”¿Ã ¿˚øÎ¡ﬂ¿‘¥œ¥Ÿ."), ch->GetMCLTime(CHARACTER::MI_TRANSFER));	
		return;
	}

	//±∫¡÷ øˆ«¡ ∫ÒøÎ 
	const int WarpPrice = 10000;

	//±∫¡÷ ±π∞Ì ∞ÀªÁ 
	if (!CMonarch::instance().IsMoneyOk(WarpPrice, ch->GetEmpire()))
	{
		int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("±π∞Ìø° µ∑¿Ã ∫Œ¡∑«’¥œ¥Ÿ. «ˆ¿Á : %u « ø‰±›æ◊ : %u"), NationMoney, WarpPrice);
		return;	
	}

	LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(arg1);

	if (!tch)
	{
		CCI * pkCCI = P2P_MANAGER::instance().Find(arg1);

		if (pkCCI)
		{
			if (pkCCI->bEmpire != ch->GetEmpire())
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("¥Ÿ∏• ¡¶±π ¿Ø¿˙¥¬ º“»Ø«“ ºˆ æ¯Ω¿¥œ¥Ÿ."));
				return;
			}
			if (pkCCI->bChannel != g_bChannel)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s ¥‘¿∫ %d √§≥Œø° ¡¢º” ¡ﬂ ¿‘¥œ¥Ÿ. («ˆ¿Á √§≥Œ: %d)"), arg1, pkCCI->bChannel, g_bChannel);
				return;
			}
			if (!IsMonarchWarpZone(pkCCI->lMapIndex))
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("«ÿ¥Á ¡ˆø™¿∏∑Œ ¿Ãµø«“ ºˆ æ¯Ω¿¥œ¥Ÿ."));
				return;
			}
			if (!IsMonarchWarpZone(ch->GetMapIndex()))
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("«ÿ¥Á ¡ˆø™¿∏∑Œ º“»Ø«“ ºˆ æ¯Ω¿¥œ¥Ÿ."));
				return;
			}

			TPacketGGTransfer pgg;

			pgg.bHeader = HEADER_GG_TRANSFER;
			strlcpy(pgg.szName, arg1, sizeof(pgg.szName));
			pgg.lX = ch->GetX();
			pgg.lY = ch->GetY();

			P2P_MANAGER::instance().Send(&pgg, sizeof(TPacketGGTransfer));
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s ¥‘¿ª º“»Ø«œø¥Ω¿¥œ¥Ÿ."), arg1);
			
			//±∫¡÷ µ∑ ªË∞®	
			CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);
			//ƒ≈∏¿” √ ±‚»≠ 
			ch->SetMC(CHARACTER::MI_TRANSFER);
		}
		else
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("¿‘∑¬«œΩ≈ ¿Ã∏ß¿ª ∞°¡¯ ªÁøÎ¿⁄∞° æ¯Ω¿¥œ¥Ÿ."));
		}

		return;
	}

	if (ch == tch)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("¿⁄Ω≈¿ª º“»Ø«“ ºˆ æ¯Ω¿¥œ¥Ÿ."));
		return;
	}

	if (tch->GetEmpire() != ch->GetEmpire())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("¥Ÿ∏• ¡¶±π ¿Ø¿˙¥¬ º“»Ø«“ ºˆ æ¯Ω¿¥œ¥Ÿ."));
		return;
	}
	if (!IsMonarchWarpZone(tch->GetMapIndex()))
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("«ÿ¥Á ¡ˆø™¿∏∑Œ ¿Ãµø«“ ºˆ æ¯Ω¿¥œ¥Ÿ."));
		return;
	}
	if (!IsMonarchWarpZone(ch->GetMapIndex()))
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("«ÿ¥Á ¡ˆø™¿∏∑Œ º“»Ø«“ ºˆ æ¯Ω¿¥œ¥Ÿ."));
		return;
	}

	//tch->Show(ch->GetMapIndex(), ch->GetX(), ch->GetY(), ch->GetZ());
	tch->WarpSet(ch->GetX(), ch->GetY(), ch->GetMapIndex());

	//±∫¡÷ µ∑ ªË∞®	
	CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);
	//ƒ≈∏¿” √ ±‚»≠ 
	ch->SetMC(CHARACTER::MI_TRANSFER);
}

ACMD(do_monarch_info)
{
	if (CMonarch::instance().IsMonarch(ch->GetPlayerID(), ch->GetEmpire()))	
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("≥™¿« ±∫¡÷ ¡§∫∏"));
		TMonarchInfo * p = CMonarch::instance().GetMonarch();
		for (int n = 1; n < 4; ++n)
		{
			if (n == ch->GetEmpire())
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[%s±∫¡÷] : %s  ∫∏¿Ø±›æ◊ %lld "), EMPIRE_NAME(n), p->name[n], p->money[n]);
			else
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[%s±∫¡÷] : %s  "), EMPIRE_NAME(n), p->name[n]);

		}
	}
	else
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("±∫¡÷ ¡§∫∏"));
		TMonarchInfo * p = CMonarch::instance().GetMonarch();
		for (int n = 1; n < 4; ++n)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[%s±∫¡÷] : %s  "), EMPIRE_NAME(n), p->name[n]);

		}
	}
	
}	

ACMD(do_elect)
{
	db_clientdesc->DBPacketHeader(HEADER_GD_COME_TO_VOTE, ch->GetDesc()->GetHandle(), 0);
}

// LUA_ADD_GOTO_INFO
struct GotoInfo
{
	std::string 	st_name;

	BYTE 	empire;
	int 	mapIndex;
	DWORD 	x, y;

	GotoInfo()
	{
		st_name 	= "";
		empire 		= 0;
		mapIndex 	= 0;

		x = 0;
		y = 0;
	}

	GotoInfo(const GotoInfo& c_src)
	{
		__copy__(c_src);
	}

	void operator = (const GotoInfo& c_src)
	{
		__copy__(c_src);
	}

	void __copy__(const GotoInfo& c_src)
	{
		st_name 	= c_src.st_name;
		empire 		= c_src.empire;
		mapIndex 	= c_src.mapIndex;

		x = c_src.x;
		y = c_src.y;
	}
};

extern void BroadcastNotice(const char * c_pszBuf);

ACMD(do_monarch_tax)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Usage: monarch_tax <1-50>");
		return;
	}

	// ±∫¡÷ ∞ÀªÁ	
	if (!ch->IsMonarch())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("±∫¡÷∏∏¿Ã ªÁøÎ«“ºˆ ¿÷¥¬ ±‚¥…¿‘¥œ¥Ÿ"));
		return;
	}

	// ºº±›º≥¡§ 
	int tax = 0;
	str_to_number(tax,  arg1);

	if (tax < 1 || tax > 50)
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("1-50 ªÁ¿Ã¿« ºˆƒ°∏¶ º±≈√«ÿ¡÷ººø‰"));

	quest::CQuestManager::instance().SetEventFlag("trade_tax", tax); 

	// ±∫¡÷ø°∞‘ ∏ﬁºº¡ˆ «œ≥™
	ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("ºº±›¿Ã %d %∑Œ º≥¡§µ«æ˙Ω¿¥œ¥Ÿ"));

	// ∞¯¡ˆ 
	char szMsg[1024];	

	snprintf(szMsg, sizeof(szMsg), "±∫¡÷¿« ∏Ì¿∏∑Œ ºº±›¿Ã %d %% ∑Œ ∫Ø∞Êµ«æ˙Ω¿¥œ¥Ÿ", tax);
	BroadcastNotice(szMsg);

	snprintf(szMsg, sizeof(szMsg), "æ’¿∏∑Œ¥¬ ∞≈∑° ±›æ◊¿« %d %% ∞° ±π∞Ì∑Œ µÈæÓ∞°∞‘µÀ¥œ¥Ÿ.", tax);
	BroadcastNotice(szMsg);

	// ƒ≈∏¿” √ ±‚»≠ 
	ch->SetMC(CHARACTER::MI_TAX); 
}

static const DWORD cs_dwMonarchMobVnums[] =
{
	191, //	ªÍ∞ﬂΩ≈
	192, //	¿˙Ω≈
	193, //	øıΩ≈
	194, //	»£Ω≈
	391, //	πÃ¡§
	392, //	¿∫¡§
	393, //	ºº∂˚
	394, //	¡¯»Ò
	491, //	∏Õ»Ø
	492, //	∫∏øÏ
	493, //	±∏∆–
	494, //	√ﬂ»Á
	591, //	∫Ò∑˘¥‹¥Î¿Â
	691, //	øı±Õ ¡∑¿Â
	791, //	π–±≥±≥¡÷
	1304, // ¥©∑∑π¸±Õ
	1901, // ±∏πÃ»£
	2091, // ø©ø’∞≈πÃ
	2191, // ∞≈¥ÎªÁ∏∑∞≈∫œ
	2206, // »≠ø∞ø’i
	0,
};

ACMD(do_monarch_mob)
{
	char arg1[256];
	LPCHARACTER	tch;

	one_argument(argument, arg1, sizeof(arg1));

	if (!ch->IsMonarch())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("±∫¡÷∏∏¿Ã ªÁøÎ«“ºˆ ¿÷¥¬ ±‚¥…¿‘¥œ¥Ÿ"));
		return;
	}
	
	if (!*arg1)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Usage: mmob <mob name>");
		return;
	}

	BYTE pcEmpire = ch->GetEmpire();
	BYTE mapEmpire = SECTREE_MANAGER::instance().GetEmpireFromMapIndex(ch->GetMapIndex());

	if (LC_IsYMIR() == true || LC_IsKorea() == true)
	{
		if (mapEmpire != pcEmpire && mapEmpire != 0)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("¿⁄±π øµ≈‰ø°º≠∏∏ ªÁøÎ«“ ºˆ ¿÷¥¬ ±‚¥…¿‘¥œ¥Ÿ"));
			return;
		}
	}

	// ±∫¡÷ ∏˜ º“»Ø ∫ÒøÎ 
	const int SummonPrice = 5000000;

	// ±∫¡÷ ƒ≈∏¿” ∞ÀªÁ
	if (!ch->IsMCOK(CHARACTER::MI_SUMMON))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%d √ ∞£ ƒ≈∏¿”¿Ã ¿˚øÎ¡ﬂ¿‘¥œ¥Ÿ."), ch->GetMCLTime(CHARACTER::MI_SUMMON));	
		return;
	}

	// ±∫¡÷ ±π∞Ì ∞ÀªÁ 
	if (!CMonarch::instance().IsMoneyOk(SummonPrice, ch->GetEmpire()))
	{
		int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("±π∞Ìø° µ∑¿Ã ∫Œ¡∑«’¥œ¥Ÿ. «ˆ¿Á : %u « ø‰±›æ◊ : %u"), NationMoney, SummonPrice);
		return;	
	}

	const CMob * pkMob;
	DWORD vnum = 0;

	if (isdigit(*arg1))
	{
		str_to_number(vnum, arg1);

		if ((pkMob = CMobManager::instance().Get(vnum)) == NULL)
			vnum = 0;
	}
	else
	{
		pkMob = CMobManager::Instance().Get(arg1, true);

		if (pkMob)
			vnum = pkMob->m_table.dwVnum;
	}

	DWORD count;

	// º“»Ø ∞°¥… ∏˜ ∞ÀªÁ
	for (count = 0; cs_dwMonarchMobVnums[count] != 0; ++count)
		if (cs_dwMonarchMobVnums[count] == vnum)
			break;

	if (0 == cs_dwMonarchMobVnums[count])
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("º“»Ø«“ºˆ æ¯¥¬ ∏ÛΩ∫≈Õ ¿‘¥œ¥Ÿ. º“»Ø∞°¥…«— ∏ÛΩ∫≈Õ¥¬ »®∆‰¿Ã¡ˆ∏¶ ¬¸¡∂«œººø‰"));
		return;
	}

	tch = CHARACTER_MANAGER::instance().SpawnMobRange(vnum, 
			ch->GetMapIndex(),
			ch->GetX() - number(200, 750), 
			ch->GetY() - number(200, 750), 
			ch->GetX() + number(200, 750), 
			ch->GetY() + number(200, 750), 
			true,
			pkMob->m_table.bType == CHAR_TYPE_STONE,
			true);

	if (tch)
	{
		// ±∫¡÷ µ∑ ªË∞®	
		CMonarch::instance().SendtoDBDecMoney(SummonPrice, ch->GetEmpire(), ch);

		// ƒ≈∏¿” √ ±‚»≠ 
		ch->SetMC(CHARACTER::MI_SUMMON); 
	}
}

static const char* FN_point_string(int apply_number)
{
	switch (apply_number)
	{
		case POINT_MAX_HP:
			return LC_TEXT("√÷¥Î ª˝∏Ì∑¬ +%d");

		case POINT_MAX_SP:
			return LC_TEXT("√÷¥Î ¡§Ω≈∑¬ +%d");

		case POINT_HT:
			return LC_TEXT("√º∑¬ +%d");

		case POINT_IQ:
			return LC_TEXT("¡ˆ¥… +%d");

		case POINT_ST:
			return LC_TEXT("±Ÿ∑¬ +%d");

		case POINT_DX:
			return LC_TEXT("πŒ√∏ +%d");

		case POINT_ATT_SPEED:
			return LC_TEXT("∞¯∞›º”µµ +%d");

		case POINT_MOV_SPEED:
			return LC_TEXT("¿Ãµøº”µµ %d");

		case POINT_CASTING_SPEED:
			return LC_TEXT("ƒ≈∏¿” -%d");

		case POINT_HP_REGEN:
			return LC_TEXT("ª˝∏Ì∑¬ »∏∫π +%d");

		case POINT_SP_REGEN:
			return LC_TEXT("¡§Ω≈∑¬ »∏∫π +%d");

		case POINT_POISON_PCT:
			return LC_TEXT("µ∂∞¯∞› %d");

		case POINT_BLEEDING_PCT:
			return LC_TEXT("Bleed pct %d%%");

		case POINT_STUN_PCT:
			return LC_TEXT("Ω∫≈œ +%d");

		case POINT_SLOW_PCT:
			return LC_TEXT("ΩΩ∑ŒøÏ +%d");

		case POINT_CRITICAL_PCT:
			return LC_TEXT("%d%% »Æ∑¸∑Œ ƒ°∏Ì≈∏ ∞¯∞›");

		case POINT_RESIST_CRITICAL:
			return LC_TEXT("ªÛ¥Î¿« ƒ°∏Ì≈∏ »Æ∑¸ %d%% ∞®º“");

		case POINT_PENETRATE_PCT:
			return LC_TEXT("%d%% »Æ∑¸∑Œ ∞¸≈Î ∞¯∞›");

		case POINT_RESIST_PENETRATE:
			return LC_TEXT("ªÛ¥Î¿« ∞¸≈Î ∞¯∞› »Æ∑¸ %d%% ∞®º“");

		case POINT_ATTBONUS_HUMAN:
			return LC_TEXT("¿Œ∞£∑˘ ∏ÛΩ∫≈Õ ≈∏∞›ƒ° +%d%%");

		case POINT_ATTBONUS_ANIMAL:
			return LC_TEXT("µøπ∞∑˘ ∏ÛΩ∫≈Õ ≈∏∞›ƒ° +%d%%");

		case POINT_ATTBONUS_ORC:
			return LC_TEXT("øı±Õ¡∑ ≈∏∞›ƒ° +%d%%");

		case POINT_ATTBONUS_MILGYO:
			return LC_TEXT("π–±≥∑˘ ≈∏∞›ƒ° +%d%%");

		case POINT_ATTBONUS_UNDEAD:
			return LC_TEXT("Ω√√º∑˘ ≈∏∞›ƒ° +%d%%");

		case POINT_ATTBONUS_DEVIL:
			return LC_TEXT("æ«∏∂∑˘ ≈∏∞›ƒ° +%d%%");

		case POINT_STEAL_HP:
			return LC_TEXT("≈∏∞›ƒ° %d%% ∏¶ ª˝∏Ì∑¬¿∏∑Œ »Ìºˆ");

		case POINT_STEAL_SP:
			return LC_TEXT("≈∏∑¬ƒ° %d%% ∏¶ ¡§Ω≈∑¬¿∏∑Œ »Ìºˆ");

		case POINT_MANA_BURN_PCT:
			return LC_TEXT("%d%% »Æ∑¸∑Œ ≈∏∞›Ω√ ªÛ¥Î ¿¸Ω≈∑¬ º“∏");

		case POINT_DAMAGE_SP_RECOVER:
			return LC_TEXT("%d%% »Æ∑¸∑Œ «««ÿΩ√ ¡§Ω≈∑¬ »∏∫π");

		case POINT_BLOCK:
			return LC_TEXT("π∞∏Æ≈∏∞›Ω√ ∫Ì∑∞ »Æ∑¸ %d%%");

		case POINT_DODGE:
			return LC_TEXT("»∞ ∞¯∞› »∏«« »Æ∑¸ %d%%");

		case POINT_RESIST_SWORD:
			return LC_TEXT("«—º’∞À πÊæÓ %d%%");

		case POINT_RESIST_TWOHAND:
			return LC_TEXT("æÁº’∞À πÊæÓ %d%%");

		case POINT_RESIST_DAGGER:
			return LC_TEXT("µŒº’∞À πÊæÓ %d%%");

		case POINT_RESIST_BELL:
			return LC_TEXT("πÊøÔ πÊæÓ %d%%");

		case POINT_RESIST_FAN:
			return LC_TEXT("∫Œ√§ πÊæÓ %d%%");

		case POINT_RESIST_BOW:
			return LC_TEXT("»∞∞¯∞› ¿˙«◊ %d%%");

		case POINT_RESIST_CLAW:
			return LC_TEXT("Defense chance against claws +%d%%");

		case POINT_RESIST_FIRE:
			return LC_TEXT("»≠ø∞ ¿˙«◊ %d%%");

		case POINT_RESIST_ELEC:
			return LC_TEXT("¿¸±‚ ¿˙«◊ %d%%");

		case POINT_RESIST_MAGIC:
			return LC_TEXT("∏∂π˝ ¿˙«◊ %d%%");

		case POINT_RESIST_WIND:
			return LC_TEXT("πŸ∂˜ ¿˙«◊ %d%%");

		case POINT_RESIST_ICE:
			return LC_TEXT("≥√±‚ ¿˙«◊ %d%%");

		case POINT_RESIST_EARTH:
			return LC_TEXT("¥Î¡ˆ ¿˙«◊ %d%%");

		case POINT_RESIST_DARK:
			return LC_TEXT("æÓµ“ ¿˙«◊ %d%%");

		case POINT_REFLECT_MELEE:
			return LC_TEXT("¡˜¡¢ ≈∏∞›ƒ° π›ªÁ »Æ∑¸ : %d%%");

		case POINT_REFLECT_CURSE:
			return LC_TEXT("¿˙¡÷ µ«µπ∏Æ±‚ »Æ∑¸ %d%%");

		case POINT_POISON_REDUCE:
			return LC_TEXT("µ∂ ¿˙«◊ %d%%");

		case POINT_BLEEDING_REDUCE:
			return LC_TEXT("Bleed reduce %d%%");

		case POINT_KILL_SP_RECOVER:
			return LC_TEXT("%d%% »Æ∑¸∑Œ ¿˚≈ƒ°Ω√ ¡§Ω≈∑¬ »∏∫π");

		case POINT_EXP_DOUBLE_BONUS:
			return LC_TEXT("%d%% »Æ∑¸∑Œ ¿˚≈ƒ°Ω√ ∞Ê«Ëƒ° √ﬂ∞° ªÛΩ¬");

		case POINT_GOLD_DOUBLE_BONUS:
			return LC_TEXT("%d%% »Æ∑¸∑Œ ¿˚≈ƒ°Ω√ µ∑ 2πË µÂ∑”");

		case POINT_ITEM_DROP_BONUS:
			return LC_TEXT("%d%% »Æ∑¸∑Œ ¿˚≈ƒ°Ω√ æ∆¿Ã≈€ 2πË µÂ∑”");

		case POINT_POTION_BONUS:
			return LC_TEXT("π∞æ‡ ªÁøÎΩ√ %d%% º∫¥… ¡ı∞°");

		case POINT_KILL_HP_RECOVERY:
			return LC_TEXT("%d%% »Æ∑¸∑Œ ¿˚≈ƒ°Ω√ ª˝∏Ì∑¬ »∏∫π");

		case POINT_ATT_GRADE_BONUS:
			return LC_TEXT("∞¯∞›∑¬ +%d");

		case POINT_DEF_GRADE_BONUS:
			return LC_TEXT("πÊæÓ∑¬ +%d");

		case POINT_MAGIC_ATT_GRADE:
			return LC_TEXT("∏∂π˝ ∞¯∞›∑¬ +%d");

		case POINT_MAGIC_DEF_GRADE:
			return LC_TEXT("∏∂π˝ πÊæÓ∑¬ +%d");

		case POINT_MAX_STAMINA:
			return LC_TEXT("√÷¥Î ¡ˆ±∏∑¬ +%d");

		case POINT_ATTBONUS_WARRIOR:
			return LC_TEXT("π´ªÁø°∞‘ ∞≠«‘ +%d%%");

		case POINT_ATTBONUS_ASSASSIN:
			return LC_TEXT("¿⁄∞¥ø°∞‘ ∞≠«‘ +%d%%");

		case POINT_ATTBONUS_SURA:
			return LC_TEXT("ºˆ∂Ûø°∞‘ ∞≠«‘ +%d%%");

		case POINT_ATTBONUS_SHAMAN:
			return LC_TEXT("π´¥Áø°∞‘ ∞≠«‘ +%d%%");

		case POINT_ATTBONUS_WOLFMAN:
			return LC_TEXT("Strong against Lycans +%d%%");

		case POINT_ATTBONUS_MONSTER:
			return LC_TEXT("∏ÛΩ∫≈Õø°∞‘ ∞≠«‘ +%d%%");

		case POINT_MALL_ATTBONUS:
			return LC_TEXT("∞¯∞›∑¬ +%d%%");

		case POINT_MALL_DEFBONUS:
			return LC_TEXT("πÊæÓ∑¬ +%d%%");

		case POINT_MALL_EXPBONUS:
			return LC_TEXT("∞Ê«Ëƒ° %d%%");

		case POINT_MALL_ITEMBONUS:
			return LC_TEXT("æ∆¿Ã≈€ µÂ∑”¿≤ %.1fπË");

		case POINT_MALL_GOLDBONUS:
			return LC_TEXT("µ∑ µÂ∑”¿≤ %.1fπË");

		case POINT_MAX_HP_PCT:
			return LC_TEXT("√÷¥Î ª˝∏Ì∑¬ +%d%%");

		case POINT_MAX_SP_PCT:
			return LC_TEXT("√÷¥Î ¡§Ω≈∑¬ +%d%%");

		case POINT_SKILL_DAMAGE_BONUS:
			return LC_TEXT("Ω∫≈≥ µ•πÃ¡ˆ %d%%");

		case POINT_NORMAL_HIT_DAMAGE_BONUS:
			return LC_TEXT("∆Ú≈∏ µ•πÃ¡ˆ %d%%");

		case POINT_SKILL_DEFEND_BONUS:
			return LC_TEXT("Ω∫≈≥ µ•πÃ¡ˆ ¿˙«◊ %d%%");

		case POINT_NORMAL_HIT_DEFEND_BONUS:
			return LC_TEXT("∆Ú≈∏ µ•πÃ¡ˆ ¿˙«◊ %d%%");

		case POINT_RESIST_WARRIOR:
			return LC_TEXT("π´ªÁ∞¯∞›ø° %d%% ¿˙«◊");

		case POINT_RESIST_ASSASSIN:
			return LC_TEXT("¿⁄∞¥∞¯∞›ø° %d%% ¿˙«◊");

		case POINT_RESIST_SURA:
			return LC_TEXT("ºˆ∂Û∞¯∞›ø° %d%% ¿˙«◊");

		case POINT_RESIST_SHAMAN:
			return LC_TEXT("π´¥Á∞¯∞›ø° %d%% ¿˙«◊");

		case POINT_RESIST_WOLFMAN:
			return LC_TEXT("Defense chance against Lycan attacks %d%%");
		
		#ifdef __ANTI_RESIST_MAGIC_BONUS__
		case POINT_ANTI_RESIST_MAGIC:
			return LC_TEXT("Anti Magic Resistance: %d%%");
		#endif
		
		default:
			return NULL;
	}
}

static bool FN_hair_affect_string(LPCHARACTER ch, char *buf, size_t bufsiz)
{
	if (NULL == ch || NULL == buf)
		return false;

	CAffect* aff = NULL;
	time_t expire = 0;
	struct tm ltm;
	int	year, mon, day;
	int	offset = 0;

	aff = ch->FindAffect(AFFECT_HAIR);

	if (NULL == aff)
		return false;

	expire = ch->GetQuestFlag("hair.limit_time");

	if (expire < get_global_time())
		return false;

	// set apply string
	offset = snprintf(buf, bufsiz, FN_point_string(aff->bApplyOn), aff->lApplyValue);

	if (offset < 0 || offset >= (int) bufsiz)
		offset = bufsiz - 1;

	localtime_r(&expire, &ltm);

	year	= ltm.tm_year + 1900;
	mon		= ltm.tm_mon + 1;
	day		= ltm.tm_mday;

	snprintf(buf + offset, bufsiz - offset, LC_TEXT(" (∏∏∑·¿œ : %d≥‚ %dø˘ %d¿œ)"), year, mon, day);

	return true;
}

ACMD(do_costume)
{
	char buf[512];
	const size_t bufferSize = sizeof(buf);

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	CItem* pBody = ch->GetWear(WEAR_COSTUME_BODY);
	CItem* pHair = ch->GetWear(WEAR_COSTUME_HAIR);
	CItem* pAcce = ch->GetWear(WEAR_COSTUME_ACCE);

	ch->ChatPacket(CHAT_TYPE_INFO, "COSTUME status:");
	if (pBody)
	{
		const char* itemName = pBody->GetName();
		ch->ChatPacket(CHAT_TYPE_INFO, "  BODY : %s", itemName);

		if (pBody->IsEquipped() && arg1[0] == 'b')
			ch->UnequipItem(pBody);
	}

	if (pHair)
	{
		const char* itemName = pHair->GetName();
		ch->ChatPacket(CHAT_TYPE_INFO, "  HAIR : %s", itemName);

		for (int i = 0; i < pHair->GetAttributeCount(); ++i)
		{
			const TPlayerItemAttribute& attr = pHair->GetAttribute(i);
			if (0 < attr.bType)
			{
				snprintf(buf, bufferSize, FN_point_string(attr.bType), attr.sValue);
				ch->ChatPacket(CHAT_TYPE_INFO, "     %s", buf);
			}
		}

		if (pHair->IsEquipped() && arg1[0] == 'h')
			ch->UnequipItem(pHair);
	}

	if (pAcce)
	{
		const char* itemName = pAcce->GetName();
		ch->ChatPacket(CHAT_TYPE_INFO, "  Acce : %s", itemName);

		for (int i = 0; i < pAcce->GetAttributeCount(); ++i)
		{
			const TPlayerItemAttribute& attr = pAcce->GetAttribute(i);
			if (0 < attr.bType)
			{
				snprintf(buf, bufferSize, FN_point_string(attr.bType), attr.sValue);
				ch->ChatPacket(CHAT_TYPE_INFO, "     %s", buf);
			}
		}

		if (pAcce->IsEquipped() && arg1[0] == 'a')
			ch->UnequipItem(pAcce);
	}
}

ACMD(do_hair)
{
	char buf[256];

	if (false == FN_hair_affect_string(ch, buf, sizeof(buf)))
		return;

	ch->ChatPacket(CHAT_TYPE_INFO, buf);
}

ACMD(do_inventory)
{
	int	index = 0;
	int	count		= 1;

	char arg1[256];
	char arg2[256];

	LPITEM	item;

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Usage: inventory <start_index> <count>");
		return;
	}

	if (!*arg2)
	{
		index = 0;
		str_to_number(count, arg1);
	}
	else
	{
		str_to_number(index, arg1); index = MIN(index, INVENTORY_MAX_NUM);
		str_to_number(count, arg2); count = MIN(count, INVENTORY_MAX_NUM);
	}

	for (int i = 0; i < count; ++i)
	{
		if (index >= INVENTORY_MAX_NUM)
			break;

		item = ch->GetInventoryItem(index);

		ch->ChatPacket(CHAT_TYPE_INFO, "inventory [%d] = %s",
						index, item ? item->GetName() : "<NONE>");
		++index;
	}
}

ACMD(do_acce)
{
	if (!ch->CanDoAcce())
		return;

	dev_log(LOG_DEB0, "Acce command <%s>: %s", ch->GetName(), argument);
	int acce_index = 0, inven_index = 0;
	const char *line;

	char arg1[256], arg2[256], arg3[256];
	line = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	one_argument(line, arg3, sizeof(arg3));
	if (0 == arg1[0])
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Usage: acce open");
		ch->ChatPacket(CHAT_TYPE_INFO, "       acce close");
		ch->ChatPacket(CHAT_TYPE_INFO, "       acce add <inveltory_index>");
		ch->ChatPacket(CHAT_TYPE_INFO, "       acce delete <acce_index>");
		ch->ChatPacket(CHAT_TYPE_INFO, "       acce list");
		ch->ChatPacket(CHAT_TYPE_INFO, "       acce cancel");
		ch->ChatPacket(CHAT_TYPE_INFO, "       acce make [all]");
		return;
	}

	const std::string& strArg1 = std::string(arg1);
	if (strArg1 == "r_info")
	{
		if (0 == arg2[0])
			Acce_request_result_list(ch);
		else
		{
			if (isdigit(*arg2))
			{
				int listIndex = 0, requestCount = 1;
				str_to_number(listIndex, arg2);
				if (0 != arg3[0] && isdigit(*arg3))
					str_to_number(requestCount, arg3);

				Acce_request_material_info(ch, listIndex, requestCount);
			}
		}

		return;
	}
	else if (strArg1 == "absorption")
	{
		Acce_absorption_make(ch);
		return;
	}
	else if (strArg1 == "open_absorption")
	{
		Acce_absorption_open(ch);
		return;
	}

	switch (LOWER(arg1[0]))
	{
		case 'o':
			Acce_open(ch);
			break;
		case 'c':
			Acce_close(ch);
			break;
		case 'l':
			Acce_show_list(ch);
			break;
		case 'a':
			{
				if (0 == arg2[0] || !isdigit(*arg2) || 0 == arg3[0] || !isdigit(*arg3))
					return;

				str_to_number(acce_index, arg2);
				str_to_number(inven_index, arg3);
				Acce_add_item (ch, acce_index, inven_index);
			}
			break;
		case 'd':
			{
				if (0 == arg2[0] || !isdigit(*arg2))
					return;

				str_to_number(acce_index, arg2);
				Acce_delete_item (ch, acce_index);
			}
			break;
		case 'm':
			if (0 != arg2[0])
			{
				while (true == Acce_make(ch))
					dev_log(LOG_DEB0, "Acce make success!");
			}
			else
				Acce_make(ch);
			break;
		default:
			return;
	}
}

//gift notify quest command
ACMD(do_gift)
{
	ch->ChatPacket(CHAT_TYPE_COMMAND, "gift");
}

#ifdef NEW_PET_SYSTEM
ACMD(do_CubePetAdd) {

	int pos = 0;
	int invpos = 0;

	const char *line;
	char arg1[256], arg2[256], arg3[256];

	line = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	one_argument(line, arg3, sizeof(arg3));

	if (0 == arg1[0])
		return;
	const std::string& strArg1 = std::string(arg1);
	switch (LOWER(arg1[0]))
	{
	case 'a':	// add cue_index inven_index
	{
		if (0 == arg2[0] || !isdigit(*arg2) ||
			0 == arg3[0] || !isdigit(*arg3))
			return;

		str_to_number(pos, arg2);
		str_to_number(invpos, arg3);

	}
	break;

	default:
		return;
	}

	if (ch->GetNewPetSystem()->IsActivePet())
		ch->GetNewPetSystem()->SetItemCube(pos, invpos);
	else
		return;

}

ACMD(do_PetSkill) {
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1)
		return;

	DWORD skillslot = 0;
	str_to_number(skillslot, arg1);
	if (skillslot > 2 || skillslot < 0)
		return;

	if (ch->GetNewPetSystem()->IsActivePet())
		ch->GetNewPetSystem()->DoPetSkill(skillslot);
	else
		ch->ChatPacket(CHAT_TYPE_INFO, "[Pet] Devam etmek icin Pet'i cagirmis olmaniz gerekiriyor.");
}

ACMD(do_FeedCubePet) {
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1)
		return;

	DWORD feedtype = 0;
	str_to_number(feedtype, arg1);
	if (ch->GetNewPetSystem()->IsActivePet())
		ch->GetNewPetSystem()->ItemCubeFeed(feedtype);
	else
		ch->ChatPacket(CHAT_TYPE_INFO, "[Pet] Lutfen once Peti cagirin!");
}

ACMD(do_PetEvo) {

	if (ch->GetExchange() || ch->GetMyShop() || ch->GetShopOwner() || ch->IsOpenSafebox() || ch->IsCubeOpen()) {
		ch->ChatPacket(CHAT_TYPE_INFO, "[Pet] Petiniz daha fazla gelistirilemez.");
		return;
	}
	if (ch->GetNewPetSystem()->IsActivePet()) {

		int it[3][1] = { 
						{ 55003 }, //Here Modify Items to request for 1 evo
						{ 55004 }, //Here Modify Items to request for 2 evo
						{ 55005 }  //Here Modify Items to request for 3 evo
		};
		int ic[3][1] = {{ 10 },
						{ 20 },
						{ 30 }
		};
		int tmpevo = ch->GetNewPetSystem()->GetEvolution();

		if (ch->GetNewPetSystem()->GetLevel() == 40 && tmpevo == 0 ||
			ch->GetNewPetSystem()->GetLevel() == 60 && tmpevo == 1 ||
			ch->GetNewPetSystem()->GetLevel() == 80 && tmpevo == 2) {


			for (int b = 0; b < 1; b++) {
				if (ch->CountSpecifyItem(it[tmpevo][b]) < ic[tmpevo][b]) {
					ch->ChatPacket(CHAT_TYPE_INFO, "[Pet] Gelistirme icin gereken itemler:");
					for (int c = 0; c < 1; c++) {
						DWORD vnum = it[tmpevo][c];
						ch->ChatPacket(CHAT_TYPE_INFO, "%s X%d", ITEM_MANAGER::instance().GetTable(vnum)->szLocaleName , ic[tmpevo][c]);
					}
					return;
				}
			}
			for (int c = 0; c < 1; c++) {
				ch->RemoveSpecifyItem(it[tmpevo][c], ic[tmpevo][c]);
			}
			ch->GetNewPetSystem()->IncreasePetEvolution();

		}
		else {
			ch->ChatPacket(CHAT_TYPE_INFO, "[Pet] Peti suanda gelistiremezsin.");
			return;
		}

	}else
		ch->ChatPacket(CHAT_TYPE_INFO, "[Pet] Once pet'i cagir!");

}

#endif

ACMD(do_cube)
{
	if (!ch->CanDoCube())
		return;

	dev_log(LOG_DEB0, "CUBE COMMAND <%s>: %s", ch->GetName(), argument);
	int cube_index = 0, inven_index = 0;
	const char *line;

	char arg1[256], arg2[256], arg3[256];

	line = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	one_argument(line, arg3, sizeof(arg3));

	if (0 == arg1[0])
	{
		// print usage
		ch->ChatPacket(CHAT_TYPE_INFO, "Usage: cube open");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube close");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube add <inveltory_index>");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube delete <cube_index>");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube list");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube cancel");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube make [all]");
		return;
	}

	const std::string& strArg1 = std::string(arg1);

	// r_info (request information)
	// /cube r_info     ==> (Client -> Server) «ˆ¿Á NPC∞° ∏∏µÈ ºˆ ¿÷¥¬ ∑πΩ√«« ø‰√ª
	//					    (Server -> Client) /cube r_list npcVNUM resultCOUNT 123,1/125,1/128,1/130,5
	//
	// /cube r_info 3   ==> (Client -> Server) «ˆ¿Á NPC∞° ∏∏µÈºˆ ¿÷¥¬ ∑πΩ√«« ¡ﬂ 3π¯¬∞ æ∆¿Ã≈€¿ª ∏∏µÂ¥¬ µ• « ø‰«— ¡§∫∏∏¶ ø‰√ª
	// /cube r_info 3 5 ==> (Client -> Server) «ˆ¿Á NPC∞° ∏∏µÈºˆ ¿÷¥¬ ∑πΩ√«« ¡ﬂ 3π¯¬∞ æ∆¿Ã≈€∫Œ≈Õ ¿Ã»ƒ 5∞≥¿« æ∆¿Ã≈€¿ª ∏∏µÂ¥¬ µ• « ø‰«— ¿Á∑· ¡§∫∏∏¶ ø‰√ª
	//					   (Server -> Client) /cube m_info startIndex count 125,1|126,2|127,2|123,5&555,5&555,4/120000@125,1|126,2|127,2|123,5&555,5&555,4/120000
	//
	if (strArg1 == "r_info")
	{
		if (0 == arg2[0])
			//Cube_request_result_list(ch);
			return;
		else
		{
			if (isdigit(*arg2))
			{
				int listIndex = 0, requestCount = 1;
				str_to_number(listIndex, arg2);

				if (0 != arg3[0] && isdigit(*arg3))
					str_to_number(requestCount, arg3);

				Cube_request_material_info(ch, listIndex, requestCount);
			}
		}

		return;
	}

	switch (LOWER(arg1[0]))
	{
		case 'o':	// open
			Cube_open(ch);
			break;

		case 'c':	// close
			Cube_close(ch);
			break;

		case 'l':	// list
			Cube_show_list(ch);
			break;

		case 'a':	// add cue_index inven_index
			{
				if (0 == arg2[0] || !isdigit(*arg2) ||
					0 == arg3[0] || !isdigit(*arg3))
					return;

				str_to_number(cube_index, arg2);
				str_to_number(inven_index, arg3);
				Cube_add_item (ch, cube_index, inven_index);
			}
			break;

		case 'd':	// delete
			{
				if (0 == arg2[0] || !isdigit(*arg2))
					return;

				str_to_number(cube_index, arg2);
				Cube_delete_item (ch, cube_index);
			}
			break;

		case 'm':	// make
			if (0 != arg2[0])
			{
				while (true == Cube_make(ch))
					dev_log (LOG_DEB0, "cube make success");
			}
			else
				Cube_make(ch);
			break;

		default:
			return;
	}
}

ACMD(do_cards)
{
	const char *line;
	char arg1[256], arg2[256];

	line = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	switch (LOWER(arg1[0]))
	{
		case 'o':    // open
			if (isdigit(*arg2))
			{
				DWORD safemode = NULL;
				str_to_number(safemode, arg2);
				ch->Cards_open(safemode);
			}
			break;
		case 'p':    // open
			ch->Cards_pullout();
			break;
		case 'e':    // open
			ch->CardsEnd();
			break;
		case 'd':    // open
			if (isdigit(*arg2))
			{
				DWORD destroy_index = NULL;
				str_to_number(destroy_index, arg2);
				ch->CardsDestroy(destroy_index);
			}
			break;
		case 'a':    // open
			if (isdigit(*arg2))
			{
				DWORD accpet_index = NULL;
				str_to_number(accpet_index, arg2);
				ch->CardsAccept(accpet_index);
			}
			break;
		case 'r':    // open
			if (isdigit(*arg2))
			{
				DWORD restore_index = NULL;
				str_to_number(restore_index, arg2);
				ch->CardsRestore(restore_index);
			}
			break;
		default:
			return;
	}
}

ACMD(do_in_game_mall)
{
	if (LC_IsYMIR() == true || LC_IsKorea() == true)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "mall http://metin2.co.kr/04_mall/mall/login.htm");
		return;
	}

	if (true == LC_IsTaiwan())
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "mall http://203.69.141.203/mall/mall/item_main.htm");
		return;
	}

	// §–_§– ƒËµµº≠πˆ æ∆¿Ã≈€∏Ù URL «œµÂƒ⁄µ˘ √ﬂ∞°
	if (true == LC_IsWE_Korea())
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "mall http://metin2.co.kr/50_we_mall/mall/login.htm");
		return;
	}

	if (LC_IsJapan() == true)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "mall http://mt2.oge.jp/itemmall/itemList.php");
		return;
	}
	
	if (LC_IsNewCIBN() == true && test_server)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "mall http://218.99.6.51/04_mall/mall/login.htm");
		return;
	}
	if (LC_IsSingapore() == true)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "mall http://www.metin2.sg/ishop.php");
		return;
	}
	/*
	if (LC_IsCanada() == true)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "mall http://mall.z8games.com/mall_entry.aspx?tb=m2");
		return;
	}*/

	if (LC_IsEurope() == true)
	{
		char country_code[3];

		switch (LC_GetLocalType())
		{
			case LC_GERMANY:	country_code[0] = 'd'; country_code[1] = 'e'; country_code[2] = '\0'; break;
			case LC_FRANCE:		country_code[0] = 'f'; country_code[1] = 'r'; country_code[2] = '\0'; break;
			case LC_ITALY:		country_code[0] = 'i'; country_code[1] = 't'; country_code[2] = '\0'; break;
			case LC_SPAIN:		country_code[0] = 'e'; country_code[1] = 's'; country_code[2] = '\0'; break;
			case LC_UK:			country_code[0] = 'e'; country_code[1] = 'n'; country_code[2] = '\0'; break;
			case LC_TURKEY:		country_code[0] = 't'; country_code[1] = 'r'; country_code[2] = '\0'; break;
			case LC_POLAND:		country_code[0] = 'p'; country_code[1] = 'l'; country_code[2] = '\0'; break;
			case LC_PORTUGAL:	country_code[0] = 'p'; country_code[1] = 't'; country_code[2] = '\0'; break;
			case LC_GREEK:		country_code[0] = 'g'; country_code[1] = 'r'; country_code[2] = '\0'; break;
			case LC_RUSSIA:		country_code[0] = 'r'; country_code[1] = 'u'; country_code[2] = '\0'; break;
			case LC_DENMARK:	country_code[0] = 'd'; country_code[1] = 'k'; country_code[2] = '\0'; break;
			case LC_BULGARIA:	country_code[0] = 'b'; country_code[1] = 'g'; country_code[2] = '\0'; break;
			case LC_CROATIA:	country_code[0] = 'h'; country_code[1] = 'r'; country_code[2] = '\0'; break;
			case LC_MEXICO:		country_code[0] = 'm'; country_code[1] = 'x'; country_code[2] = '\0'; break;
			case LC_ARABIA:		country_code[0] = 'a'; country_code[1] = 'e'; country_code[2] = '\0'; break;
			case LC_CZECH:		country_code[0] = 'c'; country_code[1] = 'z'; country_code[2] = '\0'; break;
			case LC_ROMANIA:	country_code[0] = 'r'; country_code[1] = 'o'; country_code[2] = '\0'; break;
			case LC_HUNGARY:	country_code[0] = 'h'; country_code[1] = 'u'; country_code[2] = '\0'; break;
			case LC_NETHERLANDS: country_code[0] = 'n'; country_code[1] = 'l'; country_code[2] = '\0'; break;
			case LC_USA:		country_code[0] = 'u'; country_code[1] = 's'; country_code[2] = '\0'; break;
			case LC_CANADA:	country_code[0] = 'c'; country_code[1] = 'a'; country_code[2] = '\0'; break;
			default:
				if (test_server == true)
				{
					country_code[0] = 'd'; country_code[1] = 'e'; country_code[2] = '\0';
				}
				break;
		}

		char buf[512+1];
		char sas[33];
		MD5_CTX ctx;
		const char sas_key[] = "GF9001";

		snprintf(buf, sizeof(buf), "%u%u%s", ch->GetPlayerID(), ch->GetAID(), sas_key);

		MD5Init(&ctx);
		MD5Update(&ctx, (const unsigned char *) buf, strlen(buf));
#ifdef __FreeBSD__
		MD5End(&ctx, sas);
#else
		static const char hex[] = "0123456789abcdef";
		unsigned char digest[16];
		MD5Final(digest, &ctx);
		int i;
		for (i = 0; i < 16; ++i) {
			sas[i+i] = hex[digest[i] >> 4];
			sas[i+i+1] = hex[digest[i] & 0x0f];
		}
		sas[i+i] = '\0';
#endif

		snprintf(buf, sizeof(buf), "mall http://%s/ishop?pid=%u&c=%s&sid=%d&sas=%s",
				g_strWebMallURL.c_str(), ch->GetPlayerID(), country_code, g_server_id, sas);

		ch->ChatPacket(CHAT_TYPE_COMMAND, buf);
	}
}

// ¡÷ªÁ¿ß
ACMD(do_dice)
{
    ch->ChatPacket(CHAT_TYPE_INFO, "Dice komudu bu serverde kullanilamaz.. ");
}

ACMD(do_click_mall)
{
	ch->ChatPacket(CHAT_TYPE_COMMAND, "ShowMeMallPassword");
}

ACMD(do_ride)
{
    dev_log(LOG_DEB0, "[DO_RIDE] start");
    if (ch->IsDead() || ch->IsStun())
	return;

    // ≥ª∏Æ±‚
    {
	if (ch->IsHorseRiding())
	{
	    dev_log(LOG_DEB0, "[DO_RIDE] stop riding");
	    ch->StopRiding(); 
	    return;
	}

	if (ch->GetMountVnum())
	{
	    dev_log(LOG_DEB0, "[DO_RIDE] unmount");
	    do_unmount(ch, NULL, 0, 0);
	    return;
	}
    }

    // ≈∏±‚
    {
	if (ch->GetHorse() != NULL)
	{
	    dev_log(LOG_DEB0, "[DO_RIDE] start riding");
	    ch->StartRiding();
	    return;
	}

	for (BYTE i=0; i<INVENTORY_MAX_NUM; ++i)
	{
	    LPITEM item = ch->GetInventoryItem(i);
	    if (NULL == item)
		continue;

	    // ¿Ø¥œ≈© ≈ª∞Õ æ∆¿Ã≈€
		if (item->IsRideItem())
		{
			if (NULL==ch->GetWear(WEAR_UNIQUE1) || NULL==ch->GetWear(WEAR_UNIQUE2))
			{
				dev_log(LOG_DEB0, "[DO_RIDE] USE UNIQUE ITEM");
				//ch->EquipItem(item);
				ch->UseItem(TItemPos (INVENTORY, i));
				return;
			}
		}

	    // ¿œπ› ≈ª∞Õ æ∆¿Ã≈€
	    // TODO : ≈ª∞ÕøÎ SubType √ﬂ∞°
	    switch (item->GetVnum())
	    {
		case 71114:	// ¿˙Ω≈¿ÃøÎ±«
		case 71116:	// ªÍ∞ﬂΩ≈¿ÃøÎ±«
		case 71118:	// ≈ı¡ˆπ¸¿ÃøÎ±«
		case 71120:	// ªÁ¿⁄ø’¿ÃøÎ±«
		    dev_log(LOG_DEB0, "[DO_RIDE] USE QUEST ITEM");
		    ch->UseItem(TItemPos (INVENTORY, i));
		    return;
	    }

		// GF mantis #113524, 52001~52090 π¯ ≈ª∞Õ
		if( (item->GetVnum() > 52000) && (item->GetVnum() < 52091) )	{
			dev_log(LOG_DEB0, "[DO_RIDE] USE QUEST ITEM");
			ch->UseItem(TItemPos (INVENTORY, i));
		    return;
		}
	}
    }


    // ≈∏∞≈≥™ ≥ª∏± ºˆ æ¯¿ª∂ß
    ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("∏ª¿ª ∏’¿˙ º“»Ø«ÿ¡÷ººø‰."));
}
#ifdef __AUCTION__
// temp_auction
ACMD(do_get_item_id_list)
{
	for (int i = 0; i < INVENTORY_MAX_NUM; i++)
	{
		LPITEM item = ch->GetInventoryItem(i);
		if (item != NULL)
			ch->ChatPacket(CHAT_TYPE_INFO, "name : %s id : %d", item->GetProto()->szName, item->GetID());
	}
}

// temp_auction

ACMD(do_enroll_auction)
{
	char arg1[256];
	char arg2[256];
	char arg3[256];
	char arg4[256];
	two_arguments (two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2)), arg3, sizeof(arg3), arg4, sizeof(arg4));

	DWORD item_id = strtoul(arg1, NULL, 10);
	BYTE empire = strtoul(arg2, NULL, 10);
	int bidPrice = strtol(arg3, NULL, 10);
	int immidiatePurchasePrice = strtol(arg4, NULL, 10);

	LPITEM item = ITEM_MANAGER::instance().Find(item_id);
	if (item == NULL)
		return;

	AuctionManager::instance().enroll_auction(ch, item, empire, bidPrice, immidiatePurchasePrice);
}

ACMD(do_enroll_wish)
{
	char arg1[256];
	char arg2[256];
	char arg3[256];
	one_argument (two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2)), arg3, sizeof(arg3));
	
	DWORD item_num = strtoul(arg1, NULL, 10);
	BYTE empire = strtoul(arg2, NULL, 10);
	int wishPrice = strtol(arg3, NULL, 10);

	AuctionManager::instance().enroll_wish(ch, item_num, empire, wishPrice);
}

ACMD(do_enroll_sale)
{
	char arg1[256];
	char arg2[256];
	char arg3[256];
	one_argument (two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2)), arg3, sizeof(arg3));
	
	DWORD item_id = strtoul(arg1, NULL, 10);
	DWORD wisher_id = strtoul(arg2, NULL, 10);
	int salePrice = strtol(arg3, NULL, 10);

	LPITEM item = ITEM_MANAGER::instance().Find(item_id);
	if (item == NULL)
		return;

	AuctionManager::instance().enroll_sale(ch, item, wisher_id, salePrice);
}

// temp_auction
// packet¿∏∑Œ ≈ÎΩ≈«œ∞‘ «œ∞Ì, ¿Ã∞« ªË¡¶«ÿæﬂ«—¥Ÿ.
ACMD(do_get_auction_list)
{
	char arg1[256];
	char arg2[256];
	char arg3[256];
	two_arguments (one_argument (argument, arg1, sizeof(arg1)), arg2, sizeof(arg2), arg3, sizeof(arg3));

	AuctionManager::instance().get_auction_list (ch, strtoul(arg1, NULL, 10), strtoul(arg2, NULL, 10), strtoul(arg3, NULL, 10));
}
//
//ACMD(do_get_wish_list)
//{
//	char arg1[256];
//	char arg2[256];
//	char arg3[256];
//	two_arguments (one_argument (argument, arg1, sizeof(arg1)), arg2, sizeof(arg2), arg3, sizeof(arg3));
//
//	AuctionManager::instance().get_wish_list (ch, strtoul(arg1, NULL, 10), strtoul(arg2, NULL, 10), strtoul(arg3, NULL, 10));
//}
ACMD (do_get_my_auction_list)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	AuctionManager::instance().get_my_auction_list (ch, strtoul(arg1, NULL, 10), strtoul(arg2, NULL, 10));
}

ACMD (do_get_my_purchase_list)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	AuctionManager::instance().get_my_purchase_list (ch, strtoul(arg1, NULL, 10), strtoul(arg2, NULL, 10));
}

ACMD (do_auction_bid)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	AuctionManager::instance().bid (ch, strtoul(arg1, NULL, 10), strtoul(arg2, NULL, 10));
}

ACMD (do_auction_impur)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof(arg1));

	AuctionManager::instance().immediate_purchase (ch, strtoul(arg1, NULL, 10));
}

ACMD (do_get_auctioned_item)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	AuctionManager::instance().get_auctioned_item (ch, strtoul(arg1, NULL, 10), strtoul(arg2, NULL, 10));
}

ACMD (do_buy_sold_item)
{
	char arg1[256];
	char arg2[256];
	one_argument (argument, arg1, sizeof(arg1));

	AuctionManager::instance().get_auctioned_item (ch, strtoul(arg1, NULL, 10), strtoul(arg2, NULL, 10));
}

ACMD (do_cancel_auction)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof(arg1));

	AuctionManager::instance().cancel_auction (ch, strtoul(arg1, NULL, 10));
}

ACMD (do_cancel_wish)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof(arg1));

	AuctionManager::instance().cancel_wish (ch, strtoul(arg1, NULL, 10));
}

ACMD (do_cancel_sale)
{
	char arg1[256];
	one_argument (argument, arg1, sizeof(arg1));

	AuctionManager::instance().cancel_sale (ch, strtoul(arg1, NULL, 10));
}

ACMD (do_rebid)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	AuctionManager::instance().rebid (ch, strtoul(arg1, NULL, 10), strtoul(arg2, NULL, 10));
}

ACMD (do_bid_cancel)
{
	char arg1[256];
	char arg2[256];
	two_arguments (argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	AuctionManager::instance().bid_cancel (ch, strtoul(arg1, NULL, 10));
}
#endif

ACMD(do_bonus_costume_transfer)
{
	const char *line;
	int inven_index1 = 0, inven_index2 = 0,inven_index3=0;
	LPITEM	item1;
	LPITEM	item2;
	LPITEM	item3;
	
	char arg1[256], arg2[256], arg3[256];
	line = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	one_argument(line, arg3, sizeof(arg3));
	if (!*arg1)
	{
		return;
	}
	if (!*arg2)
	{
		return;
	}
	if (!*arg3)
	{
		return;
	}
	if (*arg2)
		str_to_number(inven_index2, arg2);
	if (*arg1)
		str_to_number(inven_index1, arg1);
	if (*arg3)
		str_to_number(inven_index3, arg3);

	item1 = ch->GetInventoryItem(inven_index1);
	item2 = ch->GetInventoryItem(inven_index2);
	item3 = ch->GetInventoryItem(inven_index3);
	
	if(item1->GetSubType()!=COSTUME_BODY)
		return;
	if(item2->GetSubType()!=COSTUME_BODY)
		return;
	if(item3->GetVnum()!=70065)
		return;
	
	int attrs = item2->GetAttributeCount();
	int i,type,value;
	for(i=0;i<=ITEM_ATTRIBUTE_MAX_NUM;i++)
		item1->SetForceAttribute(i, 0, 0);
	for(i=0;i<=attrs;i++){
		type=item2->GetAttributeType(i);
		value=item2->GetAttributeValue(i);
		item1->SetForceAttribute(i, type, value);
	}
	ITEM_MANAGER::instance().RemoveItem(item2);	
	ch->RemoveSpecifyItem(70065,1);
}

ACMD(do_open_offline_shop)
{
	// If character is dead, return false
	if (ch->IsDead())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Oluyken cevrimdisi pazar acamazsin..");
		return;
	}

	// If character is exchanging with someone, return false
	if (ch->GetExchange())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Bu itemi ticarette kullaniyorsun..");
		return;
	}

	// If character has a private shop, return false
	if (ch->GetMyShop())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Daha once acik olan pazarinizi kapatiniz..");
		return;
	}

	// If character is look at one offline shop, return false
	if (ch->GetOfflineShop())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Cevrimdisi pazar magazasina bakarken pazar kuramazsin..");
		return;
	}

	// If cube window is open, return false
	if (ch->IsCubeOpen())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Pencere acik oldugu icin pazar kuramam!");
		return;
	}

	// If character's safebox is open, return false
	if (ch->IsOpenSafebox())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Depon acik oldugu icin cevrimdisi pazar kuramazsin!");
		return;
	}

	// If character's shop window is open, return false
	if (ch->GetShop())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "item pencerisi acik oldugu icin pazar kuramam!");
		return;
	}

	// Send the command to client.
	ch->ChatPacket(CHAT_TYPE_COMMAND, "OpenOfflineShop");
}