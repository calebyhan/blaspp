// Copyright (c) 2017-2022, University of Tennessee. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// This program is free software: you can redistribute it and/or modify it under
// the terms of the BSD 3-Clause license. See the accompanying LICENSE file.

#ifndef BLAS_COUNTER_HH
#define BLAS_COUNTER_HH

#include "blas/defines.h"
#include "blas/util.hh"

#ifdef BLAS_HAVE_PAPI
    #include "sde_lib.h"
    #include "sde_lib.hpp"
#endif

namespace blas {

//==============================================================================
/// Initialize PAPI counters for BLAS++.
/// Uses thread-safe Scott Meyers Singleton.
/// This class acts like a namespace -- all public functions are static.
class counter
{
public:
    #ifdef BLAS_HAVE_PAPI
        typedef papi_sde::PapiSde::CountingSet CountingSet;
    #else
        typedef void CountingSet;
        typedef void cset_list_object_t;
    #endif

public:
    //------------------------------------------------------------------------------
    /// ID to differentiate routines in the counter set.
    enum class Id {
        gemm,
        hemm,
        her2k,
        herk,
        symm,
        syr2k,
        syrk,
        trmm,
        trsm,
        // Add alphabetically.
    };

    //------------------------------------------------------------------------------
    struct gemm_type {
        blas::Op transA, transB;
        int64_t m, n, k;
    };

    //------------------------------------------------------------------------------
    struct hemm_type {
        // reusable for hemm, symm
        blas::Side side;
        blas::Uplo uplo;
        int64_t m, n;
    };

    //------------------------------------------------------------------------------
    struct herk_type {
        // reusable for herk, her2k, syrk, skr2k
        blas::Uplo uplo;
        blas::Op trans;
        int64_t n, k;
    };

    //------------------------------------------------------------------------------
    struct trmm_type {
        // reusable for trmm, trsm
        blas::Side side;
        blas::Uplo uplo;
        blas::Op transA;
        blas::Diag diag;
        int64_t m, n;
    };

    //--------------------------------------------------------------------------
    /// Initializes PAPI counting set on first call.
    /// Without PAPI, returns null.
    /// @return PAPI counting set.
    static CountingSet* get()
    {
        static counter s_cnt;
        return s_cnt.set_;
    }

    //--------------------------------------------------------------------------
    /// Inserts element into the PAPI counting set.
    /// Without PAPI, does nothing.
    template <typename T>
    static void insert( T element, Id id )
    {
        #ifdef BLAS_HAVE_PAPI
            get()->insert( element, uint32_t( id ) );
        #endif
    }

    //--------------------------------------------------------------------------
    /// Inserts element with hashable_size into the PAPI counting set.
    /// hashable_size <= sizeof(element).
    /// Without PAPI, does nothing.
    template <typename T>
    static void insert( size_t hashable_size, T element, Id id )
    {
        #ifdef BLAS_HAVE_PAPI
            get()->insert( hashable_size, element, uint32_t( id ) );
        #endif
    }

    //--------------------------------------------------------------------------
    /// TODO
    /// Prints out all elements in the BLAS++ counting set.
    /// Without PAPI, does nothing.
    static void print( cset_list_object_t* list )
    {
        #ifdef BLAS_HAVE_PAPI
            for (auto iter = list; iter != nullptr; iter = iter->next) {
                Id type_id = static_cast<Id>( iter->type_id );
                switch (type_id) {
                    case Id::gemm:
                    {
                        auto *ptr = static_cast<gemm_type *>( iter->ptr );
                        printf( "gemm( %c, %c, %lld, %lld, %lld ) count %d\n",
                                op2char( ptr->transA ), op2char( ptr->transB ),
                                llong( ptr->m ), llong( ptr->n ), llong( ptr->k ),
                                iter->count );
                        break;
                    }
                    case Id::hemm:
                    {
                        auto *ptr = static_cast<hemm_type *>( iter->ptr );
                        printf( "hemm( %c, %c, %lld, %lld ) count %d\n",
                                side2char( ptr->side ), uplo2char( ptr->uplo ),
                                llong( ptr->m ), llong( ptr->n ), iter->count );
                        break;
                    }
                    case Id::her2k:
                    {
                        auto *ptr = static_cast<herk_type *>( iter->ptr );
                        printf( "her2k( %c, %c, %lld, %lld ) count %d\n",
                                uplo2char( ptr->uplo ), op2char( ptr->trans ),
                                llong( ptr->n ), llong( ptr->k ), iter->count );
                        break;
                    }
                    case Id::herk:
                    {
                        auto *ptr = static_cast<herk_type *>( iter->ptr );
                        printf( "herk( %c, %c, %lld, %lld ) count %d\n",
                                uplo2char( ptr->uplo ), op2char( ptr->trans ),
                                llong( ptr->n ), llong( ptr->k ), iter->count );
                        break;
                    }
                    case Id::symm:
                    {
                        auto *ptr = static_cast<hemm_type *>( iter->ptr );
                        printf( "symm( %c, %c, %lld, %lld ) count %d\n",
                                side2char( ptr->side ), uplo2char( ptr->uplo ),
                                llong( ptr->m ), llong( ptr->n ), iter->count );
                        break;
                    }
                    case Id::syr2k:
                    {
                        auto *ptr = static_cast<herk_type *>( iter->ptr );
                        printf( "syr2k( %c, %c, %lld, %lld ) count %d\n",
                                uplo2char( ptr->uplo ), op2char( ptr->trans ),
                                llong( ptr->n ), llong( ptr->k ), iter->count );
                        break;
                    }
                    case Id::syrk:
                    {
                        auto *ptr = static_cast<herk_type *>( iter->ptr );
                        printf( "syrk( %c, %c, %lld, %lld ) count %d\n",
                                uplo2char( ptr->uplo ), op2char( ptr->trans ),
                                llong( ptr->n ), llong( ptr->k ), iter->count );
                        break;
                    }
                    case Id::trmm:
                    {
                        auto *ptr = static_cast<trmm_type *>( iter->ptr );
                        printf( "trmm( %c, %c, %c, %c, %lld, %lld ) count %d\n",
                                side2char( ptr->side ), uplo2char( ptr->uplo ),
                                op2char( ptr->transA ), diag2char( ptr->diag ),
                                llong( ptr->m ), llong( ptr->n ), iter->count );
                        break;
                    }
                    case Id::trsm:
                    {
                        auto *ptr = static_cast<trmm_type *>( iter->ptr );
                        printf( "trsm( %c, %c, %c, %c, %lld, %lld ) count %d\n",
                                side2char( ptr->side ), uplo2char( ptr->uplo ),
                                op2char( ptr->transA ), diag2char( ptr->diag ),
                                llong( ptr->m ), llong( ptr->n ), iter->count );
                        break;
                    }
                }
            }
        #endif
    }

private:
    //--------------------------------------------------------------------------
    /// Constructor initializes PAPI counting set on first call to get().
    counter():
        set_( nullptr )
    {
        #ifdef BLAS_HAVE_PAPI
            papi_sde::PapiSde sde( "blas" );
            set_ = sde.create_counting_set( "counter" );
        #endif
    }

    CountingSet* set_;
};  // class count

}  // namespace blas

#endif  // BLAS_COUNTER_HH