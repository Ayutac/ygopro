/*
 * card.h
 * Contains the necessary structures to simulate cards,
 * and constant related to cards (but not effect contants).
 *
 *  Created on: 2010-4-8
 *      Author: Argon
 */

#ifndef CARD_H_
#define CARD_H_

#include "common.h"
#include "effectset.h"
#include <set>
#include <map>

class card;
class duel;
class effect;
class group;

/*
 * the initial data of a card, as printed
 */
struct card_data { 
	uint32 code; // probably the number in the left down corner ?
	uint32 alias; // like Umi and A Legendary Ocean ? but also saved as code because card::get_code() assumes so?
	uint64 setcode; // ?
	uint32 type; // Monster, Spell, Effect Monster etc, see TYPE_constants
	uint32 level; // level of a monster, an int >= 1 (or 0 for Spells etc.) 
	uint32 attribute; // Earth, Water, Divine, etc, see ATTRIBUTE_constants
	uint32 race; // Reptile, Fiend, etc for monsters, see RACE_constants
	int32 attack; // ATK
	int32 defence; // DEF
	uint32 lscale; // probably the left scale for pendulum monsters?
	uint32 rscale; // probably the right scale for pendulum monsters?
};

/*
 * A certain state of a card in a game.
 */
struct card_state { 
	uint32 code;
	uint32 type;
	uint32 level;
	uint32 rank;
	uint32 lscale;
	uint32 rscale;
	uint32 attribute;
	uint32 race;
	int32 attack; // likely to change
	int32 defence; // "
	int32 base_attack; // must be stored, e.g. for megamorph
	int32 base_defence; // "
	uint8 controler; // current controller that is (don't correct the typo, except you do it everywhere)
	uint8 location; // location on the controller's field side, e.g. deck, graveyard etc, see LOCATION_constants
	uint8 sequence; // the exact position within the location (because location is a stack of cards)
	uint8 position; // attack or defense position, face-up/face-down etc, see POSITION_constants
	uint32 reason; // ?
	card* reason_card; // ?
	uint8 reason_player; // ?
	effect* reason_effect; // ?
};

/*
 * The structure for a cache used for querys to get several information about a card at a time.
 */
struct query_cache {
	uint32 code;
	uint32 alias;
	uint32 type;
	uint32 level;
	uint32 rank;
	uint32 attribute;
	uint32 race;
	int32 attack;
	int32 defence;
	int32 base_attack;
	int32 base_defence;
	uint32 reason; // ?
	int32 is_public; // maybe if the information is public ?
	int32 is_disabled; // ?
	uint32 lscale;
	uint32 rscale;
};

/*
 * A representation of a card, that can be used for duels.
 */
class card {
public:
	typedef std::vector<card*> card_vector; // a list of cards
	typedef std::multimap<uint32, effect*> effect_container; // saves a list of effects with uint32 keys
	typedef std::set<card*, card_sort> card_set; // a list of cards with a comparator
	typedef std::map<effect*, effect_container::iterator> effect_indexer; // ?
	typedef std::map<effect*, uint32> effect_relation; // saves effect relations ?
	typedef std::map<card*, uint32> relation_map; // saves relations ?
	typedef std::map<uint16, uint16> counter_map; // Saves all counters on this card. Probably there are uint16 constants for the different counters somewhere ?
	typedef std::map<uint16, card*> attacker_map; // Associates an uint16 with a card to indicate a proposed/done/etc attack. The uint16 is a simple index for the attack ?
	int32 scrtype; // ?
	int32 ref_handle; // ?

	duel* pduel; // the duel this card is in ?
	card_data data; // the printed/original data of the card
	card_state previous; // a previous state of the card ?
	card_state temp; // a temporal state of the card, probably during effect and chain resolutions ? 
	card_state current; // the current state of the card
	query_cache q_cache; // the query cache for card::get_info(...)
	uint8 owner; // the owner of this card, not to be confused with the controller; the owner takes this card home
	uint8 summon_player; // who summoned this card
	uint32 summon_type; // how it was summoned: normal, tribute, etc, see SUMMON_TYPE_constants
	uint32 status; // the status of this card, e.g. if disabled (trap cards with Jinzo on the field), just summoned, etc, see STATUS_constants
	uint32 operation_param; // ?
	uint8 announce_count; // how often attack was annouced ? (can happen multiple times with normal monsters because of relay, e.g. attacking and then Call of the Haunted activated)
	uint8 attacked_count; // how often actually attacked (can happen multiple times with e.g. Asura Priest or Matara the Zapper)
	uint8 attack_all_target; // if this card can attack all of the opponent's monsters ? (once ?) (e.g. Asura Priest)
	uint16 cardid; // probably the ID in the left down corner of the card ? 
	uint32 fieldid; // ? Probably which of the monster or spell/trap places ?
	uint32 fieldid_r; // ?
	uint16 turnid; // ?
	uint16 turn_counter; // on what turn the card came into it's current location/sequence
	uint8 unique_pos[2]; // first entry indicates if this card is to be unique on controller's field. the second entry indicatates if this card is to be unique on the opp. field + this card.
	uint16 unique_uid; // ?
	uint32 unique_code; // ?
	uint8 assume_type; // ?
	uint32 assume_value; // ?
	effect* unique_effect; // ?
	card* equiping_target; // which card this card is equipped to ? // leave the typo or change it everywhere
	card* pre_equip_target; // which card this card was previously equipped to ?
	card* overlay_target; // ?
	relation_map relations; // ?
	counter_map counters; // the current (?) counters on this card
	attacker_map announced_cards; // the announced attacks by this card (in the current turn ?)
	attacker_map attacked_cards; // the started attacks by this card (in the current turn ?)
	attacker_map battled_cards; // the attacks completely done by this card (in the current turn ?)
	card_set equiping_cards; // the cards this card is equipped with
	card_set material_cards; // ?
	card_set effect_target_owner; // all cards that target this card with their effects
	card_set effect_target_cards; // all cards this card targets due to its effect
	card_vector xyz_materials; // the xyz material belonging to this card
	effect_container single_effect; // ?
	effect_container field_effect; // ?
	effect_container equip_effect; // ?
	effect_indexer indexer; // ?
	effect_relation relate_effect; // ?
	effect_set_v immune_effect; // ?

	card(); // constructor
	~card(); // destructor
	static bool card_operation_sort(card* c1, card* c2); // card comparator

	uint32 get_infos(byte* buf, int32 query_flag, int32 use_cache = TRUE); // takes a query and returns corresponding values of this card
	uint32 get_info_location(); // returns complete location (including controller etc)
	uint32 get_code(); // returns the code aka the name of this card, at least the code/card this card is currently treated as (e.g. Cyber Proto Dragon on the field)
	uint32 get_another_code(); // ?
	int32 is_set_card(uint32 set_code); // ?
	uint32 get_type(); // returns the type of the card, see TYPE_constants
	int32 get_base_attack(uint8 swap = FALSE); // swap in the sence of Shield and Sword
	int32 get_attack(uint8 swap = FALSE); 
	int32 get_base_defence(uint8 swap = FALSE); 
	int32 get_defence(uint8 swap = FALSE); 
	uint32 get_level();
	uint32 get_rank(); 
	uint32 get_synchro_level(card* pcard);
	uint32 get_ritual_level(card* pcard);
	uint32 is_xyz_level(card* pcard, uint32 lv);
	uint32 get_attribute(); // returns the attribute of the card, see ATTRIBUTE_constants
	uint32 get_race(); // returns the race of the card, see RACE_constants
	uint32 get_lscale();
	uint32 get_rscale();
	int32 is_position(int32 pos); // returns current.position & pos, see POSITION_constants
	void set_status(uint32 status, int32 enabled); // sets or unsets (enabled == 0) the state of this card bitwise
	int32 get_status(uint32 status); // returns this.status & status, see STATE_constants
	int32 is_status(uint32 status); // returns (this.status & status == status)

	void equip(card *target, uint32 send_msg = TRUE);
	void unequip();
	int32 get_union_count(); // returns the number of cards that are equipped to this card by their union effect
	void xyz_overlay(card_set* materials); // ?
	void xyz_add(card* mat, card_set* des); // adds xyz_material to this card
	void xyz_remove(card* mat); // removes xyz_material to this card
	void apply_field_effect(); // applies a field effect ?
	void cancel_field_effect(); // cancels a field effect ?
	void enable_field_effect(int32 enabled); // enables or disables a field effect ?
	int32 add_effect(effect* peffect); // ?
	void remove_effect(effect* peffect); // ?
	void remove_effect(effect* peffect, effect_container::iterator it); // ?
	int32 copy_effect(uint32 code, uint32 reset, uint32 count); // ?
	void reset(uint32 id, uint32 reset_type); // ?
	void reset_effect_count(); // ?
	int32 refresh_disable_status(); // ?
	uint8 refresh_control_status(); // ?

	void count_turn(uint16 ct); // changes the turn_counter to ct
	void create_relation(card* target, uint32 reset);
	void create_relation(effect* peffect);
	int32 is_has_relation(card* target);
	int32 is_has_relation(effect* peffect);
	void release_relation(card* target);
	void release_relation(effect* peffect);
	int32 leave_field_redirect(uint32 reason);
	int32 destination_redirect(uint8 destination, uint32 reason);
	int32 add_counter(uint8 playerid, uint16 countertype, uint16 count);
	int32 remove_counter(uint16 countertype, uint16 count);
	int32 is_can_add_counter(uint8 playerid, uint16 countertype, uint16 count);
	int32 get_counter(uint16 countertype);
	void set_material(card_set* materials);
	void add_card_target(card* pcard);
	void cancel_card_target(card* pcard);
	
	void filter_effect(int32 code, effect_set* eset, uint8 sort = TRUE);
	void filter_single_continuous_effect(int32 code, effect_set* eset, uint8 sort = TRUE);
	void filter_immune_effect();
	void filter_disable_related_cards();
	int32 filter_summon_procedure(uint8 playerid, effect_set* eset, uint8 ignore_count);
	int32 filter_set_procedure(uint8 playerid, effect_set* eset, uint8 ignore_count);
	void filter_spsummon_procedure(uint8 playerid, effect_set* eset);
	void filter_spsummon_procedure_g(uint8 playerid, effect_set* eset);
	effect* is_affected_by_effect(int32 code);
	effect* is_affected_by_effect(int32 code, card* target);
	effect* check_equip_control_effect();
	int32 fusion_check(group* fusion_m, card* cg, int32 chkf);
	void fusion_select(uint8 playerid, group* fusion_m, card* cg, int32 chkf);
	
	int32 is_equipable(card* pcard);
	int32 is_summonable();
	int32 is_summonable(effect* peffect);
	int32 is_can_be_summoned(uint8 playerid, uint8 ingore_count, effect* peffect);
	int32 get_summon_tribute_count();
	int32 get_set_tribute_count();
	int32 is_can_be_flip_summoned(uint8 playerid);
	int32 is_special_summonable(uint8 playerid);
	int32 is_can_be_special_summoned(effect* reason_effect, uint32 sumtype, uint8 sumpos, uint8 sumplayer, uint8 toplayer, uint8 nocheck, uint8 nolimit);
	int32 is_setable_mzone(uint8 playerid, uint8 ignore_count, effect* peffect);
	int32 is_setable_szone(uint8 playerid, uint8 ignore_fd = 0);
	int32 is_affect_by_effect(effect* peffect);
	int32 is_destructable();
	int32 is_destructable_by_battle(card* pcard);
	int32 is_destructable_by_effect(effect* peffect, uint8 playerid);
	int32 is_removeable(uint8 playerid);
	int32 is_removeable_as_cost(uint8 playerid);
	int32 is_releasable_by_summon(uint8 playerid, card* pcard);
	int32 is_releasable_by_nonsummon(uint8 playerid);
	int32 is_releasable_by_effect(uint8 playerid, effect* peffect);
	int32 is_capable_send_to_grave(uint8 playerid);
	int32 is_capable_send_to_hand(uint8 playerid);
	int32 is_capable_send_to_deck(uint8 playerid);
	int32 is_capable_send_to_extra(uint8 playerid);
	int32 is_capable_cost_to_grave(uint8 playerid);
	int32 is_capable_cost_to_hand(uint8 playerid);
	int32 is_capable_cost_to_deck(uint8 playerid);
	int32 is_capable_cost_to_extra(uint8 playerid);
	int32 is_capable_attack();
	int32 is_capable_attack_announce(uint8 playerid);
	int32 is_capable_change_position(uint8 playerid);
	int32 is_capable_turn_set(uint8 playerid);
	int32 is_capable_change_control();
	int32 is_control_can_be_changed();
	int32 is_capable_be_battle_target(card* pcard);
	int32 is_capable_be_effect_target(effect* peffect, uint8 playerid);
	int32 is_can_be_fusion_material(uint8 ignore_mon = FALSE);
	int32 is_can_be_synchro_material(card* scard, card* tuner = 0);
	int32 is_can_be_xyz_material(card* scard);
};

//Locations
#define LOCATION_DECK		0x01		//
#define LOCATION_HAND		0x02		//
#define LOCATION_MZONE		0x04		//
#define LOCATION_SZONE		0x08		//
#define LOCATION_GRAVE		0x10		//
#define LOCATION_REMOVED	0x20		//
#define LOCATION_EXTRA		0x40		//
#define LOCATION_OVERLAY	0x80		//
#define LOCATION_ONFIELD	0x0c		//

//Positions
#define POS_FACEUP_ATTACK		0x1
#define POS_FACEDOWN_ATTACK		0x2
#define POS_FACEUP_DEFENCE		0x4
#define POS_FACEDOWN_DEFENCE	0x8
#define POS_FACEUP				0x5
#define POS_FACEDOWN			0xa
#define POS_ATTACK				0x3
#define POS_DEFENCE				0xc
#define NO_FLIP_EFFECT			0x10000

//Types
#define TYPE_MONSTER		0x1			//
#define TYPE_SPELL			0x2			//
#define TYPE_TRAP			0x4			//
#define TYPE_NORMAL			0x10		//
#define TYPE_EFFECT			0x20		//
#define TYPE_FUSION			0x40		//
#define TYPE_RITUAL			0x80		//
#define TYPE_TRAPMONSTER	0x100		//
#define TYPE_SPIRIT			0x200		//
#define TYPE_UNION			0x400		//
#define TYPE_DUAL			0x800		//
#define TYPE_TUNER			0x1000		//
#define TYPE_SYNCHRO		0x2000		//
#define TYPE_TOKEN			0x4000		//
#define TYPE_QUICKPLAY		0x10000		//
#define TYPE_CONTINUOUS		0x20000		//
#define TYPE_EQUIP			0x40000		//
#define TYPE_FIELD			0x80000		//
#define TYPE_COUNTER		0x100000	//
#define TYPE_FLIP			0x200000	//
#define TYPE_TOON			0x400000	//
#define TYPE_XYZ			0x800000	//
#define TYPE_PENDULUM		0x1000000	//

//Attributes
#define ATTRIBUTE_EARTH		0x01		//
#define ATTRIBUTE_WATER		0x02		//
#define ATTRIBUTE_FIRE		0x04		//
#define ATTRIBUTE_WIND		0x08		//
#define ATTRIBUTE_LIGHT		0x10		//
#define ATTRIBUTE_DARK		0x20		//
#define ATTRIBUTE_DEVINE	0x40		//

//Races
#define RACE_WARRIOR		0x1			//
#define RACE_SPELLCASTER	0x2			//
#define RACE_FAIRY			0x4			//
#define RACE_FIEND			0x8			//
#define RACE_ZOMBIE			0x10		//
#define RACE_MACHINE		0x20		//
#define RACE_AQUA			0x40		//
#define RACE_PYRO			0x80		//
#define RACE_ROCK			0x100		//
#define RACE_WINDBEAST		0x200		//
#define RACE_PLANT			0x400		//
#define RACE_INSECT			0x800		//
#define RACE_THUNDER		0x1000		//
#define RACE_DRAGON			0x2000		//
#define RACE_BEAST			0x4000		//
#define RACE_BEASTWARRIOR	0x8000		//
#define RACE_DINOSAUR		0x10000		//
#define RACE_FISH			0x20000		//
#define RACE_SEASERPENT		0x40000		//
#define RACE_REPTILE		0x80000		//
#define RACE_PSYCHO			0x100000	//
#define RACE_DEVINE			0x200000	//
#define RACE_CREATORGOD		0x400000	//
#define RACE_PHANTOMDRAGON		0x800000	//

//Reason
#define REASON_DESTROY		0x1		//
#define REASON_RELEASE		0x2		//
#define REASON_TEMPORARY	0x4		//
#define REASON_MATERIAL		0x8		//
#define REASON_SUMMON		0x10	//
#define REASON_BATTLE		0x20	//
#define REASON_EFFECT		0x40	//
#define REASON_COST			0x80	//
#define REASON_ADJUST		0x100	//
#define REASON_LOST_TARGET	0x200	//
#define REASON_RULE			0x400	//
#define REASON_SPSUMMON		0x800	//
#define REASON_DISSUMMON	0x1000	//
#define REASON_FLIP			0x2000	//
#define REASON_DISCARD		0x4000	//
#define REASON_RDAMAGE		0x8000	//
#define REASON_RRECOVER		0x10000	//
#define REASON_RETURN		0x20000	//
#define REASON_FUSION		0x40000	//
#define REASON_SYNCHRO		0x80000	//
#define REASON_RITUAL		0x100000	//
#define REASON_XYZ			0x200000	//
#define REASON_REPLACE		0x1000000	//
#define REASON_DRAW			0x2000000	//
#define REASON_REDIRECT		0x4000000	//

//Summon Type
#define SUMMON_TYPE_NORMAL	0x10000000
#define SUMMON_TYPE_ADVANCE	0x11000000
#define SUMMON_TYPE_DUAL	0x12000000
#define SUMMON_TYPE_FLIP	0x20000000
#define SUMMON_TYPE_SPECIAL	0x40000000
#define SUMMON_TYPE_FUSION	0x43000000
#define SUMMON_TYPE_RITUAL	0x45000000
#define SUMMON_TYPE_SYNCHRO	0x46000000
#define SUMMON_TYPE_XYZ		0x49000000

//Status
#define STATUS_DISABLED				0x0001	//
#define STATUS_TO_ENABLE			0x0002	//
#define STATUS_TO_DISABLE			0x0004	//
#define STATUS_PROC_COMPLETE		0x0008	//
#define STATUS_SET_TURN				0x0010	//
#define STATUS_FLIP_SUMMONED		0x0020	//
#define STATUS_REVIVE_LIMIT			0x0040	//
#define STATUS_ATTACKED				0x0080	//
#define STATUS_FORM_CHANGED			0x0100	//
#define STATUS_SUMMONING			0x0200	//
#define STATUS_EFFECT_ENABLED		0x0400	//
#define STATUS_SUMMON_TURN			0x0800	//
#define STATUS_DESTROY_CONFIRMED	0x1000	//
#define STATUS_LEAVE_CONFIRMED		0x2000	//
#define STATUS_BATTLE_DESTROYED		0x4000	//
#define STATUS_COPYING_EFFECT		0x8000	//
#define STATUS_CHAINING				0x10000	//
#define STATUS_SUMMON_DISABLED		0x20000	//
#define STATUS_ACTIVATE_DISABLED	0x40000	//
#define STATUS_UNSUMMONABLE_CARD	0x80000	//
#define STATUS_UNION				0x100000
#define STATUS_ATTACK_CANCELED		0x200000
#define STATUS_INITIALIZING			0x400000
#define STATUS_ACTIVATED			0x800000
#define STATUS_JUST_POS				0x1000000
#define STATUS_CONTINUOUS_POS		0x2000000
#define STATUS_IS_PUBLIC			0x4000000
#define STATUS_ACT_FROM_HAND		0x8000000

//Counter
#define COUNTER_NEED_PERMIT		0x1000
#define COUNTER_NEED_ENABLE		0x2000

//Query list
#define QUERY_CODE			0x1
#define QUERY_POSITION		0x2
#define QUERY_ALIAS			0x4
#define QUERY_TYPE			0x8
#define QUERY_LEVEL			0x10
#define QUERY_RANK			0x20
#define QUERY_ATTRIBUTE		0x40
#define QUERY_RACE			0x80
#define QUERY_ATTACK		0x100
#define QUERY_DEFENCE		0x200
#define QUERY_BASE_ATTACK	0x400
#define QUERY_BASE_DEFENCE	0x800
#define QUERY_REASON		0x1000
#define QUERY_REASON_CARD	0x2000
#define QUERY_EQUIP_CARD	0x4000
#define QUERY_TARGET_CARD	0x8000
#define QUERY_OVERLAY_CARD	0x10000
#define QUERY_COUNTERS		0x20000
#define QUERY_OWNER			0x40000
#define QUERY_IS_DISABLED	0x80000
#define QUERY_IS_PUBLIC		0x100000
#define QUERY_LSCALE		0x200000
#define QUERY_RSCALE		0x400000

//Assumptions (?)
#define ASSUME_CODE			1
#define ASSUME_TYPE			2
#define ASSUME_LEVEL		3
#define ASSUME_RANK			4
#define ASSUME_ATTRIBUTE	5
#define ASSUME_RACE			6
#define ASSUME_ATTACK		7
#define ASSUME_DEFENCE		8
#endif /* CARD_H_ */
