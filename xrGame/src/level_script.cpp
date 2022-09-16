////////////////////////////////////////////////////////////////////////////
//	Module 		: level_script.cpp
//	Created 	: 28.06.2004
//  Modified 	: 28.06.2004
//	Author		: Dmitriy Iassenev
//	Description : Level script export
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"
#include "level.h"
#include "actor.h"
#include "script_game_object.h"
#include "patrol_path_storage.h"
#include "xrServer.h"
#include "client_spawn_manager.h"
#include "../xrEngine/igame_persistent.h"
#include "game_cl_base.h"
#include "UIGameCustom.h"
#include "UI/UIDialogWnd.h"
#include "date_time.h"
#include "ai_space.h"
#include "level_graph.h"
#include "PHCommander.h"
#include "PHScriptCall.h"
#include "script_engine.h"
#include "game_cl_single.h"
#include "game_sv_single.h"
#include "map_manager.h"
#include "map_spot.h"
#include "map_location.h"
#include "physics_world_scripted.h"
#include "alife_simulator.h"
#include "alife_time_manager.h"
#include "UI/UIGameTutorial.h"
#include "string_table.h"
#include "ui/UIInventoryUtilities.h"
#include "alife_object_registry.h"
#include "xrServer_Objects_ALife_Monsters.h"
#include "hudmanager.h"
#include "ui\UIMainIngameWnd.h"
#include "ui\UIHudStatesWnd.h"
#include "raypick.h"
#include "../xrcdb/xr_collide_defs.h"
#include "../xrEngine/Rain.h"

#include "../xrEngine/xr_efflensflare.h"
#include "../xrEngine/thunderbolt.h"
#include "GametaskManager.h"
#include "xr_level_controller.h"
#include "../xrEngine/GameMtlLib.h"
#include "../xrEngine/xr_input.h"
#include "script_ini_file.h"

using namespace luabind;

extern ENGINE_API float ps_r2_sun_shafts_min;
extern ENGINE_API float ps_r2_sun_shafts_value;
bool g_block_all_except_movement;
bool g_actor_allow_ladder = true;

LPCSTR command_line()
{
	return (Core.Params);
}

bool IsDynamicMusic()
{
	return !!psActorFlags.test(AF_DYNAMIC_MUSIC);
}

bool IsImportantSave()
{
	return !!psActorFlags.test(AF_IMPORTANT_SAVE);
}

#ifdef DEBUG
void check_object(CScriptGameObject *object)
{
	try {
		Msg	("check_object %s",object->Name());
	}
	catch(...) {
		object = object;
	}
}

CScriptGameObject *tpfGetActor()
{
	static bool first_time = true;
	if (first_time)
		ai().script_engine().script_log(eLuaMessageTypeError,"Do not use level.actor function!");
	first_time = false;
	
	CActor *l_tpActor = smart_cast<CActor*>(Level().CurrentEntity());
	if (l_tpActor)
		return	(smart_cast<CGameObject*>(l_tpActor)->lua_game_object());
	else
		return	(0);
}

CScriptGameObject *get_object_by_name(LPCSTR caObjectName)
{
	static bool first_time = true;
	if (first_time)
		ai().script_engine().script_log(eLuaMessageTypeError,"Do not use level.object function!");
	first_time = false;
	
	CGameObject		*l_tpGameObject	= smart_cast<CGameObject*>(Level().Objects.FindObjectByName(caObjectName));
	if (l_tpGameObject)
		return		(l_tpGameObject->lua_game_object());
	else
		return		(0);
}
#endif

CScriptGameObject* get_object_by_id(luabind::object ob)
{
	if (!ob || ob.type() == LUA_TNIL)
	{
		Msg("!WARNING : level.object_by_id(nil) called!");
		return nullptr;
	}

	u16 id = luabind::object_cast<u16>(ob);

	CGameObject* pGameObject = smart_cast<CGameObject*>(Level().Objects.net_Find(id));
	if (!pGameObject)
		return nullptr;

	return pGameObject->lua_game_object();
}

LPCSTR get_weather()
{
	return (*g_pGamePersistent->Environment().GetWeather());
}

void set_weather(LPCSTR weather_name, bool forced)
{
#ifdef INGAME_EDITOR
	if (!Device.editor())
#endif // #ifdef INGAME_EDITOR
	g_pGamePersistent->Environment().SetWeather(weather_name, forced);
}

bool set_weather_fx(LPCSTR weather_name)
{
#ifdef INGAME_EDITOR
	if (!Device.editor())
#endif // #ifdef INGAME_EDITOR
	return (g_pGamePersistent->Environment().SetWeatherFX(weather_name));

#ifdef INGAME_EDITOR
	return			(false);
#endif // #ifdef INGAME_EDITOR
}

bool start_weather_fx_from_time(LPCSTR weather_name, float time)
{
#ifdef INGAME_EDITOR
	if (!Device.editor())
#endif // #ifdef INGAME_EDITOR
	return (g_pGamePersistent->Environment().StartWeatherFXFromTime(weather_name, time));

#ifdef INGAME_EDITOR
	return			(false);
#endif // #ifdef INGAME_EDITOR
}

bool is_wfx_playing()
{
	return (g_pGamePersistent->Environment().IsWFXPlaying());
}

float get_wfx_time()
{
	return (g_pGamePersistent->Environment().wfx_time);
}

void stop_weather_fx()
{
	g_pGamePersistent->Environment().StopWFX();
}

void set_time_factor(float time_factor)
{
	if (!OnServer())
		return;

#ifdef INGAME_EDITOR
	if (Device.editor())
		return;
#endif // #ifdef INGAME_EDITOR

	Level().Server->game->SetGameTimeFactor(time_factor);
}

float get_time_factor()
{
	return (Level().GetGameTimeFactor());
}

void set_game_difficulty(ESingleGameDifficulty dif)
{
	g_SingleGameDifficulty = dif;
	game_cl_Single* game = smart_cast<game_cl_Single*>(Level().game);
	VERIFY(game);
	game->OnDifficultyChanged();
}

ESingleGameDifficulty get_game_difficulty()
{
	return g_SingleGameDifficulty;
}

u32 get_time_days()
{
	u32 year = 0, month = 0, day = 0, hours = 0, mins = 0, secs = 0, milisecs = 0;
	split_time((g_pGameLevel && Level().game) ? Level().GetGameTime() : ai().alife().time_manager().game_time(), year,
	           month, day, hours, mins, secs, milisecs);
	return day;
}

u32 get_time_hours()
{
	u32 year = 0, month = 0, day = 0, hours = 0, mins = 0, secs = 0, milisecs = 0;
	split_time((g_pGameLevel && Level().game) ? Level().GetGameTime() : ai().alife().time_manager().game_time(), year,
	           month, day, hours, mins, secs, milisecs);
	return hours;
}

u32 get_time_minutes()
{
	u32 year = 0, month = 0, day = 0, hours = 0, mins = 0, secs = 0, milisecs = 0;
	split_time((g_pGameLevel && Level().game) ? Level().GetGameTime() : ai().alife().time_manager().game_time(), year,
	           month, day, hours, mins, secs, milisecs);
	return mins;
}

void change_game_time(u32 days, u32 hours, u32 mins)
{
	game_sv_Single* tpGame = smart_cast<game_sv_Single *>(Level().Server->game);
	if (tpGame && ai().get_alife())
	{
		u32 value = days * 86400 + hours * 3600 + mins * 60;
		float fValue = static_cast<float>(value);
		value *= 1000; //msec		
		g_pGamePersistent->Environment().ChangeGameTime(fValue);
		tpGame->alife().time_manager().change_game_time(value);
	}
}

float high_cover_in_direction(u32 level_vertex_id, const Fvector& direction)
{
	if (!ai().level_graph().valid_vertex_id(level_vertex_id))
	{
		return 0;
	}

	float y, p;
	direction.getHP(y, p);
	return (ai().level_graph().high_cover_in_direction(y, level_vertex_id));
}

float low_cover_in_direction(u32 level_vertex_id, const Fvector& direction)
{
	if (!ai().level_graph().valid_vertex_id(level_vertex_id))
	{
		return 0;
	}

	float y, p;
	direction.getHP(y, p);
	return (ai().level_graph().low_cover_in_direction(y, level_vertex_id));
}

float rain_factor()
{
	return (g_pGamePersistent->Environment().CurrentEnv->rain_density);
}

u32 vertex_in_direction(u32 level_vertex_id, Fvector direction, float max_distance)
{
	if (!ai().level_graph().valid_vertex_id(level_vertex_id))
	{
		return u32(-1);
	}
	direction.normalize_safe();
	direction.mul(max_distance);
	Fvector start_position = ai().level_graph().vertex_position(level_vertex_id);
	Fvector finish_position = Fvector(start_position).add(direction);
	u32 result = u32(-1);
	ai().level_graph().farthest_vertex_in_direction(level_vertex_id, start_position, finish_position, result, 0);
	return (ai().level_graph().valid_vertex_id(result) ? result : level_vertex_id);
}

Fvector vertex_position(u32 level_vertex_id)
{
	if (!ai().level_graph().valid_vertex_id(level_vertex_id))
	{
		return Fvector{};
	}
	return (ai().level_graph().vertex_position(level_vertex_id));
}

void map_add_object_spot(u16 id, LPCSTR spot_type, LPCSTR text)
{
	CMapLocation* ml = Level().MapManager().AddMapLocation(spot_type, id);
	if (xr_strlen(text))
	{
		ml->SetHint(text);
	}
}

void map_add_object_spot_ser(u16 id, LPCSTR spot_type, LPCSTR text)
{
	CMapLocation* ml = Level().MapManager().AddMapLocation(spot_type, id);
	if (xr_strlen(text))
		ml->SetHint(text);

	ml->SetSerializable(true);
}

void map_change_spot_hint(u16 id, LPCSTR spot_type, LPCSTR text)
{
	CMapLocation* ml = Level().MapManager().GetMapLocation(spot_type, id);
	if (!ml) return;
	ml->SetHint(text);
}

void map_remove_object_spot(u16 id, LPCSTR spot_type)
{
	Level().MapManager().RemoveMapLocation(spot_type, id);
}

u16 map_has_object_spot(u16 id, LPCSTR spot_type)
{
	return Level().MapManager().HasMapLocation(spot_type, id);
}

bool patrol_path_exists(LPCSTR patrol_path)
{
	return (!!ai().patrol_paths().path(patrol_path, true));
}

LPCSTR get_name()
{
	return (*Level().name());
}

void prefetch_sound(LPCSTR name)
{
	Level().PrefetchSound(name);
}


CClientSpawnManager& get_client_spawn_manager()
{
	return (Level().client_spawn_manager());
}

/*
void start_stop_menu(CUIDialogWnd* pDialog, bool bDoHideIndicators)
{
	if(pDialog->IsShown())
		pDialog->HideDialog();
	else
		pDialog->ShowDialog(bDoHideIndicators);
}
*/

void add_dialog_to_render(CUIDialogWnd* pDialog)
{
	CurrentGameUI()->AddDialogToRender(pDialog);
}

void remove_dialog_to_render(CUIDialogWnd* pDialog)
{
	CurrentGameUI()->RemoveDialogToRender(pDialog);
}

void hide_indicators()
{
	if (CurrentGameUI())
	{
		CurrentGameUI()->HideShownDialogs();
		CurrentGameUI()->ShowGameIndicators(false);
		CurrentGameUI()->ShowCrosshair(false);
	}
	psActorFlags.set(AF_GODMODE_RT, TRUE);
}

void hide_indicators_safe()
{
	if (CurrentGameUI())
	{
		CurrentGameUI()->ShowGameIndicators(false);
		CurrentGameUI()->ShowCrosshair(false);

		CurrentGameUI()->OnExternalHideIndicators();
	}
	psActorFlags.set(AF_GODMODE_RT, TRUE);
}

void show_indicators()
{
	if (CurrentGameUI())
	{
		CurrentGameUI()->ShowGameIndicators(true);
		CurrentGameUI()->ShowCrosshair(true);
	}
	psActorFlags.set(AF_GODMODE_RT, FALSE);
}

void show_weapon(bool b)
{
	psHUD_Flags.set(HUD_WEAPON_RT2, b);
}

bool is_level_present()
{
	return (!!g_pGameLevel);
}

void add_call(const luabind::functor<bool>& condition, const luabind::functor<void>& action)
{
	luabind::functor<bool> _condition = condition;
	luabind::functor<void> _action = action;
	CPHScriptCondition* c = xr_new<CPHScriptCondition>(_condition);
	CPHScriptAction* a = xr_new<CPHScriptAction>(_action);
	Level().ph_commander_scripts().add_call(c, a);
}

void remove_call(const luabind::functor<bool>& condition, const luabind::functor<void>& action)
{
	CPHScriptCondition c(condition);
	CPHScriptAction a(action);
	Level().ph_commander_scripts().remove_call(&c, &a);
}

void add_call(const luabind::object& lua_object, LPCSTR condition, LPCSTR action)
{
	//	try{	
	//		CPHScriptObjectCondition	*c=xr_new<CPHScriptObjectCondition>(lua_object,condition);
	//		CPHScriptObjectAction		*a=xr_new<CPHScriptObjectAction>(lua_object,action);
	luabind::functor<bool> _condition = object_cast<luabind::functor<bool>>(lua_object[condition]);
	luabind::functor<void> _action = object_cast<luabind::functor<void>>(lua_object[action]);
	CPHScriptObjectConditionN* c = xr_new<CPHScriptObjectConditionN>(lua_object, _condition);
	CPHScriptObjectActionN* a = xr_new<CPHScriptObjectActionN>(lua_object, _action);
	Level().ph_commander_scripts().add_call_unique(c, c, a, a);
	//	}
	//	catch(...)
	//	{
	//		Msg("add_call excepted!!");
	//	}
}

void remove_call(const luabind::object& lua_object, LPCSTR condition, LPCSTR action)
{
	CPHScriptObjectCondition c(lua_object, condition);
	CPHScriptObjectAction a(lua_object, action);
	Level().ph_commander_scripts().remove_call(&c, &a);
}

void add_call(const luabind::object& lua_object, const luabind::functor<bool>& condition,
              const luabind::functor<void>& action)
{
	CPHScriptObjectConditionN* c = xr_new<CPHScriptObjectConditionN>(lua_object, condition);
	CPHScriptObjectActionN* a = xr_new<CPHScriptObjectActionN>(lua_object, action);
	Level().ph_commander_scripts().add_call(c, a);
}

void remove_call(const luabind::object& lua_object, const luabind::functor<bool>& condition,
                 const luabind::functor<void>& action)
{
	CPHScriptObjectConditionN c(lua_object, condition);
	CPHScriptObjectActionN a(lua_object, action);
	Level().ph_commander_scripts().remove_call(&c, &a);
}

void remove_calls_for_object(const luabind::object& lua_object)
{
	CPHSriptReqObjComparer c(lua_object);
	Level().ph_commander_scripts().remove_calls(&c);
}

cphysics_world_scripted* physics_world_scripted()
{
	return get_script_wrapper<cphysics_world_scripted>(*physics_world());
}

CEnvironment* environment()
{
	return (g_pGamePersistent->pEnvironment);
}

CEnvDescriptor* current_environment(CEnvironment* self)
{
	return (self->CurrentEnv);
}

extern bool g_bDisableAllInput;

void disable_input()
{
	// "unpress" all keys when we disable level input! (but keep input devices aquired)
	pInput->DeactivateSoft();
	g_bDisableAllInput = true;
#ifdef DEBUG
	Msg("input disabled");
#endif // #ifdef DEBUG
}

void enable_input()
{
	g_bDisableAllInput = false;
#ifdef DEBUG
	Msg("input enabled");
#endif // #ifdef DEBUG
}

void spawn_phantom(const Fvector& position)
{
	Level().spawn_item("m_phantom", position, u32(-1), u16(-1), false);
}

Fbox get_bounding_volume()
{
	return Level().ObjectSpace.GetBoundingVolume();
}

void iterate_sounds(LPCSTR prefix, u32 max_count, const CScriptCallbackEx<void>& callback)
{
	for (int j = 0, N = _GetItemCount(prefix); j < N; ++j)
	{
		string_path fn, s;
		LPSTR S = (LPSTR)&s;
		_GetItem(prefix, j, s);
		if (FS.exist(fn, "$game_sounds$", S, ".ogg"))
			callback(prefix);

		for (u32 i = 0; i < max_count; ++i)
		{
			string_path name;
			xr_sprintf(name, "%s%d", S, i);
			if (FS.exist(fn, "$game_sounds$", name, ".ogg"))
				callback(name);
		}
	}
}

void iterate_sounds1(LPCSTR prefix, u32 max_count, luabind::functor<void> functor)
{
	CScriptCallbackEx<void> temp;
	temp.set(functor);
	iterate_sounds(prefix, max_count, temp);
}

void iterate_sounds2(LPCSTR prefix, u32 max_count, luabind::object object, luabind::functor<void> functor)
{
	CScriptCallbackEx<void> temp;
	temp.set(functor, object);
	iterate_sounds(prefix, max_count, temp);
}

#include "actoreffector.h"

float add_cam_effector(LPCSTR fn, int id, bool cyclic, LPCSTR cb_func)
{
	CAnimatorCamEffectorScriptCB* e = xr_new<CAnimatorCamEffectorScriptCB>(cb_func);
	e->SetType((ECamEffectorType)id);
	e->SetCyclic(cyclic);
	e->Start(fn);
	Actor()->Cameras().AddCamEffector(e);
	return e->GetAnimatorLength();
}

float add_cam_effector(LPCSTR fn, int id, bool cyclic, LPCSTR cb_func, float cam_fov)
{
	CAnimatorCamEffectorScriptCB* e = xr_new<CAnimatorCamEffectorScriptCB>(cb_func);
	if (cam_fov)
	{
		e->m_bAbsolutePositioning = true;
		e->m_fov = cam_fov;
	}
	e->SetType((ECamEffectorType)id);
	e->SetCyclic(cyclic);
	e->Start(fn);
	Actor()->Cameras().AddCamEffector(e);
	return e->GetAnimatorLength();
}

float add_cam_effector(LPCSTR fn, int id, bool cyclic, LPCSTR cb_func, float cam_fov, bool b_hud)
{
	CAnimatorCamEffectorScriptCB* e = xr_new<CAnimatorCamEffectorScriptCB>(cb_func);
	if (cam_fov)
	{
		e->m_bAbsolutePositioning = true;
		e->m_fov = cam_fov;
	}
	e->SetHudAffect(b_hud);
	e->SetType((ECamEffectorType)id);
	e->SetCyclic(cyclic);
	e->Start(fn);
	Actor()->Cameras().AddCamEffector(e);
	return e->GetAnimatorLength();
}

float add_cam_effector(LPCSTR fn, int id, bool cyclic, LPCSTR cb_func, float cam_fov, bool b_hud, float power)
{
	CAnimatorCamEffectorScriptCB* e = xr_new<CAnimatorCamEffectorScriptCB>(cb_func);
	if (cam_fov)
	{
		e->m_bAbsolutePositioning = true;
		e->m_fov = cam_fov;
	}
	if (power)
	{
		e->SetPower(power);
	}
	e->SetHudAffect(b_hud);
	e->SetType((ECamEffectorType)id);
	e->SetCyclic(cyclic);
	e->Start(fn);
	Actor()->Cameras().AddCamEffector(e);
	return e->GetAnimatorLength();
}

void remove_cam_effector(int id)
{
	Actor()->Cameras().RemoveCamEffector((ECamEffectorType)id);
}

void set_cam_effector_factor(int id, float factor)
{
	CAnimatorCamEffectorScriptCB* e = smart_cast<CAnimatorCamEffectorScriptCB*>(Actor()->Cameras().GetCamEffector((ECamEffectorType)id));
	if (e)
		e->SetPower(factor);
}

float get_cam_effector_factor(int id)
{
	CAnimatorCamEffectorScriptCB* e = smart_cast<CAnimatorCamEffectorScriptCB*>(Actor()->Cameras().GetCamEffector((ECamEffectorType)id));
	return e ? e->GetPower() : 0.0f;
}

float get_cam_effector_length(int id)
{
	CAnimatorCamEffectorScriptCB* e = smart_cast<CAnimatorCamEffectorScriptCB*>(Actor()->Cameras().GetCamEffector((ECamEffectorType)id));
	return e ? e->GetAnimatorLength() : 0.0f;
}


bool check_cam_effector(int id)
{
	CAnimatorCamEffectorScriptCB* e = smart_cast<CAnimatorCamEffectorScriptCB*>(Actor()->Cameras().GetCamEffector((ECamEffectorType)id));
	if (e)
	{
		return e->Valid();
	}
	return false;
}


float get_snd_volume()
{
	return psSoundVFactor;
}

float get_rain_volume()
{
	CEffect_Rain* rain = g_pGamePersistent->pEnvironment->eff_Rain;
	return rain ? rain->GetRainVolume() : 0.0f;
}

void set_snd_volume(float v)
{
	psSoundVFactor = v;
	clamp(psSoundVFactor, 0.0f, 1.0f);
}

#include "actor_statistic_mgr.h"

void add_actor_points(LPCSTR sect, LPCSTR detail_key, int cnt, int pts)
{
	return Actor()->StatisticMgr().AddPoints(sect, detail_key, cnt, pts);
}

void add_actor_points_str(LPCSTR sect, LPCSTR detail_key, LPCSTR str_value)
{
	return Actor()->StatisticMgr().AddPoints(sect, detail_key, str_value);
}

int get_actor_points(LPCSTR sect)
{
	return Actor()->StatisticMgr().GetSectionPoints(sect);
}


#include "ActorEffector.h"

void add_complex_effector(LPCSTR section, int id)
{
	AddEffector(Actor(), id, section);
}

void remove_complex_effector(int id)
{
	RemoveEffector(Actor(), id);
}

#include "postprocessanimator.h"

void add_pp_effector(LPCSTR fn, int id, bool cyclic)
{
	CPostprocessAnimator* pp = xr_new<CPostprocessAnimator>(id, cyclic);
	pp->Load(fn);
	Actor()->Cameras().AddPPEffector(pp);
}

void remove_pp_effector(int id)
{
	CPostprocessAnimator* pp = smart_cast<CPostprocessAnimator*>(Actor()->Cameras().GetPPEffector((EEffectorPPType)id));

	if (pp) pp->Stop(1.0f);
}

void set_pp_effector_factor(int id, float f, float f_sp)
{
	CPostprocessAnimator* pp = smart_cast<CPostprocessAnimator*>(Actor()->Cameras().GetPPEffector((EEffectorPPType)id));

	if (pp) pp->SetDesiredFactor(f, f_sp);
}

void set_pp_effector_factor2(int id, float f)
{
	CPostprocessAnimator* pp = smart_cast<CPostprocessAnimator*>(Actor()->Cameras().GetPPEffector((EEffectorPPType)id));

	if (pp) pp->SetCurrentFactor(f);
}

#include "relation_registry.h"

int g_community_goodwill(LPCSTR _community, int _entity_id)
{
	CHARACTER_COMMUNITY c;
	c.set(_community);

	return RELATION_REGISTRY().GetCommunityGoodwill(c.index(), u16(_entity_id));
}

void g_set_community_goodwill(LPCSTR _community, int _entity_id, int val)
{
	CHARACTER_COMMUNITY c;
	c.set(_community);
	RELATION_REGISTRY().SetCommunityGoodwill(c.index(), u16(_entity_id), val);
}

void g_change_community_goodwill(LPCSTR _community, int _entity_id, int val)
{
	CHARACTER_COMMUNITY c;
	c.set(_community);
	RELATION_REGISTRY().ChangeCommunityGoodwill(c.index(), u16(_entity_id), val);
}

int g_get_community_relation(LPCSTR comm_from, LPCSTR comm_to)
{
	CHARACTER_COMMUNITY community_from;
	community_from.set(comm_from);
	CHARACTER_COMMUNITY community_to;
	community_to.set(comm_to);

	return RELATION_REGISTRY().GetCommunityRelation(community_from.index(), community_to.index());
}

void g_set_community_relation(LPCSTR comm_from, LPCSTR comm_to, int value)
{
	CHARACTER_COMMUNITY community_from;
	community_from.set(comm_from);
	CHARACTER_COMMUNITY community_to;
	community_to.set(comm_to);

	RELATION_REGISTRY().SetCommunityRelation(community_from.index(), community_to.index(), value);
}

int g_get_general_goodwill_between(u16 from, u16 to)
{
	CHARACTER_GOODWILL presonal_goodwill = RELATION_REGISTRY().GetGoodwill(from, to);
	VERIFY(presonal_goodwill != NO_GOODWILL);

	CSE_ALifeTraderAbstract* from_obj = smart_cast<CSE_ALifeTraderAbstract*>(ai().alife().objects().object(from));
	CSE_ALifeTraderAbstract* to_obj = smart_cast<CSE_ALifeTraderAbstract*>(ai().alife().objects().object(to));

	if (!from_obj || !to_obj)
	{
		ai().script_engine().script_log(ScriptStorage::eLuaMessageTypeError,
		                                "RELATION_REGISTRY::get_general_goodwill_between  : cannot convert obj to CSE_ALifeTraderAbstract!");
		return (0);
	}
	CHARACTER_GOODWILL community_to_obj_goodwill = RELATION_REGISTRY().GetCommunityGoodwill(from_obj->Community(), to);
	CHARACTER_GOODWILL community_to_community_goodwill = RELATION_REGISTRY().GetCommunityRelation(
		from_obj->Community(), to_obj->Community());

	return presonal_goodwill + community_to_obj_goodwill + community_to_community_goodwill;
}

void refresh_npc_names()
{
	CALifeObjectRegistry::OBJECT_REGISTRY alobjs = ai().alife().objects().objects();
	CALifeObjectRegistry::OBJECT_REGISTRY::iterator it = alobjs.begin();
	CALifeObjectRegistry::OBJECT_REGISTRY::iterator it_e = alobjs.end();

	for (; it != it_e; it++)
	{
		CSE_ALifeTraderAbstract* tr = smart_cast<CSE_ALifeTraderAbstract*>(it->second);
		if (tr)
		{
			tr->m_character_name = TranslateName(tr->m_character_name_str.c_str());

			if (g_pGameLevel)
			{
				CObject* obj = g_pGameLevel->Objects.net_Find(it->first);
				CInventoryOwner* owner = smart_cast<CInventoryOwner*>(obj);
				if (owner)
					owner->refresh_npc_name();
			}
		}
	}

	if (psDeviceFlags2.test(rsDiscord))
	{
		Actor()->RPC_UpdateFaction();
		Actor()->RPC_UpdateRank();
		Actor()->RPC_UpdateReputation();

		Level().GameTaskManager().RPC_UpdateTaskName();
	}
}


void LevelPressAction(int cmd)
{
	if ((cmd == MOUSE_1 || cmd == MOUSE_2) && !!GetSystemMetrics(SM_SWAPBUTTON))
		cmd = cmd == MOUSE_1 ? MOUSE_2 : MOUSE_1;

	Level().IR_OnKeyboardPress(cmd);
}

void LevelReleaseAction(int cmd)
{
	if ((cmd == MOUSE_1 || cmd == MOUSE_2) && !!GetSystemMetrics(SM_SWAPBUTTON))
		cmd = cmd == MOUSE_1 ? MOUSE_2 : MOUSE_1;

	Level().IR_OnKeyboardRelease(cmd);
}

void LevelHoldAction(int cmd)
{
	if ((cmd == MOUSE_1 || cmd == MOUSE_2) && !!GetSystemMetrics(SM_SWAPBUTTON))
		cmd = cmd == MOUSE_1 ? MOUSE_2 : MOUSE_1;

	Level().IR_OnKeyboardHold(cmd);
}

u32 vertex_id(Fvector position)
{
	return (ai().level_graph().vertex_id(position));
}

u32 render_get_dx_level()
{
	return ::Render->get_dx_level();
}

CUISequencer* g_tutorial = NULL;
CUISequencer* g_tutorial2 = NULL;

void start_tutorial(LPCSTR name)
{
	if (g_tutorial)
	{
		VERIFY(!g_tutorial2);
		g_tutorial2 = g_tutorial;
	};

	g_tutorial = xr_new<CUISequencer>();
	g_tutorial->Start(name);
	if (g_tutorial2)
		g_tutorial->m_pStoredInputReceiver = g_tutorial2->m_pStoredInputReceiver;
}

void stop_tutorial()
{
	if (g_tutorial)
		g_tutorial->Stop();
}

LPCSTR translate_string(LPCSTR str)
{
	return *CStringTable().translate(str);
}

bool has_active_tutotial()
{
	return (g_tutorial != NULL);
}

float get_weather_value_numric(LPCSTR name)
{
	CEnvDescriptor& E = *environment()->CurrentEnv;

	if (0 == xr_strcmp(name, "sky_rotation"))
		return E.sky_rotation;
	else if (0 == xr_strcmp(name, "far_plane"))
		return E.far_plane;
	else if (0 == xr_strcmp(name, "fog_density"))
		return E.fog_density;
	else if (0 == xr_strcmp(name, "fog_distance"))
		return E.fog_distance;
	else if (0 == xr_strcmp(name, "rain_density"))
		return E.rain_density;
	else if (0 == xr_strcmp(name, "thunderbolt_period"))
		return E.bolt_period;
	else if (0 == xr_strcmp(name, "thunderbolt_duration"))
		return E.bolt_duration;
	else if (0 == xr_strcmp(name, "wind_velocity"))
		return E.wind_velocity;
	else if (0 == xr_strcmp(name, "wind_direction"))
		return E.wind_direction;
	else if (0 == xr_strcmp(name, "sun_shafts_intensity"))
		return E.m_fSunShaftsIntensity;
	else if (0 == xr_strcmp(name, "water_intensity"))
		return E.m_fWaterIntensity;
	else if (0 == xr_strcmp(name, "tree_amplitude_intensity"))
		return E.m_fTreeAmplitudeIntensity;
	else if (0 == xr_strcmp(name, "volumetric_intensity_factor"))
		return E.volumetric_intensity_factor;
	else if (0 == xr_strcmp(name, "volumetric_distance_factor"))
		return E.volumetric_distance_factor;

	return (0);
}

void set_weather_value_numric(LPCSTR name, float val)
{
	CEnvDescriptorMixer& E = *environment()->CurrentEnv;

	if (0 == xr_strcmp(name, "sky_rotation"))
		E.sky_rotation = val;
	else if (0 == xr_strcmp(name, "far_plane"))
		E.far_plane = val * psVisDistance;
	else if (0 == xr_strcmp(name, "fog_density"))
	{
		E.fog_density = val;
		E.fog_near = (1.0f - E.fog_density) * 0.85f * E.fog_distance;
	}
	else if (0 == xr_strcmp(name, "fog_distance"))
	{
		E.fog_distance = val;
		clamp(E.fog_distance, 1.f, E.far_plane - 10);
		E.fog_near = (1.0f - E.fog_density) * 0.85f * E.fog_distance;
		E.fog_far = 0.99f * E.fog_distance;
	}
	else if (0 == xr_strcmp(name, "rain_density"))
		E.rain_density = val;
	else if (0 == xr_strcmp(name, "thunderbolt_period"))
		E.bolt_period = val;
	else if (0 == xr_strcmp(name, "thunderbolt_duration"))
		E.bolt_duration = val;
	else if (0 == xr_strcmp(name, "wind_velocity"))
		E.wind_velocity = val;
	else if (0 == xr_strcmp(name, "wind_direction"))
		E.wind_direction = val;
	else if (0 == xr_strcmp(name, "sun_shafts_intensity"))
	{
		E.m_fSunShaftsIntensity = val;
		E.m_fSunShaftsIntensity *= 1.0f - ps_r2_sun_shafts_min;
		E.m_fSunShaftsIntensity += ps_r2_sun_shafts_min;
		E.m_fSunShaftsIntensity *= ps_r2_sun_shafts_value;
		clamp(E.m_fSunShaftsIntensity, 0.0f, 1.0f);
	}
	else if (0 == xr_strcmp(name, "water_intensity"))
		E.m_fWaterIntensity = val;
	else if (0 == xr_strcmp(name, "tree_amplitude_intensity"))
		E.m_fTreeAmplitudeIntensity = val;
	else if (0 == xr_strcmp(name, "volumetric_intensity_factor"))
		E.volumetric_intensity_factor = val;
	else if (0 == xr_strcmp(name, "volumetric_distance_factor"))
		E.volumetric_distance_factor = val;
	else
		Msg("~xrGame\level_script.cpp (set_weather_value_numric) | [%s] is not a valid numric weather parameter to set", name);
}

Fvector3 get_weather_value_vector(LPCSTR name)
{
	CEnvDescriptor& E = *environment()->CurrentEnv;

	if (0 == xr_strcmp(name, "sky_color"))
		return E.sky_color;
	else if (0 == xr_strcmp(name, "fog_color"))
		return E.fog_color;
	else if (0 == xr_strcmp(name, "rain_color"))
		return E.rain_color;
	else if (0 == xr_strcmp(name, "ambient_color"))
		return E.ambient;
	else if (0 == xr_strcmp(name, "sun_color"))
		return E.sun_color;
	else if (0 == xr_strcmp(name, "sun_dir"))
		return E.sun_dir;

	Fvector3 vec;
	vec.set(0, 0, 0);

	if (0 == xr_strcmp(name, "clouds_color"))
	{
		Fvector4 temp = E.clouds_color;
		vec.set(temp.x, temp.y, temp.z);
	}
	else if (0 == xr_strcmp(name, "hemisphere_color"))
	{
		Fvector4 temp = E.hemi_color;
		vec.set(temp.x, temp.y, temp.z);
	}

	return vec;
}

void set_weather_value_vector(LPCSTR name, float x, float y, float z, float w = 0)
{
	CEnvDescriptor& E = *environment()->CurrentEnv;

	if (0 == xr_strcmp(name, "sky_color"))
		E.sky_color.set(x, y, z);
	else if (0 == xr_strcmp(name, "fog_color"))
		E.fog_color.set(x, y, z);
	else if (0 == xr_strcmp(name, "rain_color"))
		E.rain_color.set(x, y, z);
	else if (0 == xr_strcmp(name, "ambient_color"))
		E.ambient.set(x, y, z);
	else if (0 == xr_strcmp(name, "sun_color"))
		E.sun_color.set(x, y, z);
	else if (0 == xr_strcmp(name, "clouds_color"))
		E.clouds_color.set(x, y, z, w);
	else if (0 == xr_strcmp(name, "hemisphere_color"))
		E.hemi_color.set(x, y, z, w);
	else
		Msg("~xrGame\level_script.cpp (set_weather_value_vector) | [%s] is not a valid vector weather parameter to set", name);
}

LPCSTR get_weather_value_string(LPCSTR name)
{
	CEnvDescriptor& E = *environment()->CurrentEnv;

	if (0 == xr_strcmp(name, "clouds_texture"))
		return E.clouds_texture_name.c_str();

	else if (0 == xr_strcmp(name, "sky_texture"))
		return E.sky_texture_name.c_str();

	else if (0 == xr_strcmp(name, "ambient"))
		return E.env_ambient->name().c_str();

	return "";
}

void set_weather_value_string(LPCSTR name, LPCSTR newval)
{
	CEnvDescriptor& E = *environment()->CurrentEnv;

	if (0 == xr_strcmp(name, "clouds_texture"))
	{
		if (E.clouds_texture_name._get() != shared_str(newval)._get())
		{
			E.m_pDescriptor->OnDeviceDestroy();
			E.clouds_texture_name = newval;
			E.m_pDescriptor->OnDeviceCreate(E);
		}
	}
	else if (0 == xr_strcmp(name, "sky_texture"))
	{
		if (E.sky_texture_name._get() != shared_str(newval)._get())
		{
			string_path st_env;
			strconcat(sizeof(st_env), st_env, newval, "#small");
			E.m_pDescriptor->OnDeviceDestroy();
			E.sky_texture_name = newval;
			E.sky_texture_env_name = st_env;
			E.m_pDescriptor->OnDeviceCreate(E);
		}
	}
	else if (0 == xr_strcmp(name, "sun"))
	{
		E.lens_flare_id = environment()->eff_LensFlare->AppendDef(*environment(), environment()->m_suns_config, newval);
	}
	else if (0 == xr_strcmp(name, "thunderbolt_collection"))
	{
		E.tb_id = environment()->eff_Thunderbolt->AppendDef(*environment(),
		                                                    environment()->m_thunderbolt_collections_config,
		                                                    environment()->m_thunderbolts_config, newval);
	}
	else if (0 == xr_strcmp(name, "ambient"))
	{
		E.env_ambient = environment()->AppendEnvAmb(newval);
	}
	else
		Msg("~xrGame\level_script.cpp (set_weather_value_string) | [%s] is not a valid string weather parameter to set", name);
}

void pause_weather(bool b_pause)
{
	environment()->m_paused = b_pause;
}

bool is_weather_paused()
{
	return environment()->m_paused;
}

void reload_weather()
{
	environment()->Reload();
}

void boost_weather_value(LPCSTR name, float value)
{
	if (0 == xr_strcmp(name, "ambient_color"))
		environment()->env_boost.ambient = value;
	else if (0 == xr_strcmp(name, "hemisphere_color"))
		environment()->env_boost.hemi = value;
	else if (0 == xr_strcmp(name, "fog_color"))
		environment()->env_boost.fog_color = value;
	else if (0 == xr_strcmp(name, "rain_color"))
		environment()->env_boost.rain_color = value;
	else if (0 == xr_strcmp(name, "sky_color"))
		environment()->env_boost.sky_color = value;
	else if (0 == xr_strcmp(name, "clouds_color"))
		environment()->env_boost.clouds_color = value;
	else if (0 == xr_strcmp(name, "sun_color"))
		environment()->env_boost.sun_color = value;
	else
		Msg("~xrGame\level_script.cpp (boost_weather_value)| [%s] is not a valid weather parameter to boost", name);
}

void boost_weather_reset()
{
	environment()->env_boost.ambient = 0.f;
	environment()->env_boost.hemi = 0.f;
	environment()->env_boost.fog_color = 0.f;
	environment()->env_boost.rain_color = 0.f;
	environment()->env_boost.sky_color = 0.f;
	environment()->env_boost.sun_color = 0.f;
}

void sun_time(int hour, int minute)
{
	float real_sun_alt, real_sun_long;
	float s_alt = environment()->sun_hp[hour].x;
	float s_long = environment()->sun_hp[hour].y;

	if (minute > 0)
	{
		float s_weight = minute / 60.f;
		int next_hour = hour == 23 ? 0 : hour + 1;
		float s_alt2 = environment()->sun_hp[next_hour].x;
		float s_long2 = environment()->sun_hp[next_hour].y;

		real_sun_alt = _lerp(s_alt, s_alt2, s_weight);
		real_sun_long = _lerp(s_long, s_long2, s_weight);
	}
	else
	{
		real_sun_alt = s_alt;
		real_sun_long = s_long;
	}

	R_ASSERT(_valid(real_sun_alt));
	R_ASSERT(_valid(real_sun_long));

	CEnvDescriptor& E = *environment()->CurrentEnv;
	E.sun_dir.setHP(
		deg2rad(real_sun_alt),
		deg2rad(real_sun_long)
	);

	R_ASSERT(_valid(E.sun_dir));
}

void reload_language()
{
	CStringTable().ReloadLanguage();
}

#include "player_hud.h"

void hud_adj_offs(int off, int idx, float x, float y, float z)
{
	// Script UI
	if (idx == 20)
	{
		g_player_hud->m_adjust_ui_offset[off].set(x, y, z);

		if (off == 1)
			g_player_hud->m_adjust_ui_offset[1].mul(PI / 180.f);
	}

	// Fire point/dir ; shell point
	else if (idx == 10 || idx == 11)
	{
		g_player_hud->m_adjust_firepoint_shell[off][idx-10].set(x, y, z);
	}

	// Object pos/dir
	else if (idx == 12)
	{
		g_player_hud->m_adjust_obj[off].set(x, y, z);
	}

	// Hud offsets
	else
		g_player_hud->m_adjust_offset[off][idx].set(x, y, z);
}

#include "Inventory.h"
#include "Weapon.h"

void hud_adj_value(LPCSTR name, float val)
{
	if (0 == xr_strcmp(name, "scope_zoom_factor"))
		g_player_hud->m_adjust_zoom_factor[0] = val;
	else if (0 == xr_strcmp(name, "gl_zoom_factor"))
		g_player_hud->m_adjust_zoom_factor[1] = val;
	else if (0 == xr_strcmp(name, "scope_zoom_factor_alt"))
		g_player_hud->m_adjust_zoom_factor[2] = val;
}

void hud_adj_state(bool state)
{
	g_player_hud->m_adjust_mode = state;
}

LPCSTR vid_modes_string()
{
	xr_string resolutions = "";

	xr_token* tok = vid_mode_token;
	while (tok->name)
	{
		if (strlen(resolutions.c_str()) > 0)
			resolutions.append(",");

		resolutions.append(tok->name);
		tok++;
	}

	return resolutions.c_str();
}

u32 PlayHudMotion(u8 hand, LPCSTR itm_name, LPCSTR anm_name, bool bMixIn = true, float speed = 1.f)
{
	return g_player_hud->script_anim_play(hand, itm_name, anm_name, bMixIn, speed);
}

void StopHudMotion()
{
	g_player_hud->StopScriptAnim();
}

float MotionLength(LPCSTR section, LPCSTR name, float speed)
{
	return g_player_hud->motion_length_script(section, name, speed);
}

bool AllowHudMotion()
{
	return g_player_hud->allow_script_anim();
}

void PlayBlendAnm(LPCSTR name, u8 part, float speed, float power, bool bLooped, bool no_restart)
{
	g_player_hud->PlayBlendAnm(name, part, speed, power, bLooped, no_restart);
}

void StopBlendAnm(LPCSTR name, bool bForce)
{
	g_player_hud->StopBlendAnm(name, bForce);
}

void StopAllBlendAnms(bool bForce)
{
	g_player_hud->StopAllBlendAnms(bForce);
}

float SetBlendAnmTime(LPCSTR name, float time)
{
	return g_player_hud->SetBlendAnmTime(name, time);
}

void block_all_except_movement(bool b)
{
	g_block_all_except_movement = b;
}

bool only_movement_allowed()
{
	return g_block_all_except_movement;
}

void set_actor_allow_ladder(bool b)
{
	g_actor_allow_ladder = b;
}

void set_nv_lumfactor(float factor)
{
	g_pGamePersistent->nv_shader_data.lum_factor = factor;
}

void remove_hud_model(LPCSTR section)
{
	LPCSTR hud_section = READ_IF_EXISTS(pSettings, r_string, section, "hud", nullptr);
	if (hud_section != nullptr)
		g_player_hud->remove_from_model_pool(hud_section);
	else
		Msg("can't find hud section for [%s]", section);
}

const u32 ActorMovingState()
{
	return g_actor->MovingState();
}

extern ENGINE_API float psHUD_FOV;

const Fvector2 world2ui(Fvector pos, bool hud = false)
{
	Fmatrix world, res;
	world.identity();
	world.c = pos;

	if (hud)
	{
		Fmatrix FP, FT, FV;
		FV.build_camera_dir(Device.vCameraPosition, Device.vCameraDirection, Device.vCameraTop);
		FP.build_projection(
			deg2rad(psHUD_FOV * 83.f),
			Device.fASPECT, R_VIEWPORT_NEAR,
			g_pGamePersistent->Environment().CurrentEnv->far_plane);

		FT.mul(FP, FV);
		res.mul(FT, world);
	}
	else
		res.mul(Device.mFullTransform, world);
	
	Fvector4 v_res;

	v_res.w = res._44;
	v_res.x = res._41 / v_res.w;
	v_res.y = res._42 / v_res.w;
	v_res.z = res._43 / v_res.w;

	if (v_res.z < 0 || v_res.w < 0) return { -9999,0 };
	if (abs(v_res.x) > 1.f || abs(v_res.y) > 1.f) return { -9999,0 };

	float x = (1.f + v_res.x) / 2.f * (Device.dwWidth);
	float y = (1.f - v_res.y) / 2.f * (Device.dwHeight);

	float width_fk = Device.dwWidth / UI_BASE_WIDTH;
	float height_fk = Device.dwHeight / UI_BASE_HEIGHT;

	x /= width_fk;
	y /= height_fk;

	return { x,y };
}

const float get_env_rads()
{
	if (!CurrentGameUI())
		return 0.f;

	return CurrentGameUI()->UIMainIngameWnd->get_hud_states()->get_main_sensor_value();
}

//Alundaio: namespace level exports extension
#ifdef NAMESPACE_LEVEL_EXPORTS
//ability to update level netpacket
void g_send(NET_Packet& P, bool bReliable = 0, bool bSequential = 1, bool bHighPriority = 0, bool bSendImmediately = 0)
{
	Level().Send(P, net_flags(bReliable, bSequential, bHighPriority, bSendImmediately));
}

//can spawn entities like bolts, phantoms, ammo, etc. which normally crash when using alife():create()
void spawn_section(LPCSTR sSection, Fvector3 vPosition, u32 LevelVertexID, u16 ParentID, bool bReturnItem = false)
{
	Level().spawn_item(sSection, vPosition, LevelVertexID, ParentID, bReturnItem);
}

//ability to get the target game_object at crosshair
CScriptGameObject* g_get_target_obj()
{
	collide::rq_result& RQ = HUD().GetCurrentRayQuery();
	if (RQ.O)
	{
		CGameObject* game_object = static_cast<CGameObject*>(RQ.O);
		if (game_object)
			return game_object->lua_game_object();
	}
	return (0);
}

float g_get_target_dist()
{
	collide::rq_result& RQ = HUD().GetCurrentRayQuery();
	if (RQ.range)
		return RQ.range;
	return (0);
}

u32 g_get_target_element()
{
	collide::rq_result& RQ = HUD().GetCurrentRayQuery();
	if (RQ.element)
	{
		return RQ.element;
	}
	return (0);
}

u8 get_active_cam()
{
	CActor* actor = smart_cast<CActor*>(Level().CurrentViewEntity());
	if (actor)
		return (u8)actor->active_cam();

	return 255;
}

void set_active_cam(u8 mode)
{
	CActor* actor = smart_cast<CActor*>(Level().CurrentViewEntity());
	if (actor && mode <= ACTOR_DEFS::EActorCameras::eacMaxCam)
		actor->cam_Set((ACTOR_DEFS::EActorCameras)mode);
}

void reload_hud_xml()
{
	HUD().OnScreenResolutionChanged();
}

bool actor_safemode()
{
	return Actor()->is_safemode();
}

void actor_set_safemode(bool status)
{
	if (Actor()->is_safemode() != status)
	{
		CWeapon* wep = smart_cast<CWeapon*>(Actor()->inventory().ActiveItem());
		if (wep && wep->m_bCanBeLowered)
		{
			wep->Action(kSAFEMODE, CMD_START);
			Actor()->set_safemode(status);
		}
	}
}

void prefetch_texture(LPCSTR name)
{
	Device.m_pRender->ResourcesPrefetchCreateTexture( name );
}

void prefetch_model(LPCSTR name)
{
	::Render->models_PrefetchOne(name);
}
#endif
//-Alundaio

// KD: raypick	
bool ray_pick(const Fvector& start, const Fvector& dir, float range, collide::rq_target tgt, script_rq_result& script_R,
              CScriptGameObject* ignore_object)
{
	collide::rq_result R;
	CObject* ignore = NULL;
	if (ignore_object)
		ignore = smart_cast<CObject *>(&(ignore_object->object()));
	if (Level().ObjectSpace.RayPick(start, dir, range, tgt, R, ignore))
	{
		script_R.set(R);
		return true;
	}
	else
		return false;
}

CScriptGameObject* get_view_entity_script()
{
	CGameObject* pGameObject = smart_cast<CGameObject*>(Level().CurrentViewEntity());
	if (!pGameObject)
		return (0);

	return pGameObject->lua_game_object();
}

void set_view_entity_script(CScriptGameObject* go)
{
	CObject* o = smart_cast<CObject*>(&go->object());
	if (o)
		Level().SetViewEntity(o);
}

xrTime get_start_time()
{
	return (xrTime(Level().GetStartGameTime()));
}

void iterate_nearest(const Fvector& pos, float radius, luabind::functor<bool> functor)
{
	xr_vector<CObject*> m_nearest;
	Level().ObjectSpace.GetNearest(m_nearest, pos, radius, NULL);

	if (!m_nearest.size()) return;

	xr_vector<CObject*>::iterator it = m_nearest.begin();
	xr_vector<CObject*>::iterator it_e = m_nearest.end();
	for (; it != it_e; it++)
	{
		CGameObject* obj = smart_cast<CGameObject*>(*it);
		if (!obj) continue;
		if (functor(obj->lua_game_object())) break;
	}
}

LPCSTR PickMaterial(const Fvector& start_pos, const Fvector& dir, float trace_dist, CScriptGameObject* ignore_obj)
{
	collide::rq_result result;
	BOOL reach_wall =
		Level().ObjectSpace.RayPick(
			start_pos,
			dir,
			trace_dist,
			collide::rqtStatic,
			result,
			ignore_obj ? &ignore_obj->object() : nullptr
		)
		&&
		!result.O;

	if (reach_wall)
	{
		CDB::TRI* pTri = Level().ObjectSpace.GetStaticTris() + result.element;
		SGameMtl* pMaterial = GMLib.GetMaterialByIdx(pTri->material);

		if (pMaterial)
		{
			return *pMaterial->m_Name;
		}
	}

	return "$null";
}

CScriptIniFile* GetVisualUserdata(LPCSTR visual)
{
	string_path low_name, fn;

	VERIFY(xr_strlen(visual) < sizeof(low_name));
	xr_strcpy(low_name, visual);
	strlwr(low_name);

	if (strext(low_name)) *strext(low_name) = 0;
	xr_strcat(low_name, sizeof(low_name), ".ogf");

	if (!FS.exist(low_name))
	{
		if (!FS.exist(fn, "$level$", low_name))
		{
			if (!FS.exist(fn, "$game_meshes$", low_name))
			{
				Msg("!Can't find model file '%s'.", low_name);
				return nullptr;
			}
		}
	}
	else
	{
		xr_strcpy(fn, low_name);
	}

	IReader* data = FS.r_open(fn);
	if (!data) return nullptr;

	IReader* UD = data->open_chunk(17); //OGF_S_USERDATA
	if (!UD) return nullptr;

	CScriptIniFile* ini = xr_new<CScriptIniFile>(UD, FS.get_path("$game_config$")->m_Path);
	FS.r_close(data);
	UD->close();

	return ini;
}

#pragma optimize("s",on)
void CLevel::script_register(lua_State* L)
{
	class_<CEnvDescriptor>("CEnvDescriptor")
		.def_readonly("fog_density", &CEnvDescriptor::fog_density)
		.def_readonly("far_plane", &CEnvDescriptor::far_plane),

		class_<CEnvironment>("CEnvironment")
		.def("current", current_environment);

	module(L, "level")
		[
			//Alundaio: Extend level namespace exports
#ifdef NAMESPACE_LEVEL_EXPORTS
			def("send", &g_send), //allow the ability to send netpacket to level
			def("get_target_obj", &g_get_target_obj), //intentionally named to what is in xray extensions
			def("get_target_dist", &g_get_target_dist),
			def("get_target_element", &g_get_target_element), //Can get bone cursor is targetting
			def("spawn_item", &spawn_section),
			def("get_active_cam", &get_active_cam),
			def("set_active_cam", &set_active_cam),
			def("get_start_time", &get_start_time),
			def("get_view_entity", &get_view_entity_script),
			def("set_view_entity", &set_view_entity_script),
#endif
			//Alundaio: END
			// obsolete\deprecated
			def("object_by_id", get_object_by_id),
#ifdef DEBUG
		def("debug_object",						get_object_by_name),
		def("debug_actor",						tpfGetActor),
		def("check_object",						check_object),
#endif

			def("get_weather", get_weather),
			def("set_weather", set_weather),
			def("set_weather_fx", set_weather_fx),
			def("start_weather_fx_from_time", start_weather_fx_from_time),
			def("is_wfx_playing", is_wfx_playing),
			def("get_wfx_time", get_wfx_time),
			def("stop_weather_fx", stop_weather_fx),

			def("environment", environment),

			def("set_time_factor", set_time_factor),
			def("get_time_factor", get_time_factor),

			def("set_game_difficulty", set_game_difficulty),
			def("get_game_difficulty", get_game_difficulty),

			def("get_time_days", get_time_days),
			def("get_time_hours", get_time_hours),
			def("get_time_minutes", get_time_minutes),
			def("change_game_time", change_game_time),

			def("high_cover_in_direction", high_cover_in_direction),
			def("low_cover_in_direction", low_cover_in_direction),
			def("vertex_in_direction", vertex_in_direction),
			def("rain_factor", rain_factor),
			def("patrol_path_exists", patrol_path_exists),
			def("vertex_position", vertex_position),
			def("name", get_name),
			def("prefetch_sound", prefetch_sound),

			def("client_spawn_manager", get_client_spawn_manager),

			def("map_add_object_spot_ser", map_add_object_spot_ser),
			def("map_add_object_spot", map_add_object_spot),
			//-		def("map_add_object_spot_complex",		map_add_object_spot_complex),
			def("map_remove_object_spot", map_remove_object_spot),
			def("map_has_object_spot", map_has_object_spot),
			def("map_change_spot_hint", map_change_spot_hint),

			def("add_dialog_to_render", add_dialog_to_render),
			def("remove_dialog_to_render", remove_dialog_to_render),
			def("hide_indicators", hide_indicators),
			def("hide_indicators_safe", hide_indicators_safe),

			def("show_indicators", show_indicators),
			def("show_weapon", show_weapon),
			def("add_call", ((void (*)(const luabind::functor<bool>&, const luabind::functor<void>&))&add_call)),
			def("add_call", ((void (*)(const luabind::object&, const luabind::functor<bool>&,
			                           const luabind::functor<void>&))&add_call)),
			def("add_call", ((void (*)(const luabind::object&, LPCSTR, LPCSTR))&add_call)),
			def("remove_call", ((void (*)(const luabind::functor<bool>&, const luabind::functor<void>&))&remove_call)),
			def("remove_call",
			    ((void (*)(const luabind::object&, const luabind::functor<bool>&, const luabind::functor<void>&))&
				    remove_call)),
			def("remove_call", ((void (*)(const luabind::object&, LPCSTR, LPCSTR))&remove_call)),
			def("remove_calls_for_object", remove_calls_for_object),
			def("present", is_level_present),
			def("disable_input", disable_input),
			def("enable_input", enable_input),
			def("spawn_phantom", spawn_phantom),

			def("get_bounding_volume", get_bounding_volume),

			def("iterate_sounds", &iterate_sounds1),
			def("iterate_sounds", &iterate_sounds2),
			def("physics_world", &physics_world_scripted),
			def("get_snd_volume", &get_snd_volume),
			def("get_rain_volume", &get_rain_volume),
			def("set_snd_volume", &set_snd_volume),
			def("add_cam_effector", ((float (*)(LPCSTR, int, bool, LPCSTR))&add_cam_effector)),
			def("add_cam_effector", ((float (*)(LPCSTR, int, bool, LPCSTR, float))&add_cam_effector)),
			def("add_cam_effector", ((float (*)(LPCSTR, int, bool, LPCSTR, float, bool))&add_cam_effector)),
			def("add_cam_effector", ((float (*)(LPCSTR, int, bool, LPCSTR, float, bool, float))&add_cam_effector)),
			def("remove_cam_effector", &remove_cam_effector),
			def("set_cam_effector_factor", &set_cam_effector_factor),
			def("get_cam_effector_factor", &get_cam_effector_factor),
			def("get_cam_effector_length", &get_cam_effector_length),
			def("check_cam_effector", &check_cam_effector),
			def("add_pp_effector", &add_pp_effector),
			def("set_pp_effector_factor", &set_pp_effector_factor),
			def("set_pp_effector_factor", &set_pp_effector_factor2),
			def("remove_pp_effector", &remove_pp_effector),

			def("add_complex_effector", &add_complex_effector),
			def("remove_complex_effector", &remove_complex_effector),

			def("vertex_id", &vertex_id),

			def("game_id", &GameID),
			def("ray_pick", &ray_pick),

			def("press_action", &LevelPressAction),
			def("release_action", &LevelReleaseAction),
			def("hold_action", &LevelHoldAction),

			def("actor_moving_state", &ActorMovingState),
			def("get_env_rads", &get_env_rads),
			def("iterate_nearest", &iterate_nearest),
			def("pick_material", &PickMaterial)
		],

		module(L, "actor_stats")
		[
			def("add_points", &add_actor_points),
			def("add_points_str", &add_actor_points_str),
			def("get_points", &get_actor_points)
		];
	module(L)
	[
		class_<CRayPick>("ray_pick")
		.def(constructor<>())
		.def(constructor<Fvector&, Fvector&, float, collide::rq_target, CScriptGameObject*>())
		.def("set_position", &CRayPick::set_position)
		.def("set_direction", &CRayPick::set_direction)
		.def("set_range", &CRayPick::set_range)
		.def("set_flags", &CRayPick::set_flags)
		.def("set_ignore_object", &CRayPick::set_ignore_object)
		.def("query", &CRayPick::query)
		.def("get_result", &CRayPick::get_result)
		.def("get_object", &CRayPick::get_object)
		.def("get_distance", &CRayPick::get_distance)
		.def("get_element", &CRayPick::get_element),
		class_<script_rq_result>("rq_result")
		.def_readonly("object", &script_rq_result::O)
		.def_readonly("range", &script_rq_result::range)
		.def_readonly("element", &script_rq_result::element)
		.def_readonly("material_name", &script_rq_result::pMaterialName)
		.def_readonly("material_flags", &script_rq_result::pMaterialFlags)
		.def_readonly("material_phfriction", &script_rq_result::fPHFriction)
		.def_readonly("material_phdamping", &script_rq_result::fPHDamping)
		.def_readonly("material_phspring", &script_rq_result::fPHSpring)
		.def_readonly("material_phbounce_start_velocity", &script_rq_result::fPHBounceStartVelocity)
		.def_readonly("material_phbouncing", &script_rq_result::fPHBouncing)
		.def_readonly("material_flotation_factor", &script_rq_result::fFlotationFactor)
		.def_readonly("material_shoot_factor", &script_rq_result::fShootFactor)
		.def_readonly("material_shoot_factor_mp", &script_rq_result::fShootFactorMP)
		.def_readonly("material_bounce_damage_factor", &script_rq_result::fBounceDamageFactor)
		.def_readonly("material_injurious_speed", &script_rq_result::fInjuriousSpeed)
		.def_readonly("material_vis_transparency_factor", &script_rq_result::fVisTransparencyFactor)
		.def_readonly("material_snd_occlusion_factor", &script_rq_result::fSndOcclusionFactor)
		.def_readonly("material_density_factor", &script_rq_result::fDensityFactor)
		.def(constructor<>()),
		class_<enum_exporter<collide::rq_target>>("rq_target")
		.enum_("targets")
		[
			value("rqtNone", int(collide::rqtNone)),
			value("rqtObject", int(collide::rqtObject)),
			value("rqtStatic", int(collide::rqtStatic)),
			value("rqtShape", int(collide::rqtShape)),
			value("rqtObstacle", int(collide::rqtObstacle)),
			value("rqtBoth", int(collide::rqtBoth)),
			value("rqtDyn", int(collide::rqtDyn))
		]
	];

	module(L)
	[
		def("command_line", &command_line),
		def("IsGameTypeSingle", &IsGameTypeSingle),
		def("IsDynamicMusic", &IsDynamicMusic),
		def("render_get_dx_level", &render_get_dx_level),
		def("IsImportantSave", &IsImportantSave)
	];

	module(L, "weather")
	[
		def("get_value_numric", get_weather_value_numric),
		def("get_value_vector", get_weather_value_vector),
		def("get_value_string", get_weather_value_string),
		def("pause", pause_weather),
		def("is_paused",is_weather_paused),
		def("set_value_numric", set_weather_value_numric),
		def("set_value_vector", set_weather_value_vector),
		def("set_value_string", set_weather_value_string),
		def("reload", reload_weather),
		def("boost_value", boost_weather_value),
		def("boost_reset", boost_weather_reset),
		def("sun_time", sun_time)
	];

	module(L, "hud_adjust")
		[
		def("enabled", hud_adj_state),
		def("set_vector", hud_adj_offs),
		def("set_value", hud_adj_value),
		def("remove_hud_model", remove_hud_model)
	];

	module(L, "relation_registry")
	[
		def("community_goodwill", &g_community_goodwill),
		def("set_community_goodwill", &g_set_community_goodwill),
		def("change_community_goodwill", &g_change_community_goodwill),

		def("community_relation", &g_get_community_relation),
		def("set_community_relation", &g_set_community_relation),
		def("get_general_goodwill_between", &g_get_general_goodwill_between)
	];
	module(L, "game")
	[
		class_<xrTime>("CTime")
		.enum_("date_format")
		[
			value("DateToDay", int(InventoryUtilities::edpDateToDay)),
			value("DateToMonth", int(InventoryUtilities::edpDateToMonth)),
			value("DateToYear", int(InventoryUtilities::edpDateToYear))
		]
		.enum_("time_format")
		[
			value("TimeToHours", int(InventoryUtilities::etpTimeToHours)),
			value("TimeToMinutes", int(InventoryUtilities::etpTimeToMinutes)),
			value("TimeToSeconds", int(InventoryUtilities::etpTimeToSeconds)),
			value("TimeToMilisecs", int(InventoryUtilities::etpTimeToMilisecs))
		]
		.def(constructor<>())
		.def(constructor<const xrTime&>())
		.def(const_self < xrTime())
		.def(const_self <= xrTime())
		.def(const_self > xrTime())
		.def(const_self >= xrTime())
		.def(const_self == xrTime())
		.def(self + xrTime())
		.def(self - xrTime())

		.def("diffSec", &xrTime::diffSec_script)
		.def("add", &xrTime::add_script)
		.def("sub", &xrTime::sub_script)

		.def("setHMS", &xrTime::setHMS)
		.def("setHMSms", &xrTime::setHMSms)
		.def("set", &xrTime::set)
		.def("get", &xrTime::get,
		     out_value(_2) + out_value(_3) + out_value(_4) + out_value(_5) + out_value(_6) + out_value(_7) +
		     out_value(_8))
		.def("dateToString", &xrTime::dateToString)
		.def("timeToString", &xrTime::timeToString),
		// declarations
		def("time", get_time),
		def("get_game_time", get_time_struct),
		//		def("get_surge_time",	Game::get_surge_time),
		//		def("get_object_by_name",Game::get_object_by_name),

		def("start_tutorial", &start_tutorial),
		def("stop_tutorial", &stop_tutorial),
		def("has_active_tutorial", &has_active_tutotial),
		def("translate_string", &translate_string),
		def("reload_language", &reload_language),
		def("get_resolutions", &vid_modes_string),
		def("play_hud_motion", PlayHudMotion),
		def("stop_hud_motion", StopHudMotion),
		def("get_motion_length", MotionLength),
		def("hud_motion_allowed", AllowHudMotion),
		def("play_hud_anm", PlayBlendAnm),
		def("stop_hud_anm", StopBlendAnm),
		def("stop_all_hud_anms", StopAllBlendAnms),
		def("set_hud_anm_time", SetBlendAnmTime),
		def("only_allow_movekeys", block_all_except_movement),
		def("only_movekeys_allowed", only_movement_allowed),
		def("set_actor_allow_ladder", set_actor_allow_ladder),
		def("set_nv_lumfactor", set_nv_lumfactor),
		def("reload_ui_xml", reload_hud_xml),
		def("actor_weapon_lowered", actor_safemode),
		def("actor_lower_weapon", actor_set_safemode),
		def("prefetch_texture", prefetch_texture),
		def("prefetch_model", prefetch_model),
		def("get_visual_userdata", GetVisualUserdata),
		def("world2ui", world2ui)
	];
}