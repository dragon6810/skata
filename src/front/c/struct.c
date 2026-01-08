#include "struct.h"

void struct_freemember(struct_member_t* member)
{
    type_free(&member->type);
    free(member->ident);
}

void struct_free(struct_t* structure)
{
    list_struct_member_free(&structure->members);
}

LIST_DEF(struct_member)
LIST_DEF_FREE_DECONSTRUCT(struct_member, struct_freemember)