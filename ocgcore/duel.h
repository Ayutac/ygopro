/*
 * duel.h
 * Contains the necessary structures to simulate duels,
 * and constant related to duels.
 *
 *  Created on: 2010-4-8
 *      Author: Argon
 */

#ifndef DUEL_H_
#define DUEL_H_

#include "common.h"
#include "mtrandom.h"
#include <set>

class card;
class group;
class effect;
class field;
class interpreter;

/*
 * A structure for initial arguments, that are: start life points, start hand and draw count.
 */
struct duel_arg {
	int16 start_lp;
	int8 start_hand;
	int8 draw_count;
};

class duel {
public:
	char strbuffer[256]; // ?
	byte buffer[0x1000]; // maybe for queries ?
	uint32 bufferlen; // the length of the buffer, use with read_buffer
	byte* bufferp; // the position on the buffer
	interpreter* lua; // the interpreter for effects
	field* game_field; // the game field
	mtrandom random; // the RNG
	std::set<card*> cards; // the cards in the duel
	std::set<card*> assumes; // assumptions for certain cards ?
	std::set<group*> groups; // card groups in the duel
	std::set<group*> sgroups; // script groups in the game
	std::set<effect*> effects; // the effects currently in place
	std::set<effect*> uncopy;
	
	duel(); 
	~duel();  
	void clear(); // resets the duel object
	
	card* new_card(uint32 code); // Adds a card to the duel by code.
	group* new_group(card* pcard = 0); // Adds a card group to the duel.
	effect* new_effect(); // Adds an empty effect to the duel.
	void delete_card(card* pcard); // Deletes the specified effect.
	void delete_group(group* pgroup); // Deletes the specified effect.
	void delete_effect(effect* peffect); // Deletes the specified effect.
	void release_script_group(); // Clears the script groups.
	void restore_assumes(); // Clears the assumptions.
	int32 read_buffer(byte* buf); // reads from the buffer, into buffer and bufferlen
	void write_buffer32(uint32 value); // writes to the buffer
	void write_buffer16(uint16 value);
	void write_buffer8(uint8 value);
	void clear_buffer(); // clears the buffer
	void set_responsei(uint32 resp); // Sets a integer response?
	void set_responseb(byte* resp); // Sets a byte response?
	int32 get_next_integer(int32 l, int32 h); // Random integer in [l, h]
};

//Player
#define PLAYER_NONE		2	//
#define PLAYER_ALL		3	//
//Phase
#define PHASE_DRAW			0x01	//
#define PHASE_STANDBY		0x02	//
#define PHASE_MAIN1			0x04	//
#define PHASE_BATTLE		0x08	//
#define PHASE_DAMAGE		0x10	//
#define PHASE_DAMAGE_CAL	0x20	//
#define PHASE_MAIN2			0x40	//
#define PHASE_END			0x80	//
//Options
#define DUEL_TEST_MODE			0x01
#define DUEL_ATTACK_FIRST_TURN	0x02
#define DUEL_NO_CHAIN_HINT		0x04
#define DUEL_OBSOLETE_RULING	0x08
#define DUEL_PSEUDO_SHUFFLE		0x10
#define DUEL_TAG_MODE			0x20
#define DUEL_SIMPLE_AI			0x40
#endif /* DUEL_H_ */
