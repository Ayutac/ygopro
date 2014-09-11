/*
 * effect.cpp
 *
 *  Created on: 2010-5-7
 *      Author: Argon
 */

#include "effect.h"
#include "card.h"
#include "duel.h"
#include "group.h"
#include "interpreter.h"

/*
 * Compares two effects by their ID.
 * Returns e1->id < e2->id.
 */
bool effect_sort_id(const effect* e1, const effect* e2) {
	return e1->id < e2->id;
};

/*
 * Constructor of the effect class. Initializes effects.
 * This is what will be added by duel::new_effect()
 */
effect::effect() {
	scrtype = 3;
	ref_handle = 0;
	owner = 0;
	handler = 0;
	description = 0;
	effect_owner = PLAYER_NONE;
	card_type = 0;
	active_type = 0;
	id = 0;
	code = 0;
	type = 0;
	flag = 0;
	copy_id = 0;
	range = 0;
	s_range = 0;
	o_range = 0;
	reset_count = 0;
	reset_flag = 0;
	count_code = 0;
	category = 0;
	label = 0;
	label_object = 0;
	hint_timing[0] = 0;
	hint_timing[1] = 0;
	field_ref = 0;
	status = 0;
	condition = 0;
	cost = 0;
	target = 0;
	value = 0;
	operation = 0;
}


/*
 * Destructor, is empty by now.
 */
effect::~effect() {

}

/*
 * Returns TRUE if the effect is disable-related
 */
int32 effect::is_disable_related() {
	if (code == EFFECT_IMMUNE_EFFECT || code == EFFECT_DISABLE || code == EFFECT_CANNOT_DISABLE)
		return TRUE;
	return FALSE;
}

/*
 * Returns TRUE if this effect is available for activation right now, else FALSE.
 */
int32 effect::is_available() {
	// for an effect to be available,
	// ???,
	if (type & EFFECT_TYPE_ACTIONS)
		return FALSE;
	
	if (type & EFFECT_TYPE_SINGLE) {
		// it must belong to some player,
		if (handler->current.controler == PLAYER_NONE)
			return FALSE;
		// if single range, card must be in right location
		if((flag & EFFECT_FLAG_SINGLE_RANGE) && !(range & handler->current.location))
			return FALSE;
		//   and better face-up or otherwise enabled,
		if((flag & EFFECT_FLAG_SINGLE_RANGE) && (handler->current.location & LOCATION_ONFIELD)
		        && (handler->is_position(POS_FACEDOWN) || !handler->is_status(STATUS_EFFECT_ENABLED)))
			return FALSE;
		// if the effect is dependent on the owner (?), it should be enabled or unable to be disabled,
		if((flag & EFFECT_FLAG_OWNER_RELATE) && !(flag & EFFECT_FLAG_CANNOT_DISABLE) && owner->is_status(STATUS_DISABLED))
			return FALSE;
		// (basically the same, just this time check if the owner is the handler)
		if(owner == handler && !(flag & EFFECT_FLAG_CANNOT_DISABLE) && handler->get_status(STATUS_DISABLED))
			return FALSE;
	}
	if (type & EFFECT_TYPE_EQUIP) {
		// it (still) must belong to some trainer,
		if(handler->current.controler == PLAYER_NONE)
			return FALSE;
		// (we know this one too)
		if((flag & EFFECT_FLAG_OWNER_RELATE) && !(flag & EFFECT_FLAG_CANNOT_DISABLE) && owner->is_status(STATUS_DISABLED))
			return FALSE;
		// (this one as well)
		if(owner == handler && !(flag & EFFECT_FLAG_CANNOT_DISABLE) && handler->get_status(STATUS_DISABLED))
			return FALSE;
		// if ???, handler must have effect enabled and be face-up,
		if(!(flag & EFFECT_FLAG_SET_AVAILABLE)) {
			if(!(handler->get_status(STATUS_EFFECT_ENABLED)))
				return FALSE;
			if(!handler->is_position(POS_FACEUP))
				return FALSE;
		}
	}
	if (type & EFFECT_TYPE_FIELD) { // we know most of this by now.
		if (!(flag & EFFECT_FLAG_FIELD_ONLY)) {
			if(handler->current.controler == PLAYER_NONE)
				return FALSE;
			if((flag & EFFECT_FLAG_OWNER_RELATE) && !(flag & EFFECT_FLAG_CANNOT_DISABLE) && owner->is_status(STATUS_DISABLED))
				return FALSE;
			if(owner == handler && !(flag & EFFECT_FLAG_CANNOT_DISABLE) && handler->get_status(STATUS_DISABLED))
				return FALSE;
			// if the handler was destroyed by battle, then it is available if specified so,
			if(handler->is_status(STATUS_BATTLE_DESTROYED) && !(flag & EFFECT_FLAG_AVAILABLE_BD))
				return FALSE;
			if(!(handler->get_status(STATUS_EFFECT_ENABLED)))
				return FALSE;
			if(!(range & handler->current.location))
				return FALSE;
			if((handler->current.location & LOCATION_ONFIELD) && !handler->is_position(POS_FACEUP))
				return FALSE;
		}
	}
	if (!condition) // if unconditional, effect is available
		return TRUE;
	// else check conditions out
	pduel->lua->add_param(this, PARAM_TYPE_EFFECT); 
	int32 res = pduel->lua->check_condition(condition, 1);
	if(res) { // if condition fulfilled
		if(!(status & EFFECT_STATUS_AVAILABLE)) //.. and if that's new
			id = pduel->game_field->infos.field_id++; //.... make the effect available in field
		status |= EFFECT_STATUS_AVAILABLE; //.. and on status 
	} else
		status &= ~EFFECT_STATUS_AVAILABLE; // else remove availability
	return res;
}

/*
 * Returns TRUE if this effect can be activated again by a certain player.
 */
int32 effect::check_count_limit(uint8 playerid) {
	if((flag & EFFECT_FLAG_COUNT_LIMIT)) {
		if(count_code == 0) { //???
			if((reset_count & 0xf00) == 0) // RESET_SELF_TURN|RESET_OPPO_TURN|RESET_PHASE|RESET_CHAIN I guess
				return FALSE;
		} else { // ?? I guess the number of activations is somehow coded into reset_count?
			uint32 code = count_code & 0xfffffff;
			uint32 count = (reset_count >> 12) & 0xf;
			if(code == 1) {
				if(pduel->game_field->get_effect_code((count_code & 0xf0000000) | handler->fieldid, PLAYER_NONE) >= count)
					return FALSE;
			} else {
				if(pduel->game_field->get_effect_code(count_code, playerid) >= count)
					return FALSE;
			}
		}
	}
	return TRUE; // if no count limit, then of course it can.
}

/*
 * Returns TRUE if this effect can be activated (by a player).
 */
int32 effect::is_activateable(uint8 playerid, const tevent& e, int32 neglect_cond, int32 neglect_cost, int32 neglect_target) {
	// the effect can be activated
	// if it isn't activated by certain other circumstances,
	if(!(type & EFFECT_TYPE_ACTIONS))
		return FALSE;
	// if the player is still allowed to activate that effect,
	if(!check_count_limit(playerid))
		return FALSE;
	// if the effect can only effect the field / be activated from the field (???)
	if (!(flag & EFFECT_FLAG_FIELD_ONLY)) {
		//.. and is to be activated
		if (type & EFFECT_TYPE_ACTIVATE) {
			//.... its handler must be controlled by the player who wants to activate it
			if(handler->current.controler != playerid)
				return FALSE;
			//.... its handler can only be field one time if it's unique
			if(pduel->game_field->check_unique_onfield(handler, playerid))
				return FALSE;
			//.... in case of a counter card
			if(!(handler->data.type & TYPE_COUNTER)) {
				// ????
				if((code < 1132 || code > 1149) && pduel->game_field->infos.phase == PHASE_DAMAGE && !(flag & EFFECT_FLAG_DAMAGE_STEP))
					return FALSE;
				if((code < 1134 || code > 1136) && pduel->game_field->infos.phase == PHASE_DAMAGE_CAL && !(flag & EFFECT_FLAG_DAMAGE_CAL))
					return FALSE;
			}
			//.... if the handler's in the hand
			if(handler->current.location == LOCATION_HAND) {
				// it can't be a trap card unless trap cards can be currently activated from the hand,
				if((handler->data.type & TYPE_TRAP) && !handler->is_affected_by_effect(EFFECT_TRAP_ACT_IN_HAND))
					return FALSE;
				// and if it's a spell it must be the player's turn,
				if((handler->data.type & TYPE_SPELL) && (pduel->game_field->infos.turn_player != playerid)) {
					// of course unless it's a quick spell and quick spells can be activated from the hand during the opponent's turn,
					if(!(handler->data.type & TYPE_QUICKPLAY) || !handler->is_affected_by_effect(EFFECT_QP_ACT_IN_NTPHAND))
						return FALSE;
				}
				// if it is a monster
				if(handler->data.type & TYPE_MONSTER) {
					// it has to be a pendulum monster
					if(!(handler->data.type & TYPE_PENDULUM))
						return FALSE;
					// and there has to be at least one free pendulum zone,
					if(pduel->game_field->player[playerid].list_szone[6] && pduel->game_field->player[playerid].list_szone[7])
						return FALSE;
				// if it is not a monster there must be space to activate it,
				} else if(!(handler->data.type & TYPE_FIELD) && pduel->game_field->get_useable_count(playerid, LOCATION_SZONE, playerid, LOCATION_REASON_TOFIELD) <= 0)
					return FALSE;
			//.... if it's in the spell and trap card zone
			} else if(handler->current.location == LOCATION_SZONE) {
				// it has to be face down
				if(handler->is_position(POS_FACEUP))
					return FALSE;
				// and if it was just set
				if(handler->get_status(STATUS_SET_TURN)) {
					// in case of a spell it can't be a quick spell
					if((handler->data.type & TYPE_SPELL) && (handler->data.type & TYPE_QUICKPLAY))
						return FALSE;
					// and in case of a trap
					if((handler->data.type & TYPE_TRAP) && !handler->is_affected_by_effect(EFFECT_TRAP_ACT_IN_SET_TURN))
						return FALSE;
				}
			}
			//.... it must be allowed to be activated
			if(handler->is_affected_by_effect(EFFECT_FORBIDDEN))
				return FALSE;
			if(handler->is_affected_by_effect(EFFECT_CANNOT_TRIGGER))
				return FALSE;
		//.. and the type isn't continous
		} else if(!(type & EFFECT_TYPE_CONTINUOUS)) { // from if (type & EFFECT_TYPE_ACTIVATE)
			//.... if the card is on the field or removed, not a flip and face-down, it must have it's effect actively enabled
			if((handler->current.location & (LOCATION_ONFIELD | LOCATION_REMOVED))
			        && (code != EVENT_FLIP) && (!handler->is_position(POS_FACEUP) || !handler->is_status(STATUS_EFFECT_ENABLED)))
				return FALSE;
			//.... if the effect is neither a flip nor a trigger_f (?)
			if(!(type & (EFFECT_TYPE_FLIP | EFFECT_TYPE_TRIGGER_F))) {
				// ????
				if((code < 1132 || code > 1149) && pduel->game_field->infos.phase == PHASE_DAMAGE && !(flag & EFFECT_FLAG_DAMAGE_STEP))
					return FALSE;
				if((code < 1134 || code > 1136) && pduel->game_field->infos.phase == PHASE_DAMAGE_CAL && !(flag & EFFECT_FLAG_DAMAGE_CAL))
					return FALSE;
			}
			//.... if the effect is of the field type, it must have the flag set to enable both players to use the effect or the player must be current controller 
			if((type & EFFECT_TYPE_FIELD) && (handler->current.controler != playerid) && !(flag & EFFECT_FLAG_BOTH_SIDE))
				return FALSE;
			//.... it must be allowed to be activated
			if(handler->is_affected_by_effect(EFFECT_FORBIDDEN))
				return FALSE;
			if(handler->is_affected_by_effect(EFFECT_CANNOT_TRIGGER))
				return FALSE;
		//.. and if the type is continous
		} else { // from if !(type & EFFECT_TYPE_CONTINUOUS)
			// ???
			if(!(flag & EFFECT_FLAG_AVAILABLE_BD) && (type & EFFECT_TYPE_FIELD) && handler->is_status(STATUS_BATTLE_DESTROYED))
				return FALSE;
			// to normally activate the effect, the card must be specifically enabled or be face-up
			if(((type & EFFECT_TYPE_FIELD) || ((type & EFFECT_TYPE_SINGLE) && (flag & EFFECT_FLAG_SINGLE_RANGE))) && (handler->current.location & LOCATION_ONFIELD)
			        && (!handler->is_position(POS_FACEUP) || !handler->is_status(STATUS_EFFECT_ENABLED)))
				return FALSE;
			// the card must lie where its effect can be activated ?
			if((type & EFFECT_TYPE_SINGLE) && (flag & EFFECT_FLAG_SINGLE_RANGE) && !(handler->current.location & range))
				return FALSE;
			// ???
			if((flag & EFFECT_FLAG_OWNER_RELATE) && !(flag & EFFECT_FLAG_CANNOT_DISABLE) && owner->is_status(STATUS_DISABLED))
				return FALSE;
			if((handler == owner) && !(flag & EFFECT_FLAG_CANNOT_DISABLE) && handler->is_status(STATUS_DISABLED))
				return FALSE;
		}
	} else { // from if !(flag & EFFECT_FLAG_FIELD_ONLY)
		// if the owner isn't the player, the effect must work for both (?)
		if((get_owner_player() != playerid) && !(flag & EFFECT_FLAG_BOTH_SIDE))
			return FALSE;
	}
	pduel->game_field->save_lp_cost();
	effect* oreason = pduel->game_field->core.reason_effect;
	uint8 op = pduel->game_field->core.reason_player;
	pduel->game_field->core.reason_effect = this;
	pduel->game_field->core.reason_player = playerid;
	int32 result = TRUE;
	if(!(type & EFFECT_TYPE_CONTINUOUS) )
		result = is_action_check(playerid);
	if(result)
		result = is_activate_ready(playerid, e, neglect_cond, neglect_cost, neglect_target);
	pduel->game_field->core.reason_effect = oreason;
	pduel->game_field->core.reason_player = op;
	pduel->game_field->restore_lp_cost();
	return result;
}

int32 effect::is_action_check(uint8 playerid) {
	effect_set eset;
	pduel->game_field->filter_player_effect(playerid, EFFECT_CANNOT_ACTIVATE, &eset);
	for(int i = 0; i < eset.count; ++i) {
		pduel->lua->add_param(this, PARAM_TYPE_EFFECT);
		pduel->lua->add_param(playerid, PARAM_TYPE_INT);
		if(eset[i]->check_value_condition(2))
			return FALSE;
	}
	eset.clear();
	pduel->game_field->filter_player_effect(playerid, EFFECT_ACTIVATE_COST, &eset);
	for(int i = 0; i < eset.count; ++i) {
		pduel->lua->add_param(eset[i], PARAM_TYPE_EFFECT);
		pduel->lua->add_param(this, PARAM_TYPE_EFFECT);
		pduel->lua->add_param(playerid, PARAM_TYPE_INT);
		if(!pduel->lua->check_condition(eset[i]->target, 3))
			continue;
		pduel->lua->add_param(eset[i], PARAM_TYPE_EFFECT);
		pduel->lua->add_param(this, PARAM_TYPE_EFFECT);
		pduel->lua->add_param(playerid, PARAM_TYPE_INT);
		if(!pduel->lua->check_condition(eset[i]->cost, 3))
			return FALSE;
	}
	return TRUE;
}
int32 effect::is_activate_ready(uint8 playerid, const tevent& e, int32 neglect_cond, int32 neglect_cost, int32 neglect_target) {
	if (!neglect_cond && condition) {
		pduel->lua->add_param(this, PARAM_TYPE_EFFECT);
		pduel->lua->add_param(playerid, PARAM_TYPE_INT);
		pduel->lua->add_param(e.event_cards , PARAM_TYPE_GROUP);
		pduel->lua->add_param(e.event_player, PARAM_TYPE_INT);
		pduel->lua->add_param(e.event_value, PARAM_TYPE_INT);
		pduel->lua->add_param(e.reason_effect , PARAM_TYPE_EFFECT);
		pduel->lua->add_param(e.reason, PARAM_TYPE_INT);
		pduel->lua->add_param(e.reason_player, PARAM_TYPE_INT);
		if (!pduel->lua->check_condition(condition, 8)) {
			return FALSE;
		}
	}
	if(!neglect_cost && cost && !(type & EFFECT_TYPE_CONTINUOUS)) {
		pduel->lua->add_param(this, PARAM_TYPE_EFFECT);
		pduel->lua->add_param(playerid, PARAM_TYPE_INT);
		pduel->lua->add_param(e.event_cards , PARAM_TYPE_GROUP);
		pduel->lua->add_param(e.event_player, PARAM_TYPE_INT);
		pduel->lua->add_param(e.event_value, PARAM_TYPE_INT);
		pduel->lua->add_param(e.reason_effect , PARAM_TYPE_EFFECT);
		pduel->lua->add_param(e.reason, PARAM_TYPE_INT);
		pduel->lua->add_param(e.reason_player, PARAM_TYPE_INT);
		pduel->lua->add_param((ptr)0, PARAM_TYPE_INT);
		if (!pduel->lua->check_condition(cost, 9)) {
			return FALSE;
		}
	}
	if(!neglect_target && target) {
		pduel->lua->add_param(this, PARAM_TYPE_EFFECT);
		pduel->lua->add_param(playerid, PARAM_TYPE_INT);
		pduel->lua->add_param(e.event_cards , PARAM_TYPE_GROUP);
		pduel->lua->add_param(e.event_player, PARAM_TYPE_INT);
		pduel->lua->add_param(e.event_value, PARAM_TYPE_INT);
		pduel->lua->add_param(e.reason_effect , PARAM_TYPE_EFFECT);
		pduel->lua->add_param(e.reason, PARAM_TYPE_INT);
		pduel->lua->add_param(e.reason_player, PARAM_TYPE_INT);
		pduel->lua->add_param((ptr)0, PARAM_TYPE_INT);
		if (!pduel->lua->check_condition(target, 9)) {
			return FALSE;
		}
	}
	return TRUE;
}
int32 effect::is_condition_check(uint8 playerid, const tevent& e) {
	if(!(type & EFFECT_TYPE_ACTIVATE) && (handler->current.location & (LOCATION_ONFIELD | LOCATION_REMOVED)) && (!handler->is_position(POS_FACEUP)))
		return FALSE;
	if(!condition)
		return TRUE;
	effect* oreason = pduel->game_field->core.reason_effect;
	uint8 op = pduel->game_field->core.reason_player;
	pduel->game_field->core.reason_effect = this;
	pduel->game_field->core.reason_player = playerid;
	pduel->game_field->save_lp_cost();
	pduel->lua->add_param(this, PARAM_TYPE_EFFECT);
	pduel->lua->add_param(playerid, PARAM_TYPE_INT);
	pduel->lua->add_param(e.event_cards , PARAM_TYPE_GROUP);
	pduel->lua->add_param(e.event_player, PARAM_TYPE_INT);
	pduel->lua->add_param(e.event_value, PARAM_TYPE_INT);
	pduel->lua->add_param(e.reason_effect , PARAM_TYPE_EFFECT);
	pduel->lua->add_param(e.reason, PARAM_TYPE_INT);
	pduel->lua->add_param(e.reason_player, PARAM_TYPE_INT);
	if(!pduel->lua->check_condition(condition, 8)) {
		pduel->game_field->restore_lp_cost();
		pduel->game_field->core.reason_effect = oreason;
		pduel->game_field->core.reason_player = op;
		return FALSE;
	}
	pduel->game_field->restore_lp_cost();
	pduel->game_field->core.reason_effect = oreason;
	pduel->game_field->core.reason_player = op;
	return TRUE;
}
int32 effect::is_activate_check(uint8 playerid, const tevent& e, int32 neglect_cond, int32 neglect_cost, int32 neglect_target) {
	pduel->game_field->save_lp_cost();
	effect* oreason = pduel->game_field->core.reason_effect;
	uint8 op = pduel->game_field->core.reason_player;
	pduel->game_field->core.reason_effect = this;
	pduel->game_field->core.reason_player = playerid;
	int32 result = TRUE;
	result = is_activate_ready(playerid, e, neglect_cond, neglect_cost, neglect_target);
	pduel->game_field->core.reason_effect = oreason;
	pduel->game_field->core.reason_player = op;
	pduel->game_field->restore_lp_cost();
	return result;
}
int32 effect::is_target(card* pcard) {
	if(type & EFFECT_TYPE_ACTIONS)
		return FALSE;
	if((type & EFFECT_TYPE_SINGLE) || (type & EFFECT_TYPE_EQUIP))
		return TRUE;
	if(pcard && !(flag & EFFECT_FLAG_SET_AVAILABLE) && (pcard->current.location & LOCATION_ONFIELD) && !pcard->is_position(POS_FACEUP))
		return FALSE;
	if(!(flag & EFFECT_FLAG_IGNORE_RANGE)) {
		if(pcard->get_status(STATUS_SUMMONING + STATUS_SUMMON_DISABLED))
			return FALSE;
		if(flag & EFFECT_FLAG_ABSOLUTE_TARGET) {
			if(pcard->current.controler == 0) {
				if (!(s_range & pcard->current.location))
					return FALSE;
			} else {
				if(!(o_range & pcard->current.location))
					return FALSE;
			}
		} else {
			if(pcard->current.controler == get_handler_player()) {
				if(!(s_range & pcard->current.location))
					return FALSE;
			} else {
				if(!(o_range & pcard->current.location))
					return FALSE;
			}
		}
	}
	if(target) {
		pduel->lua->add_param(this, PARAM_TYPE_EFFECT);
		pduel->lua->add_param(pcard, PARAM_TYPE_CARD);
		if (!pduel->lua->check_condition(target, 2)) {
			return FALSE;
		}
	}
	return TRUE;
}
int32 effect::is_target_player(uint8 playerid) {
	if(!(flag & EFFECT_FLAG_PLAYER_TARGET))
		return FALSE;
	uint8 self = get_handler_player();
	if(flag & EFFECT_FLAG_ABSOLUTE_TARGET) {
		if(s_range && playerid == 0 )
			return TRUE;
		if(o_range && playerid == 1 )
			return TRUE;
	} else {
		if(s_range && self == playerid)
			return TRUE;
		if(o_range && self != playerid)
			return TRUE;
	}
	return FALSE;
}
int32 effect::is_player_effect_target(card* pcard) {
	if(target) {
		handler->pduel->lua->add_param(this, PARAM_TYPE_EFFECT);
		handler->pduel->lua->add_param(pcard, PARAM_TYPE_CARD);
		if(!handler->pduel->lua->check_condition(target, 2)) {
			return FALSE;
		}
	}
	return TRUE;
}
int32 effect::is_immuned(effect_set_v* effects) {
	effect* peffect;
	for (int i = 0; i < effects->count; ++i) {
		peffect = effects->at(i);
		if(type & 0x7f0) {
			if(peffect->owner == handler)
				return FALSE;
		} else {
			if(peffect->owner == owner)
				return FALSE;
		}
		if(peffect->value) {
			pduel->lua->add_param(this, PARAM_TYPE_EFFECT);
			if(peffect->check_value_condition(1))
				return TRUE;
		}
	}
	return FALSE;
}
int32 effect::is_chainable(uint8 tp) {
	if(!(type & EFFECT_TYPE_ACTIONS))
		return FALSE;
	int32 sp = get_speed();
	if((type & EFFECT_TYPE_ACTIVATE) && (sp <= 1) && !(flag & EFFECT_FLAG_COF))
		return FALSE;
	if(pduel->game_field->core.current_chain.size()) {
		if(!(flag & EFFECT_FLAG_FIELD_ONLY) && (type & EFFECT_TYPE_TRIGGER_O) && (handler->current.location == LOCATION_HAND)) {
			if(pduel->game_field->core.current_chain.rbegin()->triggering_effect->get_speed() > 2)
				return FALSE;
		} else if(sp < pduel->game_field->core.current_chain.rbegin()->triggering_effect->get_speed())
			return FALSE;
	}
	if(pduel->game_field->core.chain_limit) {
		pduel->lua->add_param(this, PARAM_TYPE_EFFECT);
		pduel->lua->add_param(pduel->game_field->core.chain_limp, PARAM_TYPE_INT);
		pduel->lua->add_param(tp, PARAM_TYPE_INT);
		if(!pduel->lua->check_condition(pduel->game_field->core.chain_limit, 3))
			return FALSE;
	}
	if(pduel->game_field->core.chain_limit_p) {
		pduel->lua->add_param(this, PARAM_TYPE_EFFECT);
		pduel->lua->add_param(pduel->game_field->core.chain_limp_p, PARAM_TYPE_INT);
		pduel->lua->add_param(tp, PARAM_TYPE_INT);
		if(!pduel->lua->check_condition(pduel->game_field->core.chain_limit_p, 3))
			return FALSE;
	}
	return TRUE;
}
int32 effect::reset(uint32 reset_level, uint32 reset_type) {
	switch (reset_type) {
	case RESET_EVENT: {
		if(!(reset_flag & RESET_EVENT))
			return FALSE;
		if(owner != handler)
			reset_level &= ~RESET_DISABLE;
		if(reset_level & 0xffff0000 & reset_flag)
			return TRUE;
		return FALSE;
		break;
	}
	case RESET_CARD: {
		return owner && (owner->data.code == reset_level);
		break;
	}
	case RESET_PHASE: {
		if(!(reset_flag & RESET_PHASE))
			return FALSE;
		uint8 pid = get_handler_player();
		uint8 tp = handler->pduel->game_field->infos.turn_player;
		if((((reset_flag & RESET_SELF_TURN) && pid == tp) || ((reset_flag & RESET_OPPO_TURN) && pid != tp)) && (reset_level & 0xff & reset_flag))
			reset_count--;
		if((reset_count & 0xff) == 0)
			return TRUE;
		return FALSE;
		break;
	}
	case RESET_CODE: {
		return (code == reset_level) && (type & EFFECT_TYPE_SINGLE) && !(type & EFFECT_TYPE_ACTIONS);
		break;
	}
	case RESET_COPY: {
		return copy_id == reset_level;
		break;
	}
	}
	return FALSE;
}
void effect::dec_count(uint32 playerid) {
	if(!(flag & EFFECT_FLAG_COUNT_LIMIT))
		return;
	if(count_code == 0) {
		if((reset_count & 0xf00) == 0)
			return;
		reset_count -= 0x100;
	} else {
		uint32 code = count_code & 0xfffffff;
		if(code == 1)
			pduel->game_field->add_effect_code((count_code & 0xf0000000) | handler->fieldid, PLAYER_NONE);
		else
			pduel->game_field->add_effect_code(count_code, playerid);
	}
}
void effect::recharge() {
	if((flag & EFFECT_FLAG_COUNT_LIMIT) && (count_code == 0)) {
		reset_count &= 0xf0ff;
		reset_count |= (reset_count >> 4) & 0xf00;
	}
}
int32 effect::get_value(uint32 extraargs) {
	if(flag & EFFECT_FLAG_FUNC_VALUE) {
		pduel->lua->add_param(this, PARAM_TYPE_EFFECT, TRUE);
		int32 res = pduel->lua->get_function_value(value, 1 + extraargs);
		return res;
	} else {
		pduel->lua->params.clear();
		return (int32)value;
	}
}
int32 effect::get_value(card* pcard, uint32 extraargs) {
	if(flag & EFFECT_FLAG_FUNC_VALUE) {
		pduel->lua->add_param(pcard, PARAM_TYPE_CARD, TRUE);
		pduel->lua->add_param(this, PARAM_TYPE_EFFECT, TRUE);
		int32 res = pduel->lua->get_function_value(value, 2 + extraargs);
		return res;
	} else {
		pduel->lua->params.clear();
		return (int32)value;
	}
}
int32 effect::get_value(effect* peffect, uint32 extraargs) {
	if(flag & EFFECT_FLAG_FUNC_VALUE) {
		pduel->lua->add_param(peffect, PARAM_TYPE_EFFECT, TRUE);
		pduel->lua->add_param(this, PARAM_TYPE_EFFECT, TRUE);
		int32 res = pduel->lua->get_function_value(value, 2 + extraargs);
		return res;
	} else {
		pduel->lua->params.clear();
		return (int32)value;
	}
}
int32 effect::check_value_condition(uint32 extraargs) {
	if(flag & EFFECT_FLAG_FUNC_VALUE) {
		pduel->lua->add_param(this, PARAM_TYPE_EFFECT, TRUE);
		int32 res = pduel->lua->check_condition(value, 1 + extraargs);
		return res;
	} else {
		pduel->lua->params.clear();
		return (int32)value;
	}
}
int32 effect::get_speed() {
	if(!(type & EFFECT_TYPE_ACTIONS))
		return 0;
	if(type & (EFFECT_TYPE_TRIGGER_O | EFFECT_TYPE_TRIGGER_F | EFFECT_TYPE_IGNITION))
		return 1;
	else if(type & (EFFECT_TYPE_QUICK_O | EFFECT_TYPE_QUICK_F))
		return 2;
	else if(type & EFFECT_TYPE_ACTIVATE) {
		if(handler->data.type & TYPE_MONSTER)
			return 0;
		else if(handler->data.type & TYPE_SPELL) {
			if(handler->data.type & TYPE_QUICKPLAY)
				return 2;
			return 1;
		} else {
			if (handler->data.type & TYPE_COUNTER)
				return 3;
			return 2;
		}
	}
	return 0;
}
uint8 effect::get_owner_player() {
	if(effect_owner != PLAYER_NONE)
		return effect_owner;
	return owner->current.controler;
}
uint8 effect::get_handler_player() {
	if(flag & EFFECT_FLAG_FIELD_ONLY)
		return effect_owner;
	return handler->current.controler;
}
