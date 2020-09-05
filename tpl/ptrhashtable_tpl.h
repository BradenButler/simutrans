/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef TPL_PTRHASHTABLE_TPL_H
#define TPL_PTRHASHTABLE_TPL_H


#include "hashtable_tpl.h"

/*
 * Define the key characteristics for hashing pointers. For hashing the
 * direct value is used.
 */
template<class key_t>
class ptrhash_tpl {
public:

	static uint32 hash(const key_t key)
	{
		return (uint32)(size_t)key;
	}

	static void dump(const key_t key)
	{
		printf("%p", (void *)key);
	}

	static diff_type comp(key_t key1, key_t key2)
	{
		return (cast_ptr_to_t)key1 - (cast_ptr_to_t)key2;
	}
};


/**
 * Implements a hashtable with pointer keys
 */
template<class key_t, class value_t>
class ptrhashtable_tpl : public hashtable_tpl<key_t, value_t, ptrhash_tpl<key_t> >
{
public:
	ptrhashtable_tpl() : hashtable_tpl<key_t, value_t, ptrhash_tpl<key_t> >() {}
private:
	ptrhashtable_tpl(const ptrhashtable_tpl&);
	ptrhashtable_tpl& operator=( ptrhashtable_tpl const&);
};

#endif
