/*
 * group.h
 *
 *  Created on: 2010-5-6
 *      Author: Argon
 */

#ifndef GROUP_H_
#define GROUP_H_

#include "common.h"
#include <set>
#include <list>

class card;
class duel;

class group {
public:
	typedef std::set<card*, card_sort> card_set; // a list of cards with a comparator
	int32 scrtype; // ?
	int32 ref_handle; // ?
	duel* pduel; // the current duel
	card_set container; // ?
	card_set::iterator it; // ?
	uint32 is_readonly; // ?
	 // a list of cards
	inline bool has_card(card* c) { // returns if a certain card is in the group
		return container.find(c) != container.end();
	}
	
	group(); // constructor
	~group(); // destructor
};

#endif /* GROUP_H_ */
