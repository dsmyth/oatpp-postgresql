//
// Created by dsmyth on 1/1/21.
//

#ifndef oatpp_postgresql_mapping_PgArray_hpp
#define oatpp_postgresql_mapping_PgArray_hpp

#include "oatpp/core/Types.hpp"

#include <libpq-fe.h>

struct PgElem {
    v_int32 size;       // size of each element value (bytes)
    v_uint8 value[1];   // Beginning of value array -- dynamically sized
};

// after https://stackoverflow.com/questions/4016412/postgresqls-libpq-encoding-for-binary-transport-of-array-data
struct PgArrayHeader {
    PgArrayHeader() : ndim(0), _ign(0), oid(InvalidOid), size(0), index(0) {};
    v_int32 ndim;   // Number of dimensions
    v_int32 _ign;   // offset for data, removed by libpq
    Oid oid;        // type of element in the array

    // Start of array (1st dimension)
    v_int32 size;   // Number of elements
    v_int32 index;  // Index of first element
};

// Layout of Postgres array in memory
struct PgArray {
    PgArrayHeader header;
    PgElem elem[1]; // Beginning of (size, value) elements
};

#endif // oatpp_postgresql_mapping_PgArray_hpp