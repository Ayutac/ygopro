/*
 * card.cpp
 *
 *  Created on: 2010-5-7
 *      Author: Argon
 */

#include "card.h"
#include "field.h"
#include "effect.h"
#include "duel.h"
#include "group.h"
#include "interpreter.h"
#include "ocgapi.h"
#include <memory.h>
#include <iostream>
#include <algorithm>

/*
 * The comparison operator of struct card_sort. Sorts cards by their ID.
 */
bool card_sort::operator()(void* const & p1, void* const & p2) const {
	card* c1 = (card*)p1;
	card* c2 = (card*)p2;
	return c1->cardid < c2->cardid;
}

/*
 * Sorts two cards which are used in a duel.
 */
bool card::card_operation_sort(card* c1, card* c2) {
	duel* pduel = c1->pduel; // the two cards are expected to be in the same duel (?)
	int32 cp1 = c1->overlay_target ? c1->overlay_target->current.controler : c1->current.controler; // get the field side / controller of the cards (?)
	int32 cp2 = c2->overlay_target ? c2->overlay_target->current.controler : c2->current.controler;
	if(cp1 != cp2) { // if cards are on opposite sides of the field and
		if(cp1 == PLAYER_NONE || cp2 == PLAYER_NONE) // if a position is not associated to a player, put it behind 
			return cp1 < cp2;
		// else card of active player comes first
		if(pduel->game_field->infos.turn_player == 0) 
			return cp1 < cp2;
		else
			return cp1 > cp2;
	}
	if(c1->current.location != c2->current.location) // order by LOCATION_constants in card.h, i.e. deck < hand < monster zone < etc
		return c1->current.location < c2->current.location;
	if(c1->current.location & LOCATION_OVERLAY) { // if c1 is in a overlay status (then so must be c2, in the same location too) ?
		if(c1->overlay_target->current.sequence != c2->overlay_target->current.sequence)
			return c1->overlay_target->current.sequence < c2->overlay_target->current.sequence;
		else return c1->current.sequence < c2->current.sequence;
	} else {
		if(c1->current.location & 0x71) // 0x71 would be extra | removed | grave | deck
			return c1->current.sequence > c2->current.sequence; // so there change the ordering
		else
			return c1->current.sequence < c2->current.sequence; // else do the "normal" ordering
	}
}

/*
 * Constructor of the card class. Initializes some values and reserves memory for the structs.
 */
card::card() {
	scrtype = 1;
	ref_handle = 0;
	owner = PLAYER_NONE;
	operation_param = 0;
	status = 0;
	memset(&q_cache, 0xff, sizeof(query_cache));
	equiping_target = 0;
	pre_equip_target = 0;
	overlay_target = 0;
	memset(&current, 0, sizeof(card_state));
	memset(&previous, 0, sizeof(card_state));
	memset(&temp, 0xff, sizeof(card_state));
	unique_pos[0] = unique_pos[1] = 0;
	unique_code = 0;
	assume_type = 0;
	assume_value = 0;
	current.controler = PLAYER_NONE;
}

/*
 * Destructor of the card class. Tidies up.
 */
card::~card() {
	indexer.clear();
	relations.clear();
	counters.clear();
	equiping_cards.clear();
	material_cards.clear();
	single_effect.clear();
	field_effect.clear();
	equip_effect.clear();
	relate_effect.clear();
}

/*
 * Returns info about the card. buf is where the information will be saved.
 */
uint32 card::get_infos(byte* buf, int32 query_flag, int32 use_cache) {
	int32* p = (int32*)buf; // we take a pointer to the start of the buffer
	int32 tdata = 0; // a help variable
	p += 2; // space for number of entries and the query_flag
	/*
	 * Whenever one part of the query_flag is matched, we increase the pointer p
	 * through p++ and then save the queried value into the dereferenced adress through
	 * *p; we just do the incrementing and dereferencing in one step with *p++
	 */
	if(query_flag & QUERY_CODE) *p++ = data.code;
	if(query_flag & QUERY_POSITION) *p++ = get_info_location();
	/*
	 * We can use a chache to speed up generating the query answer.
	 * If we don't use it for this query, we save the answer for the next time using the nice A = B = C construct.
	 * If we use the cache, some other things are done ?
	 */
	if(!use_cache) { 
		if(query_flag & QUERY_ALIAS) q_cache.code = *p++ = get_code();
		if(query_flag & QUERY_TYPE) q_cache.type = *p++ = get_type();
		if(query_flag & QUERY_LEVEL) q_cache.level = *p++ = get_level();
		if(query_flag & QUERY_RANK) q_cache.rank = *p++ = get_rank();
		if(query_flag & QUERY_ATTRIBUTE) q_cache.attribute = *p++ = get_attribute();
		if(query_flag & QUERY_RACE) q_cache.race = *p++ = get_race();
		if(query_flag & QUERY_ATTACK) q_cache.attack = *p++ = get_attack();
		if(query_flag & QUERY_DEFENCE) q_cache.defence = *p++ = get_defence();
		if(query_flag & QUERY_BASE_ATTACK) q_cache.base_attack = *p++ = get_base_attack();
		if(query_flag & QUERY_BASE_DEFENCE) q_cache.base_defence = *p++ = get_base_defence();
		if(query_flag & QUERY_REASON) q_cache.reason = *p++ = current.reason;
	} else {
		if((query_flag & QUERY_ALIAS) && ((uint32)(tdata = get_code()) != q_cache.alias)) {
			q_cache.alias = tdata;
			*p++ = tdata;
		} else query_flag &= ~QUERY_ALIAS;
		if((query_flag & QUERY_TYPE) && ((uint32)(tdata = get_type()) != q_cache.type)) {
			q_cache.type = tdata;
			*p++ = tdata;
		} else query_flag &= ~QUERY_TYPE;
		if((query_flag & QUERY_LEVEL) && ((uint32)(tdata = get_level()) != q_cache.level)) {
			q_cache.level = tdata;
			*p++ = tdata;
		} else query_flag &= ~QUERY_LEVEL;
		if((query_flag & QUERY_RANK) && ((uint32)(tdata = get_rank()) != q_cache.rank)) {
			q_cache.rank = tdata;
			*p++ = tdata;
		} else query_flag &= ~QUERY_RANK;
		if((query_flag & QUERY_ATTRIBUTE) && ((uint32)(tdata = get_attribute()) != q_cache.attribute)) {
			q_cache.attribute = tdata;
			*p++ = tdata;
		} else query_flag &= ~QUERY_ATTRIBUTE;
		if((query_flag & QUERY_RACE) && ((uint32)(tdata = get_race()) != q_cache.race)) {
			q_cache.race = tdata;
			*p++ = tdata;
		} else query_flag &= ~QUERY_RACE;
		if((query_flag & QUERY_ATTACK) && ((tdata = get_attack()) != q_cache.attack)) {
			q_cache.attack = tdata;
			*p++ = tdata;
		} else query_flag &= ~QUERY_ATTACK;
		if((query_flag & QUERY_DEFENCE) && ((tdata = get_defence()) != q_cache.defence)) {
			q_cache.defence = tdata;
			*p++ = tdata;
		} else query_flag &= ~QUERY_DEFENCE;
		if((query_flag & QUERY_BASE_ATTACK) && ((tdata = get_base_attack()) != q_cache.base_attack)) {
			q_cache.base_attack = tdata;
			*p++ = tdata;
		} else query_flag &= ~QUERY_BASE_ATTACK;
		if((query_flag & QUERY_BASE_DEFENCE) && ((tdata = get_base_defence()) != q_cache.base_defence)) {
			q_cache.base_defence = tdata;
			*p++ = tdata;
		} else query_flag &= ~QUERY_BASE_DEFENCE;
		if((query_flag & QUERY_REASON) && ((uint32)(tdata = current.reason) != q_cache.reason)) {
			q_cache.reason = tdata;
			*p++ = tdata;
		} else query_flag &= ~QUERY_REASON;
	}
	if(query_flag & QUERY_REASON_CARD)
		*p++ = current.reason_card ? current.reason_card->get_info_location() : 0;
	if(query_flag & QUERY_EQUIP_CARD) {
		if(equiping_target)
			*p++ = equiping_target->get_info_location();
		else
			query_flag &= ~QUERY_EQUIP_CARD;
	}
	if(query_flag & QUERY_TARGET_CARD) {
		*p++ = effect_target_cards.size();
		card_set::iterator cit;
		for(cit = effect_target_cards.begin(); cit != effect_target_cards.end(); ++cit)
			*p++ = (*cit)->get_info_location();
	}
	if(query_flag & QUERY_OVERLAY_CARD) {
		*p++ = xyz_materials.size();
		for(auto clit = xyz_materials.begin(); clit != xyz_materials.end(); ++clit)
			*p++ = (*clit)->data.code;
	}
	if(query_flag & QUERY_COUNTERS) {
		*p++ = counters.size();
		counter_map::iterator cmit;
		for(cmit = counters.begin(); cmit != counters.end(); ++cmit)
			*p++ = cmit->first + (cmit->second << 16);
	}
	if(query_flag & QUERY_OWNER)
		*p++ = owner;
	if(query_flag & QUERY_IS_DISABLED) {
		tdata = (status & STATUS_DISABLED) ? 1 : 0;
		if(!use_cache || (tdata != q_cache.is_disabled)) {
			q_cache.is_disabled = tdata;
			*p++ = tdata;
		} else
			query_flag &= ~QUERY_IS_DISABLED;
	}
	if(query_flag & QUERY_IS_PUBLIC)
		*p++ = (status & STATUS_IS_PUBLIC) ? 1 : 0;
	if(!use_cache) {
		if(query_flag & QUERY_LSCALE) q_cache.lscale = *p++ = get_lscale();
		if(query_flag & QUERY_RSCALE) q_cache.rscale = *p++ = get_rscale();
	} else {
		if((query_flag & QUERY_LSCALE) && ((uint32)(tdata = get_lscale()) != q_cache.lscale)) {
			q_cache.lscale = tdata;
			*p++ = tdata;
		} else query_flag &= ~QUERY_LSCALE;
		if((query_flag & QUERY_RSCALE) && ((uint32)(tdata = get_rscale()) != q_cache.rscale)) {
			q_cache.rscale = tdata;
			*p++ = tdata;
		} else query_flag &= ~QUERY_RSCALE;
	}
	*(uint32*)buf = (byte*)p - buf; // (basically) save the number of entries
	*(uint32*)(buf + 4) = query_flag; // save the query_flag
	return (byte*)p - buf; // return the size of entries; the query result is still saved in buf
}

/*
 * Returns information about the position of this card.
 * Bits 1-8 contain the controller.
 * Bits 9-16 contain the location, see LOCATION_constants in card.h (includes overlay)
 * Bits 17-24 contain the sequence ?
 * Bits 25-32 contain the inner sequence or position (depending on overlay or not).
 */
uint32 card::get_info_location() {
	/*
	 * If some other card lies on this card, return the position of the other card
	 * and simply OR the location of it with LOCATION_OVERLAY
	 */
	if(overlay_target) { 
		uint32 c = overlay_target->current.controler;
		uint32 l = overlay_target->current.location | LOCATION_OVERLAY;
		uint32 s = overlay_target->current.sequence;
		uint32 ss = current.sequence;
		return c + (l << 8) + (s << 16) + (ss << 24);
	} else {
		uint32 c = current.controler;
		uint32 l = current.location;
		uint32 s = current.sequence;
		uint32 ss = current.position;
		return c + (l << 8) + (s << 16) + (ss << 24);
	}
}

/*
 * Returns the current identity of this card as given by the corresponding card code (left down corner of a card).
 * "current" means it can be changed, e.g. A Legendary Ocean is always treated as Umi, which is implemented here.
 */
uint32 card::get_code() {
	if(assume_type == ASSUME_CODE)
		return assume_value;
	/*
	 * Here the assumption is made that not-continous aliases can only be changed while in 0x1c
	 * So if they are not there, we can assume the code is the card's printed code respectively its alias' code.
	 */
	if(!(current.location & 0x1c)) { // that is mzone | szone | grave
		if(data.alias)
			return data.alias;
		return data.code;
	}
	if (temp.code != 0xffffffff) // if the code is altered to a valid value, return it
		return temp.code;
	// else we check/determinate what could it possibly be now, by checking active effects 
	effect_set effects;
	uint32 code = data.code;
	temp.code = data.code;
	filter_effect(EFFECT_CHANGE_CODE, &effects);
	if (effects.count)
		code = effects.get_last()->get_value(this);
	temp.code = 0xffffffff;
	if (code == data.code) {
		if(data.alias)
			code = data.alias;
	} else {
		card_data dat;
		read_card(code, &dat);
		if (dat.alias)
			code = dat.alias;
	}
	return code;
}

/*
 * Returns another code ?
 */
uint32 card::get_another_code() {
	effect_set eset;
	filter_effect(EFFECT_ADD_CODE, &eset);
	if(!eset.count)
		return 0;
	uint32 otcode = eset.get_last()->get_value(this);
	if(get_code() != otcode)
		return otcode;
	if(data.alias == otcode)
		return data.code;
	return 0;
}

/*
 * ?
 */
int32 card::is_set_card(uint32 set_code) {
	uint32 code = get_code();
	uint64 setcode;
	if (code == data.code) {
		setcode = data.setcode;
	} else {
		card_data dat;
		::read_card(code, &dat);
		setcode = dat.setcode;
	}
	uint32 settype = set_code & 0xfff;
	uint32 setsubtype = set_code & 0xf000;
	while(setcode) {
		if ((setcode & 0xfff) == settype && (setcode & 0xf000 & setsubtype) == setsubtype)
			return TRUE;
		setcode = setcode >> 16;
	}
	return FALSE;
}

/*
 * Returns the type of this card, see TYPE_constants in card.h
 */
uint32 card::get_type() {
	if(assume_type == ASSUME_TYPE)
		return assume_value;
	/*
	 * Here the assumption is made that card types can only be changed while in 0x1e
	 * So if they are not there, we can assume the card type is the card's type identified by it's color.
	 */
	if(!(current.location & 0x1e)) // that is hand | mzone | szone | grave
		return data.type;
	if((current.location == LOCATION_SZONE) && (current.sequence >= 6))
		return TYPE_PENDULUM + TYPE_SPELL; // pendulum monsters treated as spells
	if (temp.type != 0xffffffff) // if the code is altered to a valid value, return it
		return temp.type;
	// else we check/determinate what could it possibly be now, by checking active effects 
	effect_set effects;
	int32 type = data.type;
	temp.type = data.type;
	filter_effect(EFFECT_ADD_TYPE, &effects, FALSE);
	filter_effect(EFFECT_REMOVE_TYPE, &effects, FALSE);
	filter_effect(EFFECT_CHANGE_TYPE, &effects);
	for (int32 i = 0; i < effects.count; ++i) {
		if (effects[i]->code == EFFECT_ADD_TYPE)
			type |= effects[i]->get_value(this);
		else if (effects[i]->code == EFFECT_REMOVE_TYPE)
			type &= ~(effects[i]->get_value(this));
		else
			type = effects[i]->get_value(this);
		temp.type = type;
	}
	temp.type = 0xffffffff;
	return type;
}

/*
 * Returns the base attack of the card.
 */
int32 card::get_base_attack(uint8 swap) {
	/*
	 * Here the assumption is made that the base attack can't be
         * changed while the card is not in the monster card zone.
	 */
	if (current.location != LOCATION_MZONE)
		return data.attack;
	if (temp.base_attack != -1) // if the code is altered to a valid value, return it
		return temp.base_attack;
	// else we check/determinate what could it possibly be now, by checking active effects 
	if(!swap && is_affected_by_effect(EFFECT_SWAP_BASE_AD))
		return get_base_defence(TRUE);
	int32 batk = data.attack;
	temp.base_attack = data.attack;
	if(temp.base_attack < 0)
		temp.base_attack = 0;
	effect_set effects;
	filter_effect(EFFECT_SET_BASE_ATTACK, &effects);
	for (int32 i = 0; i < effects.count; ++i) {
		batk = effects[i]->get_value(this);
		if (batk < 0)
			batk = 0;
		temp.base_attack = batk;
	}
	if (batk < 0)
		batk = 0;
	temp.base_attack = -1;
	return batk;
}

/*
 * Returns the attack of the card.
 */
int32 card::get_attack(uint8 swap) {
	if(assume_type == ASSUME_ATTACK)
		return assume_value;
	/*
	 * Here the assumption is made that the attack can't be
         * changed while the card is not in the monster card zone.
	 */
	if (current.location != LOCATION_MZONE)
		return data.attack; // this is why we return the base attack of the card
	if (temp.attack != -1) // if the code is altered to a valid value, return it
		return temp.attack;
	// else we check/determinate what could it possibly be now, by checking active effects
	if(!swap && is_affected_by_effect(EFFECT_SWAP_AD))
		return get_defence(TRUE);
	uint32 base = get_base_attack();
	temp.base_attack = base;
	temp.attack = base;
	int32 up = 0, upc = 0, final = -1, atk, rev = FALSE;
	effect_set eset;
	effect_set effects;
	filter_effect(EFFECT_UPDATE_ATTACK, &eset, FALSE);
	filter_effect(EFFECT_SET_ATTACK, &eset, FALSE);
	filter_effect(EFFECT_SET_ATTACK_FINAL, &eset);
	if (is_affected_by_effect(EFFECT_REVERSE_UPDATE))
		rev = TRUE;
	for (int32 i = 0; i < eset.count; ++i) {
		switch (eset[i]->code) {
		case EFFECT_UPDATE_ATTACK:
			if ((eset[i]->type & EFFECT_TYPE_SINGLE) && !(eset[i]->flag & EFFECT_FLAG_SINGLE_RANGE)) {
				for (int32 j = 0; j < effects.count; ++j) {
					if (effects[j]->flag & EFFECT_FLAG_REPEAT) {
						base = effects[j]->get_value(this);
						up = 0;
						upc = 0;
						temp.attack = base;
					}
				}
				up += eset[i]->get_value(this);
			} else
				upc += eset[i]->get_value(this);
			break;
		case EFFECT_SET_ATTACK:
			base = eset[i]->get_value(this);
			if (!(eset[i]->type & EFFECT_TYPE_SINGLE))
				up = 0;
			break;
		case EFFECT_SET_ATTACK_FINAL:
			if ((eset[i]->type & EFFECT_TYPE_SINGLE) && !(eset[i]->flag & EFFECT_FLAG_SINGLE_RANGE)) {
				base = eset[i]->get_value(this);
				up = 0;
				upc = 0;
			} else
				effects.add_item(eset[i]);
			break;
		}
		if (!rev)
			temp.attack = base + up + upc;
		else
			temp.attack = base - up - upc;
	}
	for (int32 i = 0; i < effects.count; ++i) {
		final = effects[i]->get_value(this);
		temp.attack = final;
	}
	if (final == -1) {
		if (!rev)
			atk = base + up + upc;
		else
			atk = base - up - upc;
	} else
		atk = final;
	if (atk < 0)
		atk = 0;
	temp.base_attack = -1;
	temp.attack = -1;
	return atk;
}

/*
 * Returns the base defense of the card.
 */
int32 card::get_base_defence(uint8 swap) {
	/*
	 * Here the assumption is made that the base defense can't be
         * changed while the card is not in the monster card zone.
	 */
	if (current.location != LOCATION_MZONE)
		return data.defence;
	if (temp.base_defence != -1) // if the code is altered to a valid value, return it
		return temp.base_defence;
	// else we check/determinate what could it possibly be now, by checking active effects
	if(!swap && is_affected_by_effect(EFFECT_SWAP_BASE_AD))
		return get_base_attack(TRUE);
	int32 bdef = data.defence;
	temp.base_defence = data.defence;
	if(temp.base_defence < 0)
		temp.base_defence = 0;
	effect_set effects;
	filter_effect(EFFECT_SET_BASE_DEFENCE, &effects);
	for (int32 i = 0; i < effects.count; ++i) {
		bdef = effects[i]->get_value(this);
		if (bdef < 0)
			bdef = 0;
		temp.base_defence = bdef;
	}
	if (bdef < 0)
		bdef = 0;
	temp.base_defence = -1;
	return bdef;
}

/*
 * Returns the defense of the card.
 */
int32 card::get_defence(uint8 swap) {
	if(assume_type == ASSUME_DEFENCE)
		return assume_value;
	/*
	 * Here the assumption is made that the base defense can't be
         * changed while the card is not in the monster card zone.
	 */
	if (current.location != LOCATION_MZONE)
		return data.defence; // this is why we return the base defense of the card
	if (temp.defence != -1) // if the code is altered to a valid value, return it
		return temp.defence;
	// else we check/determinate what could it possibly be now, by checking active effects
	if(!swap && is_affected_by_effect(EFFECT_SWAP_AD))
		return get_attack(TRUE);
	uint32 base = get_base_defence();
	temp.base_defence = base;
	temp.defence = base;
	int32 up = 0, upc = 0, final = -1, def, rev = FALSE;
	effect_set eset;
	effect_set effects;
	filter_effect(EFFECT_UPDATE_DEFENCE, &eset, FALSE);
	filter_effect(EFFECT_SET_DEFENCE, &eset, FALSE);
	filter_effect(EFFECT_SET_DEFENCE_FINAL, &eset);
	if (is_affected_by_effect(EFFECT_REVERSE_UPDATE))
		rev = TRUE;
	for (int32 i = 0; i < eset.count; ++i) {
		switch (eset[i]->code) {
		case EFFECT_UPDATE_DEFENCE:
			if ((eset[i]->type & EFFECT_TYPE_SINGLE) && !(eset[i]->flag & EFFECT_FLAG_SINGLE_RANGE)) {
				for (int32 j = 0; j < effects.count; ++j) {
					if (effects[j]->flag & EFFECT_FLAG_REPEAT) {
						base = effects[j]->get_value(this);
						up = 0;
						upc = 0;
						temp.defence = base;
					}
				}
				up += eset[i]->get_value(this);
			} else
				upc += eset[i]->get_value(this);
			break;
		case EFFECT_SET_DEFENCE:
			base = eset[i]->get_value(this);
			if (!(eset[i]->type & EFFECT_TYPE_SINGLE))
				up = 0;
			break;
		case EFFECT_SET_DEFENCE_FINAL:
			if ((eset[i]->type & EFFECT_TYPE_SINGLE) && !(eset[i]->flag & EFFECT_FLAG_SINGLE_RANGE)) {
				base = eset[i]->get_value(this);
				up = 0;
				upc = 0;
			} else
				effects.add_item(eset[i]);
			break;
		}
		if (!rev)
			temp.defence = base + up + upc;
		else
			temp.defence = base - up - upc;
	}
	for (int32 i = 0; i < effects.count; ++i) {
		final = effects[i]->get_value(this);
		temp.defence = final;
	}
	if (final == -1) {
		if (!rev)
			def = base + up + upc;
		else
			def = base - up - upc;
	} else
		def = final;
	if (def < 0)
		def = 0;
	temp.base_defence = -1;
	temp.defence = -1;
	return def;
}

/*
 * Returns the level of the card.
 */
uint32 card::get_level() {
	if(data.type & TYPE_XYZ) // XYZ don't have a level
		return 0;
	if(assume_type == ASSUME_LEVEL)
		return assume_value;
	/*
	 * Here the assumption is made that the level can't be
         * changed while the card is not in the monster card zone or in the hand.
	 */
	if(!(current.location & (LOCATION_MZONE + LOCATION_HAND)))
		return data.level;
	if (temp.level != 0xffffffff) // if the code is altered to a valid value, return it
		return temp.level;
	// else we check/determinate what could it possibly be now, by checking active effects
	effect_set effects;
	int32 level = data.level;
	temp.level = data.level;
	int32 up = 0, upc = 0;
	filter_effect(EFFECT_UPDATE_LEVEL, &effects, FALSE);
	filter_effect(EFFECT_CHANGE_LEVEL, &effects);
	for (int32 i = 0; i < effects.count; ++i) {
		if (effects[i]->code == EFFECT_UPDATE_LEVEL) {
			if ((effects[i]->type & EFFECT_TYPE_SINGLE) && !(effects[i]->flag & EFFECT_FLAG_SINGLE_RANGE))
				up += effects[i]->get_value(this);
			else
				upc += effects[i]->get_value(this);
		} else {
			level = effects[i]->get_value(this);
			up = 0;
		}
		temp.level = level;
	}
	level += up + upc;
	if(level < 1 && (get_type() & TYPE_MONSTER))
		level = 1;
	temp.level = 0xffffffff;
	return level;
}

/*
 * Returns the rank of the card.
 */
uint32 card::get_rank() {
	if(!(data.type & TYPE_XYZ)) // only XYZ have a rank
		return 0;
	if(assume_type == ASSUME_RANK)
		return assume_value;
	/*
	 * Here the assumption is made that the rank can't be
         * changed while the card is not in the monster card zone.
	 */
	if(!(current.location & LOCATION_MZONE))
		return data.level;
	if (temp.level != 0xffffffff) // if the code is altered to a valid value, return it
		return temp.level;
	// else we check/determinate what could it possibly be now, by checking active effects
	effect_set effects;
	int32 rank = data.level;
	temp.level = data.level;
	int32 up = 0, upc = 0;
	filter_effect(EFFECT_UPDATE_RANK, &effects, FALSE);
	filter_effect(EFFECT_CHANGE_RANK, &effects);
	for (int32 i = 0; i < effects.count; ++i) {
		if (effects[i]->code == EFFECT_UPDATE_RANK) {
			if ((effects[i]->type & EFFECT_TYPE_SINGLE) && !(effects[i]->flag & EFFECT_FLAG_SINGLE_RANGE))
				up += effects[i]->get_value(this);
			else
				upc += effects[i]->get_value(this);
		} else {
			rank = effects[i]->get_value(this);
			up = 0;
		}
		temp.level = rank;
	}
	rank += up + upc;
	if(rank < 1 && (get_type() & TYPE_MONSTER))
		rank = 1;
	temp.level = 0xffffffff;
	return rank;
}

/*
 * Returns the synchro level of the card ?
 */
uint32 card::get_synchro_level(card* pcard) {
	if(data.type & TYPE_XYZ)
		return 0;
	uint32 lev;
	effect_set eset;
	filter_effect(EFFECT_SYNCHRO_LEVEL, &eset);
	if(eset.count)
		lev = eset[0]->get_value(pcard);
	else
		lev = get_level();
	return lev;
}

/*
 * Returns the ritual level of the card ?
 */
uint32 card::get_ritual_level(card* pcard) {
	if(data.type & TYPE_XYZ)
		return 0;
	uint32 lev;
	effect_set eset;
	filter_effect(EFFECT_RITUAL_LEVEL, &eset);
	if(eset.count)
		lev = eset[0]->get_value(pcard);
	else
		lev = get_level();
	return lev;
}

/*
 * ?
 */
uint32 card::is_xyz_level(card* pcard, uint32 lv) {
	if(data.type & TYPE_XYZ)
		return FALSE;
	uint32 lev;
	effect_set eset;
	filter_effect(EFFECT_XYZ_LEVEL, &eset);
	if(eset.count)
		lev = eset[0]->get_value(pcard);
	else
		lev = get_level();
	return ((lev & 0xffff) == lv) || ((lev >> 16) == lv);
}

/*
 * Returns the attribute of the card.
 */
uint32 card::get_attribute() {
	if(assume_type == ASSUME_ATTRIBUTE)
		return assume_value;
	/*
	 * Here the assumption is made that the attribute can't be
         * changed while the card is not in the monster card zone or in the grave.
	 */
	if(!(current.location & (LOCATION_MZONE + LOCATION_GRAVE)))
		return data.attribute;
	if((current.location == LOCATION_GRAVE) && (data.type & (TYPE_SPELL + TYPE_TRAP)))
		return data.attribute;
	if (temp.attribute != 0xffffffff) // if the code is altered to a valid value, return it
		return temp.attribute;
	// else we check/determinate what could it possibly be now, by checking active effects
	effect_set effects;
	int32 attribute = data.attribute;
	temp.attribute = data.attribute;
	filter_effect(EFFECT_ADD_ATTRIBUTE, &effects, FALSE);
	filter_effect(EFFECT_REMOVE_ATTRIBUTE, &effects, FALSE);
	filter_effect(EFFECT_CHANGE_ATTRIBUTE, &effects);
	for (int32 i = 0; i < effects.count; ++i) {
		if (effects[i]->code == EFFECT_ADD_ATTRIBUTE)
			attribute |= effects[i]->get_value(this);
		else if (effects[i]->code == EFFECT_REMOVE_ATTRIBUTE)
			attribute &= ~(effects[i]->get_value(this));
		else
			attribute = effects[i]->get_value(this);
		temp.attribute = attribute;
	}
	temp.attribute = 0xffffffff;
	return attribute;
}

/*
 * Returns the race of the card.
 */
uint32 card::get_race() {
	if(assume_type == ASSUME_RACE)
		return assume_value;
	/*
	 * Here the assumption is made that the race can't be
         * changed while the card is not in the monster card zone or in the grave.
	 */
	if(!(current.location & (LOCATION_MZONE + LOCATION_GRAVE)))
		return data.race;
	if((current.location == LOCATION_GRAVE) && (data.type & (TYPE_SPELL + TYPE_TRAP)))
		return data.race;
	if (temp.race != 0xffffffff) // if the code is altered to a valid value, return it
		return temp.race;
	// else we check/determinate what could it possibly be now, by checking active effects
	effect_set effects;
	int32 race = data.race;
	temp.race = data.race;
	filter_effect(EFFECT_ADD_RACE, &effects, FALSE);
	filter_effect(EFFECT_REMOVE_RACE, &effects, FALSE);
	filter_effect(EFFECT_CHANGE_RACE, &effects);
	for (int32 i = 0; i < effects.count; ++i) {
		if (effects[i]->code == EFFECT_ADD_RACE)
			race |= effects[i]->get_value(this);
		else if (effects[i]->code == EFFECT_REMOVE_RACE)
			race &= ~(effects[i]->get_value(this));
		else
			race = effects[i]->get_value(this);
		temp.race = race;
	}
	temp.race = 0xffffffff;
	return race;
}

/*
 * Returns the left scale of the pendulum card.
 */
uint32 card::get_lscale() {
	/*
	 * Here the assumption is made that the left scale can't be
         * changed while the card is not in the spell/trap card zone.
	 */
	if(!(current.location & LOCATION_SZONE))
		return data.lscale;
	if (temp.lscale != 0xffffffff)
		return temp.lscale;
	effect_set effects;
	int32 lscale = data.lscale;
	temp.lscale = data.lscale;
	int32 up = 0, upc = 0;
	filter_effect(EFFECT_UPDATE_LSCALE, &effects, FALSE);
	filter_effect(EFFECT_CHANGE_LSCALE, &effects);
	for (int32 i = 0; i < effects.count; ++i) {
		if (effects[i]->code == EFFECT_UPDATE_LSCALE) {
			if ((effects[i]->type & EFFECT_TYPE_SINGLE) && !(effects[i]->flag & EFFECT_FLAG_SINGLE_RANGE))
				up += effects[i]->get_value(this);
			else
				upc += effects[i]->get_value(this);
		} else {
			lscale = effects[i]->get_value(this);
			up = 0;
		}
		temp.lscale = lscale;
	}
	lscale += up + upc;
	temp.lscale = 0xffffffff;
	return lscale;
}

/*
 * Returns the right scale of the pendulum card.
 */
uint32 card::get_rscale() {
	/*
	 * Here the assumption is made that the right scale can't be
         * changed while the card is not in the spell/trap card zone.
	 */
	if(!(current.location & LOCATION_SZONE))
		return data.rscale;
	if (temp.rscale != 0xffffffff)
		return temp.rscale;
	effect_set effects;
	int32 rscale = data.rscale;
	temp.rscale = data.rscale;
	int32 up = 0, upc = 0;
	filter_effect(EFFECT_UPDATE_RSCALE, &effects, FALSE);
	filter_effect(EFFECT_CHANGE_RSCALE, &effects);
	for (int32 i = 0; i < effects.count; ++i) {
		if (effects[i]->code == EFFECT_UPDATE_RSCALE) {
			if ((effects[i]->type & EFFECT_TYPE_SINGLE) && !(effects[i]->flag & EFFECT_FLAG_SINGLE_RANGE))
				up += effects[i]->get_value(this);
			else
				upc += effects[i]->get_value(this);
		} else {
			rscale = effects[i]->get_value(this);
			up = 0;
		}
		temp.rscale = rscale;
	}
	rscale += up + upc;
	temp.rscale = 0xffffffff;
	return rscale;
}

/*
 * Returns what the current position of the card has in common with pos (e.g. face-up). See also POSITION_constants in card.h
 * (is_position(pos)) is true if the current position has any flag in common with pos
 * (pos == is_position(pos)) is true if the current position has all flags true which are true in pos.
 */
int32 card::is_position(int32 pos) {
	return current.position & pos;
}

/*
 * Sets or unsets the status of a card (e.g. disabled), see STATUS_constants in card.h
 */
void card::set_status(uint32 status, int32 enabled) {
	if (enabled)
		this->status |= status; // the new status will be added
	else 
		this->status &= ~status; // all common flags with status will be removed
}

/*
 * Returns what the status of the card has in common with status.
 * (get_status(status)) is true if the status of the card has any flag in common with status
 * (status == get_status(status)) is true if the status of the card has all flags true which are true in status.
 */
int32 card::get_status(uint32 status) {
	return this->status & status;
}

/*
 * Returns true if the status of the card has all flags true which are true in status.
 * Equivalent to (get_status(status) == status)
 */
int32 card::is_status(uint32 status) {
	if ((this->status & status) == status)
		return TRUE;
	return FALSE;
}

/*
 * Equips this card to another card (target) during a duel. If send_msg != 0 a message about the action will be sent to the duel.
 */
void card::equip(card *target, uint32 send_msg) {
	if (equiping_target) // if this card is already enquipped, it can't be enquipped to yet another card
		return;
	target->equiping_cards.insert(this); // target must know it's equipped cards
	equiping_target = target;
	// effect taking place ?
	for (auto it = equip_effect.begin(); it != equip_effect.end(); ++it) {
		if (it->second->is_disable_related())
			pduel->game_field->add_to_disable_check_list(equiping_target);
	}
	// write information about what happened
	if(send_msg) {
		pduel->write_buffer8(MSG_EQUIP);
		pduel->write_buffer32(get_info_location());
		pduel->write_buffer32(target->get_info_location());
	}
	return;
}

/*
 * Unequips this card from the card it equipped during a duel.
 */
void card::unequip() {
	if (!equiping_target) // if not equipped to anything, can't be unequipped of curse
		return;
	// effect taking place
	for (auto it = equip_effect.begin(); it != equip_effect.end(); ++it) {
		if (it->second->is_disable_related())
			pduel->game_field->add_to_disable_check_list(equiping_target);
	}
	equiping_target->equiping_cards.erase(this); // remove the card from its target's equip list
	pre_equip_target = equiping_target; // remember what target was (for what purpose ?)
	equiping_target = 0; // remove the target
	return;
}

/*
 * Returns the number of union cards equipped to this card during a duel
 */
int32 card::get_union_count() {
	card_set::iterator cit;
	int32 count = 0;
	for(cit = equiping_cards.begin(); cit != equiping_cards.end(); ++cit) {
		// card must be a union card and be used as such (e.g. no Z-Cannon equipped to the Allure Queen)
		if(((*cit)->data.type & TYPE_UNION) && (*cit)->is_status(STATUS_UNION))
			count++;
	}
	return count;
}

/*
 * ?
 */
void card::xyz_overlay(card_set* materials) {
	if(materials->size() == 0)
		return;
	card_set des;
	if(materials->size() == 1) {
		card* pcard = *materials->begin();
		pcard->reset(RESET_LEAVE + RESET_OVERLAY, RESET_EVENT);
		if(pcard->unique_code)
			pduel->game_field->remove_unique_card(pcard);
		xyz_add(pcard, &des);
	} else {
		field::card_vector cv;
		for(auto cit = materials->begin(); cit != materials->end(); ++cit)
			cv.push_back(*cit);
		std::sort(cv.begin(), cv.end(), card::card_operation_sort);
		for(auto cvit = cv.begin(); cvit != cv.end(); ++cvit) {
			(*cvit)->reset(RESET_LEAVE + RESET_OVERLAY, RESET_EVENT);
			if((*cvit)->unique_code)
				pduel->game_field->remove_unique_card(*cvit);
			xyz_add(*cvit, &des);
		}
	}
	if(des.size())
		pduel->game_field->destroy(&des, 0, REASON_LOST_TARGET + REASON_RULE, PLAYER_NONE);
}

/*
 * Adds a certain card mat to the xyz material of this card during a duel.
 * Technically non-Xyz monsters can be given material with this method.
 * What is des ?
 */
void card::xyz_add(card* mat, card_set* des) {
	if(mat->overlay_target == this) // a card can't be added as Xyz material if it already has
		return;
	pduel->write_buffer8(MSG_MOVE);
	pduel->write_buffer32(mat->data.code);
	mat->enable_field_effect(false); // this is unneeded, implying that xyz-material doesn't do field effects (see next else)
	if(mat->overlay_target) { // if the material is currently some other xyz's material, remove it from there (and send messages)
		pduel->write_buffer8(mat->overlay_target->current.controler);
		pduel->write_buffer8(mat->overlay_target->current.location | LOCATION_OVERLAY);
		pduel->write_buffer8(mat->overlay_target->current.sequence);
		pduel->write_buffer8(mat->current.sequence);
		mat->overlay_target->xyz_remove(mat);
	} else { // else remove it from the game field (and send messages)
		pduel->write_buffer8(mat->current.controler);
		pduel->write_buffer8(mat->current.location);
		pduel->write_buffer8(mat->current.sequence);
		pduel->write_buffer8(mat->current.position);
		mat->enable_field_effect(false); // the field effect of the new material ends (e.g. Jinzos effect)
		pduel->game_field->remove_card(mat); // what exactly is "game_field"? does it include hand and deck?
	}
	// more messages
	pduel->write_buffer8(current.controler);
	pduel->write_buffer8(current.location | LOCATION_OVERLAY);
	pduel->write_buffer8(current.sequence);
	pduel->write_buffer8(current.position);
	pduel->write_buffer32(REASON_XYZ + REASON_MATERIAL); // why the change is happening
	xyz_materials.push_back(mat); // ?
	// some effect taking place ?
	for(auto cit = mat->equiping_cards.begin(); cit != mat->equiping_cards.end();) {
		auto rm = cit++;
		des->insert(*rm);
		(*rm)->unequip();
	}
	/*
	 * Finally tell the system the new material is where it should be.
	 * We erase all location information from the material because when the control of this
	 * card changes, so does the control of all material, but it is enough to know about the 
	 * monster which owns the material, so we save some computation time with this approach.
	 * Could become a problem later, of course, but for now let's enjoy that it works.
	 */
	mat->overlay_target = this;
	mat->current.controler = PLAYER_NONE;
	mat->current.location = LOCATION_OVERLAY;
	mat->current.sequence = xyz_materials.size() - 1;
	mat->current.reason = REASON_XYZ + REASON_MATERIAL;
}

/*
 * Removes a certain card as XYZ-material.
 */
void card::xyz_remove(card* mat) {
	if(mat->overlay_target != this) // if the material doesn't belong to this card, abort
		return;
	xyz_materials.erase(xyz_materials.begin() + mat->current.sequence); // ?
	/*
	 * The structual weakness from xyz_add is cleverly avoided here with the use of current :)
	 */
	mat->previous.controler = mat->current.controler;
	mat->previous.location = mat->current.location;
	mat->previous.sequence = mat->current.sequence;
	// but now the material is really sent to outer space
	mat->current.controler = PLAYER_NONE;
	mat->current.location = 0;
	mat->current.sequence = 0;
	mat->overlay_target = 0;
	// whats wrong with the clit?
	for(auto clit = xyz_materials.begin(); clit != xyz_materials.end(); ++clit)
		(*clit)->current.sequence = clit - xyz_materials.begin();
}

/*
 * Applies field effects to this card during a duel, including uniqueness
 */
void card::apply_field_effect() {
	if (current.controler == PLAYER_NONE) // if this card belongs to no one, no effects are to be applied
		return;
	/* 
	 * Check out which affects are to apply.
	 * These effects must have the range where this card is currently in or
	 * they must affect the hand and somewhat be specifically triggered by this card ?
	 */
	for (auto it = field_effect.begin(); it != field_effect.end(); ++it) {
		if ((current.location & it->second->range) || ((it->second->range & LOCATION_HAND)
			&& (it->second->type & EFFECT_TYPE_TRIGGER_O) && !(it->second->code & EVENT_PHASE)))
			pduel->game_field->add_effect(it->second);
	}
	// if only one of this card can be on the field, mark it activated if it's on the field
	if(unique_code && (current.location & LOCATION_ONFIELD)) 
		pduel->game_field->add_unique_card(this);
}

/*
 * Cancels field effects to this card during a duel, including uniqueness
 */
void card::cancel_field_effect() {
	if (current.controler == PLAYER_NONE) // if this card belongs to no one, no effects are to be cancelled
		return;
	/* 
	 * Check out which affects are to cancel.
	 * These effects must have the range where this card is currently in or
	 * they must affect the hand and somewhat be specifically triggered by this card ?
	 */
	for (auto it = field_effect.begin(); it != field_effect.end(); ++it) {
		if ((current.location & it->second->range) || ((it->second->range & LOCATION_HAND)
			&& (it->second->type & EFFECT_TYPE_TRIGGER_O) && !(it->second->code & EVENT_PHASE)))
			pduel->game_field->remove_effect(it->second);
	}
	// if only one of this card can be on the field and this one is, then remove it's uniqueness
	if(unique_code && (current.location & LOCATION_ONFIELD)) 
		pduel->game_field->remove_unique_card(this);
}

/*
 * Enables or disables a field effect during a duel.
 */
void card::enable_field_effect(int32 enabled) {
	if (current.location == 0) // nirvana cards can't be applied
		return;
	if ((enabled && get_status(STATUS_EFFECT_ENABLED)) || (!enabled && !get_status(STATUS_EFFECT_ENABLED)))
		return; // if effect is running and should or can't be activated and shouldn't, do nothing
	refresh_disable_status(); // ?
	if (enabled) { // if to enable
		set_status(STATUS_EFFECT_ENABLED, TRUE); // formally enable
		effect_container::iterator it;
		// ???
		for (it = single_effect.begin(); it != single_effect.end(); ++it) {
			if ((it->second->flag & EFFECT_FLAG_SINGLE_RANGE) && (current.location & it->second->range))
				it->second->id = pduel->game_field->infos.field_id++;
		}
		for (it = field_effect.begin(); it != field_effect.end(); ++it) {
			if (current.location & it->second->range)
				it->second->id = pduel->game_field->infos.field_id++;
		}
		if(current.location == LOCATION_SZONE) {
			for (it = equip_effect.begin(); it != equip_effect.end(); ++it)
				it->second->id = pduel->game_field->infos.field_id++;
		}
		if (get_status(STATUS_DISABLED))
			reset(RESET_DISABLE, RESET_EVENT);
	} else // else disable
		set_status(STATUS_EFFECT_ENABLED, FALSE);
	filter_immune_effect();
	if (get_status(STATUS_DISABLED))
		return;
	filter_disable_related_cards();
}

/*
 * ?
 */
int32 card::add_effect(effect* peffect) {
	effect_container::iterator it, rm;
	if (get_status(STATUS_COPYING_EFFECT) && (peffect->flag & EFFECT_FLAG_UNCOPYABLE)) {
		pduel->uncopy.insert(peffect);
		return 0;
	}
	if (indexer.find(peffect) != indexer.end())
		return 0;
	card* check_target = this;
	if (peffect->type & EFFECT_TYPE_SINGLE) {
		if(peffect->code == EFFECT_SET_ATTACK && !(peffect->flag & EFFECT_FLAG_SINGLE_RANGE)) {
			for(it = single_effect.begin(); it != single_effect.end();) {
				rm = it++;
				if((rm->second->code == EFFECT_SET_ATTACK || rm->second->code == EFFECT_SET_ATTACK_FINAL)
					&& !(rm->second->flag & EFFECT_FLAG_SINGLE_RANGE))
					remove_effect(rm->second);
			}
		}
		if(peffect->code == EFFECT_SET_ATTACK_FINAL && !(peffect->flag & EFFECT_FLAG_SINGLE_RANGE)) {
			for(it = single_effect.begin(); it != single_effect.end();) {
				rm = it++;
				if((rm->second->code == EFFECT_UPDATE_ATTACK || rm->second->code == EFFECT_SET_ATTACK
					|| rm->second->code == EFFECT_SET_ATTACK_FINAL) && !(rm->second->flag & EFFECT_FLAG_SINGLE_RANGE))
					remove_effect(rm->second);
			}
		}
		if(peffect->code == EFFECT_SET_DEFENCE && !(peffect->flag & EFFECT_FLAG_SINGLE_RANGE)) {
			for(it = single_effect.begin(); it != single_effect.end();) {
				rm = it++;
				if((rm->second->code == EFFECT_SET_DEFENCE || rm->second->code == EFFECT_SET_DEFENCE_FINAL)
					&& !(rm->second->flag & EFFECT_FLAG_SINGLE_RANGE))
					remove_effect(rm->second);
			}
		}
		if(peffect->code == EFFECT_SET_DEFENCE_FINAL && !(peffect->flag & EFFECT_FLAG_SINGLE_RANGE)) {
			for(it = single_effect.begin(); it != single_effect.end();) {
				rm = it++;
				if((rm->second->code == EFFECT_UPDATE_DEFENCE || rm->second->code == EFFECT_SET_DEFENCE
					|| rm->second->code == EFFECT_SET_DEFENCE_FINAL) && !(rm->second->flag & EFFECT_FLAG_SINGLE_RANGE))
					remove_effect(rm->second);
			}
		}
		it = single_effect.insert(make_pair(peffect->code, peffect));
	} else if (peffect->type & EFFECT_TYPE_FIELD)
		it = field_effect.insert(make_pair(peffect->code, peffect));
	else if (peffect->type & EFFECT_TYPE_EQUIP) {
		it = equip_effect.insert(make_pair(peffect->code, peffect));
		if (equiping_target)
			check_target = equiping_target;
		else
			check_target = 0;
	} else
		return 0;
	peffect->id = pduel->game_field->infos.field_id++;
	peffect->card_type = data.type;
	if(get_status(STATUS_INITIALIZING))
		peffect->flag |= EFFECT_FLAG_INITIAL;
	if (get_status(STATUS_COPYING_EFFECT)) {
		peffect->copy_id = pduel->game_field->infos.copy_id;
		peffect->reset_flag |= pduel->game_field->core.copy_reset;
		peffect->reset_count = (peffect->reset_count & 0xffffff00) | pduel->game_field->core.copy_reset_count;
	}
	if((peffect->flag & EFFECT_FLAG_COPY_INHERIT) && pduel->game_field->core.reason_effect
		&& (pduel->game_field->core.reason_effect->copy_id)) {
		peffect->copy_id = pduel->game_field->core.reason_effect->copy_id;
		peffect->reset_flag |= pduel->game_field->core.reason_effect->reset_flag;
		if((peffect->reset_count & 0xff) > (pduel->game_field->core.reason_effect->reset_count & 0xff))
			peffect->reset_count = (peffect->reset_count & 0xffffff00) | (pduel->game_field->core.reason_effect->reset_count & 0xff);
	}
	indexer.insert(make_pair(peffect, it));
	peffect->handler = this;
	if ((current.location & peffect->range) && peffect->type & EFFECT_TYPE_FIELD)
		pduel->game_field->add_effect(peffect);
	if (current.controler != PLAYER_NONE && check_target) {
		if (peffect->is_disable_related())
			pduel->game_field->add_to_disable_check_list(check_target);
	}
	if(peffect->flag & EFFECT_FLAG_OATH) {
		effect* reason_effect = pduel->game_field->core.reason_effect;
		pduel->game_field->effects.oath.insert(make_pair(peffect, reason_effect));
	}
	if(peffect->reset_flag & RESET_PHASE) {
		pduel->game_field->effects.pheff.insert(peffect);
		if((peffect->reset_count & 0xff) == 0)
			peffect->reset_count += 1;
	}
	if(peffect->reset_flag & RESET_CHAIN)
		pduel->game_field->effects.cheff.insert(peffect);
	if(peffect->flag & EFFECT_FLAG_COUNT_LIMIT)
		pduel->game_field->effects.rechargeable.insert(peffect);
	if(peffect->flag & EFFECT_FLAG_CLIENT_HINT) {
		pduel->write_buffer8(MSG_CARD_HINT);
		pduel->write_buffer32(get_info_location());
		pduel->write_buffer8(CHINT_DESC_ADD);
		pduel->write_buffer32(peffect->description);
	}
	return peffect->id;
}

/*
 * ?
 */
void card::remove_effect(effect* peffect) {
	auto it = indexer.find(peffect);
	if (it == indexer.end())
		return;
	remove_effect(peffect, it->second);
}

/*
 * ?
 */
void card::remove_effect(effect* peffect, effect_container::iterator it) {
	card* check_target = this;
	if (peffect->type & EFFECT_TYPE_SINGLE)
		single_effect.erase(it);
	else if (peffect->type & EFFECT_TYPE_FIELD) {
		check_target = 0;
		if ((current.location & peffect->range) && get_status(STATUS_EFFECT_ENABLED) && !get_status(STATUS_DISABLED)) {
			if (peffect->is_disable_related())
				pduel->game_field->update_disable_check_list(peffect);
		}
		field_effect.erase(it);
		if (current.location & peffect->range)
			pduel->game_field->remove_effect(peffect);
	} else if (peffect->type & EFFECT_TYPE_EQUIP) {
		equip_effect.erase(it);
		if (equiping_target)
			check_target = equiping_target;
		else
			check_target = 0;
	}
	if ((current.controler != PLAYER_NONE) && !get_status(STATUS_DISABLED) && check_target) {
		if (peffect->is_disable_related())
			pduel->game_field->add_to_disable_check_list(check_target);
	}
	indexer.erase(peffect);
	if(peffect->flag & EFFECT_FLAG_OATH)
		pduel->game_field->effects.oath.erase(peffect);
	if(peffect->reset_flag & RESET_PHASE)
		pduel->game_field->effects.pheff.erase(peffect);
	if(peffect->reset_flag & RESET_CHAIN)
		pduel->game_field->effects.cheff.erase(peffect);
	if(peffect->flag & EFFECT_FLAG_COUNT_LIMIT)
		pduel->game_field->effects.rechargeable.erase(peffect);
	if(((peffect->code & 0xf0000) == EFFECT_COUNTER_PERMIT) && (peffect->type & EFFECT_TYPE_SINGLE)) {
		auto cmit = counters.find(peffect->code & 0xffff);
		if(cmit != counters.end()) {
			pduel->write_buffer8(MSG_REMOVE_COUNTER);
			pduel->write_buffer16(cmit->first);
			pduel->write_buffer8(current.controler);
			pduel->write_buffer8(current.location);
			pduel->write_buffer8(current.sequence);
			pduel->write_buffer8(cmit->second);
			counters.erase(cmit);
		}
	}
	if(peffect->flag & EFFECT_FLAG_CLIENT_HINT) {
		pduel->write_buffer8(MSG_CARD_HINT);
		pduel->write_buffer32(get_info_location());
		pduel->write_buffer8(CHINT_DESC_REMOVE);
		pduel->write_buffer32(peffect->description);
	}
	if(peffect->code == EFFECT_UNIQUE_CHECK) {
		pduel->game_field->remove_unique_card(this);
		unique_pos[0] = unique_pos[1] = 0;
		unique_code = 0;
	}
	pduel->game_field->core.reseted_effects.insert(peffect);
}

/*
 * ?
 */
int32 card::copy_effect(uint32 code, uint32 reset, uint32 count) {
	card_data cdata;
	read_card(code, &cdata);
	if(cdata.type & TYPE_NORMAL) // effect of normal monsters can't be copied, duh
		return -1;
	set_status(STATUS_COPYING_EFFECT, TRUE); // this card is now copying an effect
	uint32 cr = pduel->game_field->core.copy_reset;
	uint8 crc = pduel->game_field->core.copy_reset_count;
	pduel->game_field->core.copy_reset = reset;
	pduel->game_field->core.copy_reset_count = count;
	pduel->lua->add_param(this, PARAM_TYPE_CARD);
	pduel->lua->call_code_function(code, (char*) "initial_effect", 1, 0);
	pduel->game_field->infos.copy_id++;
	set_status(STATUS_COPYING_EFFECT, FALSE);
	pduel->game_field->core.copy_reset = cr;
	pduel->game_field->core.copy_reset_count = crc;
	for(auto eit = pduel->uncopy.begin(); eit != pduel->uncopy.end(); ++eit)
		pduel->delete_effect(*eit);
	pduel->uncopy.clear();
	return pduel->game_field->infos.copy_id - 1;
}

/*
 * ?
 */
void card::reset(uint32 id, uint32 reset_type) {
	effect* peffect;
	if (reset_type != RESET_EVENT && reset_type != RESET_PHASE && reset_type != RESET_CODE && reset_type != RESET_COPY && reset_type != RESET_CARD)
		return;
	if (reset_type == RESET_EVENT) {
		for (auto rit = relations.begin(); rit != relations.end();) {
			auto rrm = rit++;
			if (rrm->second & 0xffff0000 & id)
				relations.erase(rrm);
		}
		if(id & 0x47c0000)
			relate_effect.clear();
		if(id & 0x5fc0000) {
			announced_cards.clear();
			attacked_cards.clear();
			announce_count = 0;
			attacked_count = 0;
			attack_all_target = TRUE;
		}
		if(id & 0x5fe0000) {
			battled_cards.clear();
			reset_effect_count();
			auto pr = field_effect.equal_range(EFFECT_DISABLE_FIELD);
			for(; pr.first != pr.second; ++pr.first)
				pr.first->second->value = 0;
			set_status(STATUS_UNION, FALSE);
		}
		if(id & 0x57e0000) {
			counters.clear();
			for(auto cit = effect_target_owner.begin(); cit != effect_target_owner.end(); ++cit)
				(*cit)->effect_target_cards.erase(this);
			for(auto cit = effect_target_cards.begin(); cit != effect_target_cards.end(); ++cit) {
				card* pcard = *cit;
				pcard->effect_target_owner.erase(this);
				for(auto it = pcard->single_effect.begin(); it != pcard->single_effect.end();) {
					auto rm = it++;
					peffect = rm->second;
					if((peffect->owner == this) && (peffect->flag & EFFECT_FLAG_OWNER_RELATE))
						pcard->remove_effect(peffect, rm);
				}
			}
			effect_target_owner.clear();
			effect_target_cards.clear();
		}
		if(id & 0x3fe0000) {
			auto pr = field_effect.equal_range(EFFECT_USE_EXTRA_MZONE);
			for(; pr.first != pr.second; ++pr.first)
				pr.first->second->value = pr.first->second->value & 0xffff;
			pr = field_effect.equal_range(EFFECT_USE_EXTRA_SZONE);
			for(; pr.first != pr.second; ++pr.first)
				pr.first->second->value = pr.first->second->value & 0xffff;
		}
		if(id & RESET_DISABLE) {
			for(auto cmit = counters.begin(); cmit != counters.end();) {
				auto rm = cmit++;
				if(rm->first & COUNTER_NEED_ENABLE) {
					pduel->write_buffer8(MSG_REMOVE_COUNTER);
					pduel->write_buffer16(rm->first);
					pduel->write_buffer8(current.controler);
					pduel->write_buffer8(current.location);
					pduel->write_buffer8(current.sequence);
					pduel->write_buffer8(rm->second);
					counters.erase(rm);
				}
			}
		}
		if(id & RESET_TURN_SET) {
			effect* peffect = check_equip_control_effect();
			if(peffect) {
				effect* new_effect = pduel->new_effect();
				new_effect->id = peffect->id;
				new_effect->owner = this;
				new_effect->handler = this;
				new_effect->type = EFFECT_TYPE_SINGLE;
				new_effect->code = EFFECT_SET_CONTROL;
				new_effect->value = current.controler;
				new_effect->flag = EFFECT_FLAG_CANNOT_DISABLE;
				new_effect->reset_flag = RESET_EVENT | 0xec0000;
				this->add_effect(new_effect);
			}
		}
	}
	for (auto i = indexer.begin(); i != indexer.end();) {
		auto rm = i++;
		peffect = rm->first;
		auto it = rm->second;
		if (peffect->reset(id, reset_type))
			remove_effect(peffect, it);
	}
}

/*
 * ?
 */
void card::reset_effect_count() {
	for (auto i = indexer.begin(); i != indexer.end(); ++i) {
		effect* peffect = i->first;
		if (peffect->flag & EFFECT_FLAG_COUNT_LIMIT)
			peffect->recharge();
	}
}

/*
 * ?
 */
int32 card::refresh_disable_status() {
	int32 pre_dis = is_status(STATUS_DISABLED);
	filter_immune_effect();
	if (!is_affected_by_effect(EFFECT_CANNOT_DISABLE) && is_affected_by_effect(EFFECT_DISABLE))
		set_status(STATUS_DISABLED, TRUE);
	else
		set_status(STATUS_DISABLED, FALSE);
	int32 cur_dis = is_status(STATUS_DISABLED);
	if(pre_dis != cur_dis)
		filter_immune_effect();
	return is_status(STATUS_DISABLED);
}

/*
 * ?
 */
uint8 card::refresh_control_status() {
	uint8 final = owner;
	if(pduel->game_field->core.remove_brainwashing)
		return final;
	effect_set eset;
	filter_effect(EFFECT_SET_CONTROL, &eset);
	if(eset.count)
		final = (uint8) (eset.get_last()->get_value(this, 0));
	return final;
}

/*
 * Chances the turn_counter to ct during a duel
 */
void card::count_turn(uint16 ct) {
	turn_counter = ct;
	// write message
	pduel->write_buffer8(MSG_CARD_HINT);
	pduel->write_buffer32(get_info_location());
	pduel->write_buffer8(CHINT_TURN);
	pduel->write_buffer32(ct);
}

/*
 * Creates a relation initialized with reset, if there was no relation before
 */
void card::create_relation(card* target, uint32 reset) {
	if (relations.find(target) != relations.end()) // if already in a relation don't make a new one
		return;
	relations[target] = reset;
}

/*
 * Creates a relation initialized by an effect.
 */
void card::create_relation(effect* peffect) {
	auto it = relate_effect.find(peffect);
	if (it != relate_effect.end())
		++it->second;
	else
		relate_effect[peffect] = 1;
}

/*
 * Returns True if this card has a relation to another card.
 */
int32 card::is_has_relation(card* target) {
	if (relations.find(target) != relations.end())
		return TRUE;
	return FALSE;
}

/*
 * Returns True if this card has a relation to an effect.
 */
int32 card::is_has_relation(effect* peffect) {
	if (relate_effect.find(peffect) != relate_effect.end())
		return TRUE;
	return FALSE;
}

/*
 * Removes a relation to another card from this card if existent
 */
void card::release_relation(card* target) {
	if (relations.find(target) == relations.end())
		return;
	relations.erase(target);
}

/*
 * Removes a relation to an effect from this card if existent
 */
void card::release_relation(effect* peffect) {
	auto it = relate_effect.find(peffect);
	if (it != relate_effect.end() && --it->second == 0)
		relate_effect.erase(it); // nice
}

/*
 * ?
 */
int32 card::leave_field_redirect(uint32 reason) {
	effect_set es;
	uint32 redirect;
	if(data.type & TYPE_TOKEN)
		return 0;
	filter_effect(EFFECT_LEAVE_FIELD_REDIRECT, &es);
	for(int32 i = 0; i < es.count; ++i) {
		redirect = es[i]->get_value(this, 0);
		if((redirect & LOCATION_HAND) && !is_affected_by_effect(EFFECT_CANNOT_TO_HAND) && pduel->game_field->is_player_can_send_to_hand(current.controler, this))
			return redirect;
		else if((redirect & LOCATION_DECK) && !is_affected_by_effect(EFFECT_CANNOT_TO_DECK) && pduel->game_field->is_player_can_send_to_deck(current.controler, this))
			return redirect;
		else if((redirect & LOCATION_REMOVED) && !is_affected_by_effect(EFFECT_CANNOT_REMOVE) && pduel->game_field->is_player_can_remove(current.controler, this))
			return redirect;
	}
	return 0;
}

/*
 * ?
 */
int32 card::destination_redirect(uint8 destination, uint32 reason) {
	effect_set es;
	uint32 redirect;
	if(data.type & TYPE_TOKEN)
		return 0;
	if(destination == LOCATION_HAND)
		filter_effect(EFFECT_TO_HAND_REDIRECT, &es);
	else if(destination == LOCATION_DECK)
		filter_effect(EFFECT_TO_DECK_REDIRECT, &es);
	else if(destination == LOCATION_GRAVE)
		filter_effect(EFFECT_TO_GRAVE_REDIRECT, &es);
	else if(destination == LOCATION_REMOVED)
		filter_effect(EFFECT_REMOVE_REDIRECT, &es);
	else return 0;
	for(int32 i = 0; i < es.count; ++i) {
		redirect = es[i]->get_value(this, 0);
		if((redirect & LOCATION_HAND) && !is_affected_by_effect(EFFECT_CANNOT_TO_HAND) && pduel->game_field->is_player_can_send_to_hand(current.controler, this))
			return redirect;
		if((redirect & LOCATION_DECK) && !is_affected_by_effect(EFFECT_CANNOT_TO_DECK) && pduel->game_field->is_player_can_send_to_deck(current.controler, this))
			return redirect;
		if((redirect & LOCATION_REMOVED) && !is_affected_by_effect(EFFECT_CANNOT_REMOVE) && pduel->game_field->is_player_can_remove(current.controler, this))
			return redirect;
	}
	return 0;
}

/*
 * Adds count number of conters of type countertype to this card.
 */
int32 card::add_counter(uint8 playerid, uint16 countertype, uint16 count) {
	if(!is_can_add_counter(playerid, countertype, count))
		return FALSE;
	counters[countertype] += count;
	pduel->write_buffer8(MSG_ADD_COUNTER);
	pduel->write_buffer16(countertype);
	pduel->write_buffer8(current.controler);
	pduel->write_buffer8(current.location);
	pduel->write_buffer8(current.sequence);
	pduel->write_buffer8(count);
	return TRUE;
}

/*
 * Remove count number of conters of type countertype to this card,
 * if there are enough on this card (then returns True, else False)
 */
int32 card::remove_counter(uint16 countertype, uint16 count) {
	counter_map::iterator cmit = counters.find(countertype);
	if(cmit == counters.end())
		return FALSE;
	if(cmit->second <= count)
		counters.erase(cmit);
	else cmit->second -= count;
	pduel->write_buffer8(MSG_REMOVE_COUNTER);
	pduel->write_buffer16(countertype);
	pduel->write_buffer8(current.controler);
	pduel->write_buffer8(current.location);
	pduel->write_buffer8(current.sequence);
	pduel->write_buffer8(count);
	return TRUE;
}

/*
 * Checks if a specific number of counters of a certain type can be added to this card.
 */
int32 card::is_can_add_counter(uint8 playerid, uint16 countertype, uint16 count) {
	effect_set eset;
	if(!pduel->game_field->is_player_can_place_counter(playerid, this, countertype, count))
		return FALSE; // ?
	/*
	 * Here the assumption is made that only cards face-up on the field can recieve counters.
	 */	
	if(!(current.location & LOCATION_ONFIELD) || !is_position(POS_FACEUP))
		return FALSE; 
	if((countertype & COUNTER_NEED_ENABLE) && is_status(STATUS_DISABLED))
		return FALSE; // if the counters can't be placed due to an effect
	if((countertype & COUNTER_NEED_PERMIT) && !is_affected_by_effect(EFFECT_COUNTER_PERMIT + (countertype & 0xffff)))
		return FALSE; // ?
	int32 limit = -1;
	int32 cur = 0;
	counter_map::iterator cmit = counters.find(countertype);
	if(cmit != counters.end())
		cur = cmit->second;
	filter_effect(EFFECT_COUNTER_LIMIT + countertype, &eset);
	for(int32 i = 0; i < eset.count; ++i)
		limit = eset[i]->get_value();
	if(limit > 0 && (cur + count > limit))
		return FALSE;
	return TRUE;
}

/*
 * Return number of type countertype on this card.
 */
int32 card::get_counter(uint16 countertype) {
	counter_map::iterator cmit = counters.find(countertype);
	if(cmit == counters.end())
		return 0;
	return cmit->second;
}

/*
 * ??
 */
void card::set_material(card_set* materials) {
	material_cards = *materials;
	card_set::iterator cit;
	for(cit = material_cards.begin(); cit != material_cards.end(); ++cit)
		(*cit)->current.reason_card = this;
	effect_set eset;
	filter_effect(EFFECT_MATERIAL_CHECK, &eset);
	for(int i = 0; i < eset.count; ++i) {
		eset[i]->get_value(this);
	}
}

/*
 * The specified card will be added to the cards affected by this card
 * and this card will be added to the cards affecting the specified card
 */
void card::add_card_target(card* pcard) {
	effect_target_cards.insert(pcard);
	pcard->effect_target_owner.insert(this);
	pduel->write_buffer8(MSG_CARD_TARGET);
	pduel->write_buffer32(get_info_location());
	pduel->write_buffer32(pcard->get_info_location());
}

/*
 * If the specified card is linked to this card by targeting,
 * remove the links (see add_card_target)
 */
void card::cancel_card_target(card* pcard) {
	auto cit = effect_target_cards.find(pcard);
	if(cit != effect_target_cards.end()) {
		effect_target_cards.erase(cit);
		pcard->effect_target_owner.erase(this);
		pduel->write_buffer8(MSG_CANCEL_TARGET);
		pduel->write_buffer32(get_info_location());
		pduel->write_buffer32(pcard->get_info_location());
	}
}

/*
 * ?
 */
void card::filter_effect(int32 code, effect_set* eset, uint8 sort) {
	effect* peffect;
	auto rg = single_effect.equal_range(code);
	for (; rg.first != rg.second; ++rg.first) {
		peffect = rg.first->second;
		if (peffect->is_available() && (!(peffect->flag & EFFECT_FLAG_SINGLE_RANGE) || is_affect_by_effect(peffect)))
			eset->add_item(peffect);
	}
	for (auto cit = equiping_cards.begin(); cit != equiping_cards.end(); ++cit) {
		rg = (*cit)->equip_effect.equal_range(code);
		for (; rg.first != rg.second; ++rg.first) {
			peffect = rg.first->second;
			if (peffect->is_available() && is_affect_by_effect(peffect))
				eset->add_item(peffect);
		}
	}
	rg = pduel->game_field->effects.aura_effect.equal_range(code);
	for (; rg.first != rg.second; ++rg.first) {
		peffect = rg.first->second;
		if (!(peffect->flag & EFFECT_FLAG_PLAYER_TARGET) && peffect->is_available()
			&& peffect->is_target(this) && is_affect_by_effect(peffect))
			eset->add_item(peffect);
	}
	if(sort)
		eset->sort();
}

/*
 * ?
 */
void card::filter_single_continuous_effect(int32 code, effect_set* eset, uint8 sort) {
	auto rg = single_effect.equal_range(code);
	for (; rg.first != rg.second; ++rg.first)
		eset->add_item(rg.first->second);
	for (auto cit = equiping_cards.begin(); cit != equiping_cards.end(); ++cit) {
		rg = (*cit)->equip_effect.equal_range(code);
		for (; rg.first != rg.second; ++rg.first)
			eset->add_item(rg.first->second);
	}
	if(sort)
		eset->sort();
}

/*
 * ?
 */
void card::filter_immune_effect() {
	effect* peffect;
	immune_effect.clear();
	auto rg = single_effect.equal_range(EFFECT_IMMUNE_EFFECT);
	for (; rg.first != rg.second; ++rg.first) {
		peffect = rg.first->second;
		if (peffect->is_available())
			immune_effect.add_item(peffect);
	}
	for (auto cit = equiping_cards.begin(); cit != equiping_cards.end(); ++cit) {
		rg = (*cit)->equip_effect.equal_range(EFFECT_IMMUNE_EFFECT);
		for (; rg.first != rg.second; ++rg.first) {
			peffect = rg.first->second;
			if (peffect->is_available())
				immune_effect.add_item(peffect);
		}
	}
	rg = pduel->game_field->effects.aura_effect.equal_range(EFFECT_IMMUNE_EFFECT);
	for (; rg.first != rg.second; ++rg.first) {
		peffect = rg.first->second;
		if (peffect->is_target(this) && peffect->is_available())
			immune_effect.add_item(peffect);
	}
	immune_effect.sort();
}

/*
 * ?
 */
void card::filter_disable_related_cards() {
	for (auto it = indexer.begin(); it != indexer.end(); ++it) {
		effect* peffect = it->first;
		if (peffect->is_disable_related()) {
			if (peffect->type & EFFECT_TYPE_FIELD)
				pduel->game_field->update_disable_check_list(peffect);
			else if ((peffect->type & EFFECT_TYPE_EQUIP) && equiping_target)
				pduel->game_field->add_to_disable_check_list(equiping_target);
		}
	}
}

/*
 * ?
 */
int32 card::filter_summon_procedure(uint8 playerid, effect_set* peset, uint8 ignore_count) {
	effect_set eset;
	effect* proc;
	filter_effect(EFFECT_LIMIT_SUMMON_PROC, &eset);
	if(eset.count > 1)
		return FALSE;
	if(eset.count) {
		proc = eset[0];
		if(proc->check_count_limit(playerid) && is_summonable(proc) && pduel->game_field->is_player_can_summon(proc->get_value(this), playerid, this)) {
			peset->add_item(eset[0]);
			return -1;
		}
		return -2;
	}
	eset.clear();
	filter_effect(EFFECT_SUMMON_PROC, &eset);
	for(int32 i = 0; i < eset.count; ++i)
		if(eset[i]->check_count_limit(playerid) && is_summonable(eset[i]) && pduel->game_field->is_player_can_summon(eset[i]->get_value(this), playerid, this))
			peset->add_item(eset[i]);
	if(!pduel->game_field->is_player_can_summon(SUMMON_TYPE_NORMAL, playerid, this))
		return FALSE;
	int32 rcount = get_summon_tribute_count();
	int32 min = rcount & 0xffff, max = (rcount >> 16) & 0xffff;
	if(min > 0 && !pduel->game_field->is_player_can_summon(SUMMON_TYPE_ADVANCE, playerid, this))
		return FALSE;
	int32 fcount = pduel->game_field->get_useable_count(current.controler, LOCATION_MZONE, current.controler, LOCATION_REASON_TOFIELD);
	if(max <= -fcount)
		return FALSE;
	if(min < -fcount + 1)
		min = -fcount + 1;
	if(min == 0)
		return TRUE;
	int32 m = pduel->game_field->get_summon_release_list(this, 0, 0, 0);
	if(m >= min)
		return TRUE;
	return FALSE;
}

/*
 * ?
 */
int32 card::filter_set_procedure(uint8 playerid, effect_set* peset, uint8 ignore_count) {
	effect_set eset;
	effect* proc;
	filter_effect(EFFECT_LIMIT_SET_PROC, &eset);
	if(eset.count > 1)
		return FALSE;
	if(eset.count) {
		proc = eset[0];
		if(proc->check_count_limit(playerid) && is_summonable(proc) && pduel->game_field->is_player_can_mset(proc->get_value(this), playerid, this)) {
			peset->add_item(eset[0]);
			return -1;
		}
		return -2;
	}
	eset.clear();
	filter_effect(EFFECT_SET_PROC, &eset);
	for(int32 i = 0; i < eset.count; ++i)
		if(eset[i]->check_count_limit(playerid) && is_summonable(eset[i]) && pduel->game_field->is_player_can_mset(eset[i]->get_value(this), playerid, this))
			peset->add_item(eset[i]);
	if(!pduel->game_field->is_player_can_mset(SUMMON_TYPE_NORMAL, playerid, this))
		return FALSE;
	int32 rcount = get_set_tribute_count();
	int32 min = rcount & 0xffff, max = (rcount >> 16) & 0xffff;
	if(min > 0 && !pduel->game_field->is_player_can_mset(SUMMON_TYPE_ADVANCE, playerid, this))
		return FALSE;
	int32 fcount = pduel->game_field->get_useable_count(current.controler, LOCATION_MZONE, current.controler, LOCATION_REASON_TOFIELD);
	if(max <= -fcount)
		return FALSE;
	if(min < -fcount + 1)
		min = -fcount + 1;
	if(min == 0)
		return TRUE;
	int32 m = pduel->game_field->get_summon_release_list(this, 0, 0, 0);
	if(m >= min)
		return TRUE;
	return FALSE;
}

/*
 * ?
 */
void card::filter_spsummon_procedure(uint8 playerid, effect_set* peset) {
	auto pr = field_effect.equal_range(EFFECT_SPSUMMON_PROC);
	uint8 toplayer;
	uint8 topos;
	effect* peffect;
	for(; pr.first != pr.second; ++pr.first) {
		peffect = pr.first->second;
		if(peffect->flag & EFFECT_FLAG_SPSUM_PARAM) {
			topos = peffect->s_range;
			if(peffect->o_range == 0)
				toplayer = playerid;
			else
				toplayer = 1 - playerid;
		} else {
			topos = POS_FACEUP;
			toplayer = playerid;
		}
		if(peffect->is_available() && peffect->check_count_limit(playerid) && is_summonable(peffect)
			&& pduel->game_field->is_player_can_spsummon(peffect, peffect->get_value(this), topos, playerid, toplayer, this))
			peset->add_item(pr.first->second);
	}
}

/*
 * ?
 */
void card::filter_spsummon_procedure_g(uint8 playerid, effect_set* peset) {
	auto pr = field_effect.equal_range(EFFECT_SPSUMMON_PROC_G);
	for(; pr.first != pr.second; ++pr.first) {
		effect* peffect = pr.first->second;
		if(!peffect->is_available())
			continue;
		effect* oreason = pduel->game_field->core.reason_effect;
		uint8 op = pduel->game_field->core.reason_player;
		pduel->game_field->core.reason_effect = peffect;
		pduel->game_field->core.reason_player = this->current.controler;
		pduel->game_field->save_lp_cost();
		pduel->lua->add_param(peffect, PARAM_TYPE_EFFECT);
		pduel->lua->add_param(this, PARAM_TYPE_CARD);
		if(pduel->lua->check_condition(peffect->condition, 2))
			peset->add_item(pr.first->second);
		pduel->game_field->restore_lp_cost();
		pduel->game_field->core.reason_effect = oreason;
		pduel->game_field->core.reason_player = op;
	}
}

/*
 * ?
 */
effect* card::is_affected_by_effect(int32 code) {
	effect* peffect;
	auto rg = single_effect.equal_range(code);
	for (; rg.first != rg.second; ++rg.first) {
		peffect = rg.first->second;
		if (peffect->is_available() && (!(peffect->flag & EFFECT_FLAG_SINGLE_RANGE) || is_affect_by_effect(peffect)))
			return peffect;
	}
	for (auto cit = equiping_cards.begin(); cit != equiping_cards.end(); ++cit) {
		rg = (*cit)->equip_effect.equal_range(code);
		for (; rg.first != rg.second; ++rg.first) {
			peffect = rg.first->second;
			if (peffect->is_available() && is_affect_by_effect(peffect))
				return peffect;
		}
	}
	rg = pduel->game_field->effects.aura_effect.equal_range(code);
	for (; rg.first != rg.second; ++rg.first) {
		peffect = rg.first->second;
		if (!(peffect->flag & EFFECT_FLAG_PLAYER_TARGET) && peffect->is_available()
			&& peffect->is_target(this) && is_affect_by_effect(peffect))
			return peffect;
	}
	return 0;
}

/*
 * ?
 */
effect* card::is_affected_by_effect(int32 code, card* target) {
	effect* peffect;
	auto rg = single_effect.equal_range(code);
	for (; rg.first != rg.second; ++rg.first) {
		peffect = rg.first->second;
		if (peffect->is_available() && (!(peffect->flag & EFFECT_FLAG_SINGLE_RANGE) || is_affect_by_effect(peffect))
			&& peffect->get_value(target))
			return peffect;
	}
	for (auto cit = equiping_cards.begin(); cit != equiping_cards.end(); ++cit) {
		rg = (*cit)->equip_effect.equal_range(code);
		for (; rg.first != rg.second; ++rg.first) {
			peffect = rg.first->second;
			if (peffect->is_available() && is_affect_by_effect(peffect) && peffect->get_value(target))
				return peffect;
		}
	}
	rg = pduel->game_field->effects.aura_effect.equal_range(code);
	for (; rg.first != rg.second; ++rg.first) {
		peffect = rg.first->second;
		if (!(peffect->flag & EFFECT_FLAG_PLAYER_TARGET) && peffect->is_available()
			&& peffect->is_target(this) && is_affect_by_effect(peffect) && peffect->get_value(target))
			return peffect;
	}
	return 0;
}

/*
 * ?
 */
effect* card::check_equip_control_effect() {
	effect* ret_effect = 0;
	for (auto cit = equiping_cards.begin(); cit != equiping_cards.end(); ++cit) {
		auto rg = (*cit)->equip_effect.equal_range(EFFECT_SET_CONTROL);
		for (; rg.first != rg.second; ++rg.first) {
			effect* peffect = rg.first->second;
			if(!ret_effect || peffect->id > ret_effect->id)
				ret_effect = peffect;
		}
	}
	return ret_effect;
}

/*
 * ?
 */
int32 card::fusion_check(group* fusion_m, card* cg, int32 chkf) {
	auto ecit = single_effect.find(EFFECT_FUSION_MATERIAL);
	if(ecit == single_effect.end())
		return FALSE;
	effect* peffect = ecit->second;
	if(!peffect->condition)
		return FALSE;
	pduel->lua->add_param(peffect, PARAM_TYPE_EFFECT);
	pduel->lua->add_param(fusion_m, PARAM_TYPE_GROUP);
	pduel->lua->add_param(cg, PARAM_TYPE_CARD);
	pduel->lua->add_param(chkf, PARAM_TYPE_INT);
	return pduel->lua->check_condition(peffect->condition, 4);
}

/*
 * ?
 */
void card::fusion_select(uint8 playerid, group* fusion_m, card* cg, int32 chkf) {
	effect* peffect = 0;
	auto ecit = single_effect.find(EFFECT_FUSION_MATERIAL);
	if(ecit != single_effect.end())
		peffect = ecit->second;
	pduel->game_field->add_process(PROCESSOR_SELECT_FUSION, 0, peffect, fusion_m, playerid + (chkf << 16), (ptr)cg);
}

/*
 * ?
 */
int32 card::is_equipable(card* pcard) {
	effect_set eset;
	/*
	 * Here the assumption is made that only cards in the monster card zone can be equipped
	 * and that no card can equip itself (which is an interesting thought)
	 */
	if(this == pcard || pcard->current.location != LOCATION_MZONE) 
		return FALSE;
	filter_effect(EFFECT_EQUIP_LIMIT, &eset); // ?
	if(eset.count == 0)
		return FALSE;
	for(int32 i = 0; i < eset.count; ++i)
		if(eset[i]->get_value(pcard))
			return TRUE;
	return FALSE;
}

/*
 * Returns if this card can be summoned.
 * I guess that includes trap monsters from the moment their effect activates 
 * and they are summoned directly after.
 */
int32 card::is_summonable() {
	if(!(data.type & TYPE_MONSTER)) // only monster can be summoned
		return FALSE;
	// monster must not be unsummonable and have no revive limit (?)
	return !(status & (STATUS_REVIVE_LIMIT | STATUS_UNSUMMONABLE_CARD));
}

/*
 * ?
 */
int32 card::is_summonable(effect* peffect) {
	effect* oreason = pduel->game_field->core.reason_effect;
	uint8 op = pduel->game_field->core.reason_player;
	pduel->game_field->core.reason_effect = peffect;
	pduel->game_field->core.reason_player = this->current.controler;
	pduel->game_field->save_lp_cost();
	pduel->lua->add_param(peffect, PARAM_TYPE_EFFECT);
	pduel->lua->add_param(this, PARAM_TYPE_CARD);
	uint32 result = FALSE;
	if(pduel->game_field->core.limit_tuner || pduel->game_field->core.limit_syn) {
		pduel->lua->add_param(pduel->game_field->core.limit_tuner, PARAM_TYPE_CARD);
		pduel->lua->add_param(pduel->game_field->core.limit_syn, PARAM_TYPE_GROUP);
		if(pduel->lua->check_condition(peffect->condition, 4))
			result = TRUE;
	} else if(pduel->game_field->core.limit_xyz) {
		pduel->lua->add_param(pduel->game_field->core.limit_xyz, PARAM_TYPE_GROUP);
		if(pduel->lua->check_condition(peffect->condition, 3))
			result = TRUE;
	} else {
		if(pduel->lua->check_condition(peffect->condition, 2))
			result = TRUE;
	}
	pduel->game_field->restore_lp_cost();
	pduel->game_field->core.reason_effect = oreason;
	pduel->game_field->core.reason_player = op;
	return result;
}

/*
 * Returns if the card can be summoned.
 */
int32 card::is_can_be_summoned(uint8 playerid, uint8 ignore_count, effect* peffect) {
	if(!is_summonable()) // if the card is summonable in the first place (e.g. Black Hole is not) (?)
		return FALSE;
	if(pduel->game_field->check_unique_onfield(this, playerid)) // another one of unique cards can't be summoned
		return FALSE;
	if(!ignore_count && (pduel->game_field->core.extra_summon[playerid] || !is_affected_by_effect(EFFECT_EXTRA_SUMMON_COUNT))
		&& pduel->game_field->core.summon_count[playerid] >= pduel->game_field->get_summon_count_limit(playerid))
		return FALSE; // probably if there is still a summon left to make.
	if(is_affected_by_effect(EFFECT_FORBIDDEN)) // if can't be summoned by some effect
		return FALSE;
	pduel->game_field->save_lp_cost();
	effect_set eset;
	filter_effect(EFFECT_SUMMON_COST, &eset);
	for(int32 i = 0; i < eset.count; ++i) {
		pduel->lua->add_param(eset[i], PARAM_TYPE_EFFECT);
		pduel->lua->add_param(this, PARAM_TYPE_CARD);
		pduel->lua->add_param(playerid, PARAM_TYPE_INT);
		if(!pduel->lua->check_condition(eset[i]->cost, 3)) {
			pduel->game_field->restore_lp_cost();
			return FALSE;
		}
	}
	if(current.location == LOCATION_MZONE) {
		if(is_position(POS_FACEDOWN)
			|| !is_affected_by_effect(EFFECT_DUAL_SUMMONABLE)
			|| is_affected_by_effect(EFFECT_DUAL_STATUS)
			|| !pduel->game_field->is_player_can_summon(SUMMON_TYPE_DUAL, playerid, this)
			|| is_affected_by_effect(EFFECT_CANNOT_SUMMON)) {
			pduel->game_field->restore_lp_cost();
			return FALSE;
		}
	} else if(current.location == LOCATION_HAND) {
		if(is_status(STATUS_REVIVE_LIMIT) || is_affected_by_effect(EFFECT_CANNOT_SUMMON)) {
			pduel->game_field->restore_lp_cost();
			return FALSE;
		}
		effect_set proc;
		int32 res = filter_summon_procedure(playerid, &proc, ignore_count);
		if((peffect && res < 0) || (!peffect && (!res || res == -2) && !proc.count)
			|| (peffect && (proc.count == 0) && !pduel->game_field->is_player_can_summon(peffect->get_value(), playerid, this))) {
			pduel->game_field->restore_lp_cost();
			return FALSE;
		}
	}
	pduel->game_field->restore_lp_cost();
	return TRUE;
}

/*
 * Return the number of monsters that need to be tributed for this card.
 */
int32 card::get_summon_tribute_count() {
	int32 min = 0, max = 0;
	int32 minul = 0, maxul = 0;
	int32 level = get_level();
	// normal tribute count
	if(level < 5)
		return 0;
	else if(level < 7)
		min = max = 1;
	else
		min = max = 2;
	// effects ..?
	effect_set eset;
	filter_effect(EFFECT_DECREASE_TRIBUTE, &eset);
	for(int32 i = 0; i < eset.count; ++i) {
		int32 dec = eset[i]->get_value(this);
		if(!(eset[i]->flag & EFFECT_FLAG_COUNT_LIMIT)) {
			if(minul < (dec & 0xffff))
				minul = dec & 0xffff;
			if(maxul < (dec >> 16))
				maxul = dec >> 16;
		} else if((eset[i]->reset_count & 0xf00) > 0) {
			min -= dec & 0xffff;
			max -= dec >> 16;
		}
	}
	min -= minul;
	max -= maxul;
	if(min < 0) min = 0;
	if(max < min) max = min;
	return min + (max << 16);
}

/*
 * Return the number of monsters that need to be tributed for this card to be set.
 */
int32 card::get_set_tribute_count() {
	int32 min = 0, max = 0;
	int32 level = get_level();
	// normal tribute count
	if(level < 5)
		return 0;
	else if(level < 7)
		min = max = 1;
	else
		min = max = 2;
	// effects ..?
	effect_set eset;
	filter_effect(EFFECT_DECREASE_TRIBUTE_SET, &eset);
	if(eset.count) {
		int32 dec = eset.get_last()->get_value(this);
		min -= dec & 0xffff;
		max -= dec >> 16;
	}
	if(min < 0) min = 0;
	if(max < min) max = min;
	return min + (max << 16);
}

/*
 * Return if the card can be flip summoned.
 */
int32 card::is_can_be_flip_summoned(uint8 playerid) {
	if(is_status(STATUS_SUMMON_TURN) || is_status(STATUS_FORM_CHANGED))
		return FALSE; // not in the turn of summon and ? 
	if(announce_count > 0) // ?
		return FALSE;
	if(current.location != LOCATION_MZONE) // only monsters can be flip summoned
		return FALSE;
	if(!(current.position & POS_FACEDOWN)) // face-up monsters can't be flipped
		return FALSE;
	if(pduel->game_field->check_unique_onfield(this, playerid)) // if there's already on there and unique, you can't flip this one
		return FALSE;
	if(!pduel->game_field->is_player_can_flipsummon(playerid, this)) // can player even flip ?
		return FALSE;
	if(is_affected_by_effect(EFFECT_FORBIDDEN)) // flip forbidden by effect (?)
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_FLIP_SUMMON)) // flip forbidden by effect
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_CHANGE_POSITION)) // flip not possible due to position changing constraint
		return FALSE;
	pduel->game_field->save_lp_cost(); // ??
	effect_set eset;
	filter_effect(EFFECT_FLIPSUMMON_COST, &eset);
	for(int32 i = 0; i < eset.count; ++i) {
		pduel->lua->add_param(eset[i], PARAM_TYPE_EFFECT);
		pduel->lua->add_param(this, PARAM_TYPE_CARD);
		pduel->lua->add_param(playerid, PARAM_TYPE_INT);
		if(!pduel->lua->check_condition(eset[i]->cost, 3)) {
			pduel->game_field->restore_lp_cost();
			return FALSE;
		}
	}
	pduel->game_field->restore_lp_cost();
	return TRUE;
}

/*
 * Returns if the card can be special summoned.
 */
int32 card::is_special_summonable(uint8 playerid) {
	if(!(data.type & TYPE_MONSTER)) // card must be kind of monster
		return FALSE;
	if(pduel->game_field->check_unique_onfield(this, playerid)) // another one of unique cards can't be summoned 
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_SPECIAL_SUMMON)) // special summon not possible due to an effect
		return FALSE;
	if(is_affected_by_effect(EFFECT_FORBIDDEN)) // summon of this specific card forbidden by effect
		return FALSE;
	if(current.location & (LOCATION_GRAVE + LOCATION_REMOVED) && is_status(STATUS_REVIVE_LIMIT) && !is_status(STATUS_PROC_COMPLETE)) // something about the card being able to be special summoned from the grave / from the remove
		return FALSE;
	// effects ?
	pduel->game_field->save_lp_cost();
	effect_set eset;
	filter_effect(EFFECT_SPSUMMON_COST, &eset);
	for(int32 i = 0; i < eset.count; ++i) {
		pduel->lua->add_param(eset[i], PARAM_TYPE_EFFECT);
		pduel->lua->add_param(this, PARAM_TYPE_CARD);
		pduel->lua->add_param(playerid, PARAM_TYPE_INT);
		if(!pduel->lua->check_condition(eset[i]->cost, 3)) {
			pduel->game_field->restore_lp_cost();
			return FALSE;
		}
	}
	eset.clear();
	// ??
	filter_spsummon_procedure(playerid, &eset);
	pduel->game_field->core.limit_tuner = 0;
	pduel->game_field->core.limit_xyz = 0;
	pduel->game_field->core.limit_syn = 0;
	pduel->game_field->restore_lp_cost();
	return eset.count;
}

/*
 * Returns if the card can be special summoned. I don't get the difference. More parameters ?
 */
int32 card::is_can_be_special_summoned(effect * reason_effect, uint32 sumtype, uint8 sumpos, uint8 sumplayer, uint8 toplayer, uint8 nocheck, uint8 nolimit) {
	// monsters can't be summoned again if they are still on the filed (at least as special summon)
	if(current.location == LOCATION_MZONE) 
		return FALSE;
	// face-down removed monster can't be special summoned
	if(current.location == LOCATION_REMOVED && (current.position & POS_FACEDOWN)) 
		return FALSE;
	if(is_status(STATUS_REVIVE_LIMIT) && !is_status(STATUS_PROC_COMPLETE)) { // ?
		// that are grave | removed | szone and hand | deck
		if((!nolimit && (current.location & 0x38)) || (!nocheck && (current.location & 0x3))) 
			return FALSE;
	}
	// if special summoning is not happening face-down, check if the card is unique and already on the field
	if(((sumpos & POS_FACEDOWN) == 0) && pduel->game_field->check_unique_onfield(this, toplayer)) 
		return FALSE;
	sumtype |= SUMMON_TYPE_SPECIAL;
	if ((sumplayer == 0 || sumplayer == 1) && !pduel->game_field->is_player_can_spsummon(reason_effect, sumtype, sumpos, sumplayer, toplayer, this)) // ?
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_SPECIAL_SUMMON)) // if special summon is not possible due to an effect
		return FALSE;
	// ?
	pduel->game_field->save_lp_cost();
	effect_set eset;
	filter_effect(EFFECT_SPSUMMON_COST, &eset);
	for(int32 i = 0; i < eset.count; ++i) {
		pduel->lua->add_param(eset[i], PARAM_TYPE_EFFECT);
		pduel->lua->add_param(this, PARAM_TYPE_CARD);
		pduel->lua->add_param(sumplayer, PARAM_TYPE_INT);
		if(!pduel->lua->check_condition(eset[i]->cost, 3)) {
			pduel->game_field->restore_lp_cost();
			return FALSE;
		}
	}
	if(!nocheck) {
		eset.clear();
		if(!(data.type & TYPE_MONSTER)) {
			pduel->game_field->restore_lp_cost();
			return FALSE;
		}
		filter_effect(EFFECT_SPSUMMON_CONDITION, &eset);
		for(int32 i = 0; i < eset.count; ++i) {
			pduel->lua->add_param(reason_effect, PARAM_TYPE_EFFECT);
			pduel->lua->add_param(sumplayer, PARAM_TYPE_INT);
			pduel->lua->add_param(sumtype, PARAM_TYPE_INT);
			pduel->lua->add_param(sumpos, PARAM_TYPE_INT);
			pduel->lua->add_param(toplayer, PARAM_TYPE_INT);
			if(!eset[i]->check_value_condition(5)) {
				pduel->game_field->restore_lp_cost();
				return FALSE;
			}
		}
	}
	pduel->game_field->restore_lp_cost();
	return TRUE;
}

/*
 * Returns if the card can be set in the monster card zone.
 */
int32 card::is_setable_mzone(uint8 playerid, uint8 ignore_count, effect* peffect) {
	if(!(data.type & TYPE_MONSTER)) // of course only monsters
		return FALSE;
	// if not summonable at all, set is also out of question
	if(status & (STATUS_REVIVE_LIMIT | STATUS_UNSUMMONABLE_CARD)) 
		return FALSE;
	if(current.location != LOCATION_HAND) // only setable from the hand
		return FALSE;
	if(is_affected_by_effect(EFFECT_FORBIDDEN)) // if forbidden, can't be set
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_MSET)) // if setting is especially forbidden
		return FALSE;
	if(!ignore_count && (pduel->game_field->core.extra_summon[playerid] || !is_affected_by_effect(EFFECT_EXTRA_SET_COUNT))
		&& pduel->game_field->core.summon_count[playerid] >= pduel->game_field->get_summon_count_limit(playerid))
		return FALSE; // the summon count I guess ?
	pduel->game_field->save_lp_cost();
	effect_set eset;
	filter_effect(EFFECT_MSET_COST, &eset);
	for(int32 i = 0; i < eset.count; ++i) {
		pduel->lua->add_param(eset[i], PARAM_TYPE_EFFECT);
		pduel->lua->add_param(this, PARAM_TYPE_CARD);
		pduel->lua->add_param(playerid, PARAM_TYPE_INT);
		if(!pduel->lua->check_condition(eset[i]->cost, 3)) {
			pduel->game_field->restore_lp_cost();
			return FALSE;
		}
	}
	eset.clear();
	int32 res = filter_set_procedure(playerid, &eset, ignore_count);
	if((peffect && res < 0) || (!peffect && (!res || res == -2) && !eset.count)
		|| (peffect && (eset.count == 0) && !pduel->game_field->is_player_can_mset(peffect->get_value(), playerid, this))) {
		pduel->game_field->restore_lp_cost();
		return FALSE;
	}
	pduel->game_field->restore_lp_cost();
	return TRUE;
}

/*
 * Returns if the card can be set in the spell and trap card zone.
 */
int32 card::is_setable_szone(uint8 playerid, uint8 ignore_fd) {
	if(!(data.type & TYPE_FIELD) && !ignore_fd && pduel->game_field->get_useable_count(current.controler, LOCATION_SZONE, current.controler, LOCATION_REASON_TOFIELD) <= 0) // ?
		return FALSE;
	// a monster that can't specifically be set in the SZone (like artifacts) can't be set at all.
	if(data.type & TYPE_MONSTER && !is_affected_by_effect(EFFECT_MONSTER_SSET))
		return FALSE; 
	if(is_affected_by_effect(EFFECT_FORBIDDEN))  // if card can't specifically be played 
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_SSET)) // if card can specifically not be set
		return FALSE;
	if(!pduel->game_field->is_player_can_sset(playerid, this)) // if the player can specifically not sett
		return FALSE;
	pduel->game_field->save_lp_cost();
	effect_set eset;
	filter_effect(EFFECT_SSET_COST, &eset);
	for(int32 i = 0; i < eset.count; ++i) {
		pduel->lua->add_param(eset[i], PARAM_TYPE_EFFECT);
		pduel->lua->add_param(this, PARAM_TYPE_CARD);
		pduel->lua->add_param(playerid, PARAM_TYPE_INT);
		if(!pduel->lua->check_condition(eset[i]->cost, 3)) {
			pduel->game_field->restore_lp_cost();
			return FALSE;
		}
	}
	pduel->game_field->restore_lp_cost();
	return TRUE;
}

/*
 * Returns if this card is affected by a specified effect.
 */
int32 card::is_affect_by_effect(effect* peffect) {
	if(is_status(STATUS_SUMMONING)) // when card is being summoned, it has spawn protection ?
		return FALSE;
	if(!peffect || (peffect->flag & EFFECT_FLAG_IGNORE_IMMUNE))
		return TRUE; // if there is no effect or the effect ignores immunity, the card is affected
	if(peffect->is_immuned(&immune_effect))
		return FALSE; // ?
	return TRUE; // else it is always affected
}

/*
 * Returns if this card is destructable.
 */
int32 card::is_destructable() {
	if(overlay_target) // xyz material is not destructable
		return FALSE;
	if(current.location & (LOCATION_GRAVE + LOCATION_REMOVED)) // neither are cards in the graveyard or removal zone 
		return FALSE;
	if(is_affected_by_effect(EFFECT_INDESTRUCTABLE)) // not destructable if indestructable by an effect
		return FALSE;
	return TRUE; // else it is
}

/*
 * Returns if this card is destructable by battle.
 */
int32 card::is_destructable_by_battle(card * pcard) {
	if(is_affected_by_effect(EFFECT_INDESTRUCTABLE_BATTLE, pcard))
		return FALSE;
	return TRUE;
}

/*
 * Returns if this card is destructable by effect.
 */
int32 card::is_destructable_by_effect(effect* peffect, uint8 playerid) {
	if(!peffect)
		return TRUE;
	effect_set eset;
	filter_effect(EFFECT_INDESTRUCTABLE_EFFECT, &eset);
	for(int32 i = 0; i < eset.count; ++i) {
		pduel->lua->add_param(peffect, PARAM_TYPE_EFFECT);
		pduel->lua->add_param(playerid, PARAM_TYPE_INT);
		pduel->lua->add_param(this, PARAM_TYPE_CARD);
		if(eset[i]->check_value_condition(3))
			return FALSE;
	}
	return TRUE;
}

/*
 * Returns if this card is removeable.
 */
int32 card::is_removeable(uint8 playerid) {
	if(!pduel->game_field->is_player_can_remove(playerid, this))
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_REMOVE))
		return FALSE;
	return TRUE;
}

/*
 * Returns if this card is removeable as cost.
 */
int32 card::is_removeable_as_cost(uint8 playerid) {
	if(is_affected_by_effect(EFFECT_CANNOT_USE_AS_COST))
		return FALSE; // if not useable as cost
	if(!pduel->game_field->is_player_can_remove(playerid, this))
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_REMOVE))
		return FALSE; // if can't be removed
	return TRUE;
}

/*
 * Returns if this card is releasable by summon what?
 */
int32 card::is_releasable_by_summon(uint8 playerid, card *pcard) {
	if(is_status(STATUS_SUMMONING)) // not if spawn
		return FALSE;
	if(overlay_target) // not if releaseable
		return FALSE;
	if(current.location & (LOCATION_GRAVE + LOCATION_REMOVED))
		return FALSE;
	if(!pduel->game_field->is_player_can_release(playerid, this))
		return FALSE;
	if(is_affected_by_effect(EFFECT_UNRELEASABLE_SUM, pcard))
		return FALSE;
	if(pcard->is_affected_by_effect(EFFECT_TRIBUTE_LIMIT, this))
		return FALSE;
	return TRUE;
}

/*
 * Returns if this card is releasable by nonsummon what?
 */
int32 card::is_releasable_by_nonsummon(uint8 playerid) {
	if(is_status(STATUS_SUMMONING))
		return FALSE;
	if(overlay_target)
		return FALSE;
	if(current.location & (LOCATION_GRAVE + LOCATION_REMOVED))
		return FALSE;
	if((current.location == LOCATION_HAND) && (data.type & (TYPE_SPELL | TYPE_TRAP)))
		return FALSE;
	if(!pduel->game_field->is_player_can_release(playerid, this))
		return FALSE;
	if(is_affected_by_effect(EFFECT_UNRELEASABLE_NONSUM))
		return FALSE;
	return TRUE;
}

/*
 * Returns if this card is releasable by effect what?
 */
int32 card::is_releasable_by_effect(uint8 playerid, effect* peffect) {
	if(!peffect)
		return TRUE;
	effect_set eset;
	filter_effect(EFFECT_UNRELEASABLE_EFFECT, &eset);
	for(int32 i = 0; i < eset.count; ++i) {
		pduel->lua->add_param(peffect, PARAM_TYPE_EFFECT);
		pduel->lua->add_param(playerid, PARAM_TYPE_INT);
		pduel->lua->add_param(this, PARAM_TYPE_CARD);
		if(eset[i]->check_value_condition(3))
			return FALSE;
	}
	return TRUE;
}

/*
 * Returns if this card is capable to send to grave ??
 */
int32 card::is_capable_send_to_grave(uint8 playerid) {
	if(is_affected_by_effect(EFFECT_CANNOT_TO_GRAVE))
		return FALSE;
	if(!pduel->game_field->is_player_can_send_to_grave(playerid, this))
		return FALSE;
	return TRUE;
}

/*
 * ?
 */
int32 card::is_capable_send_to_hand(uint8 playerid) {
	if(is_status(STATUS_LEAVE_CONFIRMED))
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_TO_HAND))
		return FALSE;
	if(!pduel->game_field->is_player_can_send_to_hand(playerid, this))
		return FALSE;
	return TRUE;
}

/*
 * ?
 */
int32 card::is_capable_send_to_deck(uint8 playerid) {
	if(is_status(STATUS_LEAVE_CONFIRMED))
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_TO_DECK))
		return FALSE;
	if(!pduel->game_field->is_player_can_send_to_deck(playerid, this))
		return FALSE;
	return TRUE;
}

/*
 * ?
 */
int32 card::is_capable_send_to_extra(uint8 playerid) {
	if(!(data.type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ)))
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_TO_DECK))
		return FALSE;
	if(!pduel->game_field->is_player_can_send_to_deck(playerid, this))
		return FALSE;
	return TRUE;
}

/*
 * ?
 */
int32 card::is_capable_cost_to_grave(uint8 playerid) {
	uint32 redirect = 0;
	uint32 dest = LOCATION_GRAVE;
	if(data.type & TYPE_TOKEN)
		return FALSE;
	if((data.type & TYPE_PENDULUM) && (current.location & LOCATION_ONFIELD))
		return FALSE;
	if(current.location == LOCATION_GRAVE)
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_USE_AS_COST))
		return FALSE;
	if(!is_capable_send_to_grave(playerid))
		return FALSE;
	uint32 op_param = operation_param;
	operation_param = dest << 8;
	if(current.location & LOCATION_ONFIELD)
		redirect = leave_field_redirect(REASON_COST) & 0xffff;
	if(redirect) dest = redirect;
	redirect = destination_redirect(dest, REASON_COST) & 0xffff;
	if(redirect) dest = redirect;
	operation_param = op_param;
	if(dest != LOCATION_GRAVE)
		return FALSE;
	return TRUE;
}

/*
 * ?
 */
int32 card::is_capable_cost_to_hand(uint8 playerid) {
	uint32 redirect = 0;
	uint32 dest = LOCATION_HAND;
	if(data.type & (TYPE_TOKEN | TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ))
		return FALSE;
	if(current.location == LOCATION_HAND)
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_USE_AS_COST))
		return FALSE;
	if(!is_capable_send_to_hand(playerid))
		return FALSE;
	uint32 op_param = operation_param;
	operation_param = dest << 8;
	if(current.location & LOCATION_ONFIELD)
		redirect = leave_field_redirect(REASON_COST) & 0xffff;
	if(redirect) dest = redirect;
	redirect = destination_redirect(dest, REASON_COST) & 0xffff;
	if(redirect) dest = redirect;
	operation_param = op_param;
	if(dest != LOCATION_HAND)
		return FALSE;
	return TRUE;
}

/*
 * ?
 */
int32 card::is_capable_cost_to_deck(uint8 playerid) {
	uint32 redirect = 0;
	uint32 dest = LOCATION_DECK;
	if(data.type & (TYPE_TOKEN | TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ))
		return FALSE;
	if(current.location == LOCATION_DECK)
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_USE_AS_COST))
		return FALSE;
	if(!is_capable_send_to_deck(playerid))
		return FALSE;
	uint32 op_param = operation_param;
	operation_param = dest << 8;
	if(current.location & LOCATION_ONFIELD)
		redirect = leave_field_redirect(REASON_COST) & 0xffff;
	if(redirect) dest = redirect;
	redirect = destination_redirect(dest, REASON_COST) & 0xffff;
	if(redirect) dest = redirect;
	operation_param = op_param;
	if(dest != LOCATION_DECK)
		return FALSE;
	return TRUE;
}

/*
 * ?
 */
int32 card::is_capable_cost_to_extra(uint8 playerid) {
	uint32 redirect = 0;
	uint32 dest = LOCATION_DECK;
	if(!(data.type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ)))
		return FALSE;
	if(current.location == LOCATION_EXTRA)
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_USE_AS_COST))
		return FALSE;
	if(!is_capable_send_to_deck(playerid))
		return FALSE;
	uint32 op_param = operation_param;
	operation_param = dest << 8;
	if(current.location & LOCATION_ONFIELD)
		redirect = leave_field_redirect(REASON_COST) & 0xffff;
	if(redirect) dest = redirect;
	redirect = destination_redirect(dest, REASON_COST) & 0xffff;
	if(redirect) dest = redirect;
	operation_param = op_param;
	if(dest != LOCATION_DECK)
		return FALSE;
	return TRUE;
}

/*
 * ?
 */
int32 card::is_capable_attack() {
	if(!is_position(POS_FACEUP_ATTACK) && !(is_position(POS_FACEUP_DEFENCE) && is_affected_by_effect(EFFECT_DEFENCE_ATTACK)))
		return FALSE;
	if(is_affected_by_effect(EFFECT_FORBIDDEN))
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_ATTACK))
		return FALSE;
	if(is_affected_by_effect(EFFECT_ATTACK_DISABLED))
		return FALSE;
	if(pduel->game_field->is_player_affected_by_effect(pduel->game_field->infos.turn_player, EFFECT_SKIP_BP))
		return FALSE;
	return TRUE;
}

/*
 * ?
 */
int32 card::is_capable_attack_announce(uint8 playerid) {
	if(!is_capable_attack())
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_ATTACK_ANNOUNCE))
		return FALSE;
	pduel->game_field->save_lp_cost();
	effect_set eset;
	pduel->game_field->filter_player_effect(playerid, EFFECT_ATTACK_COST, &eset, FALSE);
	filter_effect(EFFECT_ATTACK_COST, &eset);
	for(int32 i = 0; i < eset.count; ++i) {
		pduel->lua->add_param(eset[i], PARAM_TYPE_EFFECT);
		pduel->lua->add_param(this, PARAM_TYPE_CARD);
		pduel->lua->add_param(playerid, PARAM_TYPE_INT);
		if(!pduel->lua->check_condition(eset[i]->cost, 3)) {
			pduel->game_field->restore_lp_cost();
			return FALSE;
		}
	}
	pduel->game_field->restore_lp_cost();
	return TRUE;
}

/*
 * ?
 */
int32 card::is_capable_change_position(uint8 playerid) {
	if(is_status(STATUS_SUMMON_TURN) || is_status(STATUS_FORM_CHANGED))
		return FALSE;
	if(announce_count > 0)
		return FALSE;
	if(is_affected_by_effect(EFFECT_FORBIDDEN))
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_CHANGE_POSITION))
		return FALSE;
	if(pduel->game_field->is_player_affected_by_effect(playerid, EFFECT_CANNOT_CHANGE_POSITION))
		return FALSE;
	return TRUE;
}

/*
 * ?
 */
int32 card::is_capable_turn_set(uint8 playerid) {
	if(data.type & TYPE_TOKEN)
		return FALSE;
	if(is_position(POS_FACEDOWN))
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_TURN_SET))
		return FALSE;
	if(pduel->game_field->is_player_affected_by_effect(playerid, EFFECT_CANNOT_TURN_SET))
		return FALSE;
	return TRUE;
}

/*
 * ?
 */
int32 card::is_capable_change_control() {
	if(is_affected_by_effect(EFFECT_CANNOT_CHANGE_CONTROL))
		return FALSE;
	return TRUE;
}

/*
 * ?
 */
int32 card::is_control_can_be_changed() {
	if(current.controler == PLAYER_NONE)
		return FALSE;
	if(current.location != LOCATION_MZONE)
		return FALSE;
	if(pduel->game_field->get_useable_count(1 - current.controler, LOCATION_MZONE, current.controler, LOCATION_REASON_CONTROL) <= 0)
		return FALSE;
	if((data.type & TYPE_TRAPMONSTER) && pduel->game_field->get_useable_count(1 - current.controler, LOCATION_MZONE, current.controler, LOCATION_REASON_CONTROL) <= 0)
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_CHANGE_CONTROL))
		return FALSE;
	return TRUE;
}

/*
 * ?
 */
int32 card::is_capable_be_battle_target(card* pcard) {
	if(is_affected_by_effect(EFFECT_CANNOT_BE_BATTLE_TARGET, pcard))
		return FALSE;
	if(is_affected_by_effect(EFFECT_IGNORE_BATTLE_TARGET))
		return FALSE;
	return TRUE;
}

/*
 * ?
 */
int32 card::is_capable_be_effect_target(effect* peffect, uint8 playerid) {
	if(is_status(STATUS_SUMMONING) || is_status(STATUS_BATTLE_DESTROYED))
		return FALSE;
	effect_set eset;
	filter_effect(EFFECT_CANNOT_BE_EFFECT_TARGET, &eset);
	for(int32 i = 0; i < eset.count; ++i) {
		pduel->lua->add_param(playerid, PARAM_TYPE_INT);
		if(eset[i]->get_value(peffect, 1))
			return FALSE;
	}
	return TRUE;
}

/*
 * Checks if the card can be used as fusion material
 */
int32 card::is_can_be_fusion_material(uint8 ignore_mon) {
	if(!ignore_mon && !(get_type() & TYPE_MONSTER))
		return FALSE;
	if(is_affected_by_effect(EFFECT_FORBIDDEN)) // no forbidden cards (not even from the hand?)
		return FALSE;
	if(is_affected_by_effect(EFFECT_CANNOT_BE_FUSION_MATERIAL)) // explicit forbidden as synchro material
		return FALSE;
	return TRUE;
}

/*
 * Checks if the card can be used as synchro material.
 */
int32 card::is_can_be_synchro_material(card* scard, card* tuner) {
	if(data.type & TYPE_XYZ) // no Xzy
		return FALSE;
	if(!(get_type()&TYPE_MONSTER)) // must be a monster
		return FALSE;
	if(scard && current.controler != scard->current.controler && !is_affected_by_effect(EFFECT_SYNCHRO_MATERIAL))
		return FALSE; // ?
	if(is_affected_by_effect(EFFECT_FORBIDDEN)) // if forbidden, but it shouldn't be on the field then?
		return FALSE;
	//special fix for scrap chimera, not perfect yet
	if(tuner && (pduel->game_field->core.global_flag & GLOBALFLAG_SCRAP_CHIMERA)) {
		if(is_affected_by_effect(EFFECT_SCRAP_CHIMERA, tuner))
			return false;
	}
	effect_set eset;
	filter_effect(EFFECT_CANNOT_BE_SYNCHRO_MATERIAL, &eset);
	for(int32 i = 0; i < eset.count; ++i)
		if(eset[i]->get_value(scard))
			return FALSE;
	return TRUE;
}

/*
 * Checks if the card can be used as xyz material.
 */
int32 card::is_can_be_xyz_material(card* scard) {
	if(data.type & (TYPE_XYZ | TYPE_TOKEN)) // no other xyz or token
		return FALSE;
	if(!(get_type()&TYPE_MONSTER)) // only monster
		return FALSE;
	if(is_affected_by_effect(EFFECT_FORBIDDEN)) // if not generally forbidden
		return FALSE;
	effect_set eset;
	filter_effect(EFFECT_CANNOT_BE_XYZ_MATERIAL, &eset); // and not especially forbidden for that purpose
	for(int32 i = 0; i < eset.count; ++i)
		if(eset[i]->get_value(scard))
			return FALSE;
	return TRUE;
}
