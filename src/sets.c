#include "set.h"

#include "map.h"

SET_DEF(char*, str, map_strhash, map_strcmp, map_strcpy, map_freestr)