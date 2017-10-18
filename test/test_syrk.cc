#include <omp.h>

#include "test.hh"
#include "cblas.hh"
#include "lapack.hh"
#include "flops.hh"
#include "print_matrix.hh"
#include "check_gemm.hh"

#include "syrk.hh"

// -----------------------------------------------------------------------------
template< typename TA, typename TC >
void test_syrk_work( Params& params, bool run )
{
    using namespace blas;
    typedef typename traits2< TA, TC >::scalar_t scalar_t;
    typedef typename traits< scalar_t >::real_t real_t;
    typedef long long lld;

    // get & mark input values
    blas::Layout layout = params.layout.value();
    blas::Op trans  = params.trans.value();
    blas::Uplo uplo = params.uplo.value();
    scalar_t alpha  = params.alpha.value();
    scalar_t beta   = params.beta.value();
    int64_t n       = params.dim.n();
    int64_t k       = params.dim.k();
    int64_t align   = params.align.value();
    int64_t verbose = params.verbose.value();

    // mark non-standard output values
    params.ref_time.value();
    params.ref_gflops.value();

    if ( ! run)
        return;

    // setup
    int64_t Am = (trans == Op::NoTrans ? n : k);
    int64_t An = (trans == Op::NoTrans ? k : n);
    if (layout == Layout::RowMajor)
        std::swap( Am, An );
    int64_t lda = roundup( Am, align );
    int64_t ldc = roundup(  n, align );
    size_t size_A = size_t(lda)*An;
    size_t size_C = size_t(ldc)*n;
    TA* A    = new TA[ size_A ];
    TC* C    = new TC[ size_C ];
    TC* Cref = new TC[ size_C ];

    int64_t idist = 1;
    int iseed[4] = { 0, 0, 0, 1 };
    lapack_larnv( idist, iseed, size_A, A );
    lapack_larnv( idist, iseed, size_C, C );
    lapack_lacpy( "g", n, n, C, ldc, Cref, ldc );

    // norms for error check
    real_t work[1];
    real_t Anorm = lapack_lange( "f", Am, An, A, lda, work );
    real_t Cnorm = lapack_lansy( "f", uplo2str(uplo), n, C, ldc, work );

    // test error exits
    assert_throw( blas::syrk( Layout(0), uplo,    trans,  n,  k, alpha, A, lda, beta, C, ldc ), blas::Error );
    assert_throw( blas::syrk( layout,    Uplo(0), trans,  n,  k, alpha, A, lda, beta, C, ldc ), blas::Error );
    assert_throw( blas::syrk( layout,    uplo,    Op(0),  n,  k, alpha, A, lda, beta, C, ldc ), blas::Error );
    assert_throw( blas::syrk( layout,    uplo,    trans, -1,  k, alpha, A, lda, beta, C, ldc ), blas::Error );
    assert_throw( blas::syrk( layout,    uplo,    trans,  n, -1, alpha, A, lda, beta, C, ldc ), blas::Error );

    assert_throw( blas::syrk( Layout::ColMajor, uplo, Op::NoTrans,   n, k, alpha, A, n-1, beta, C, ldc ), blas::Error );
    assert_throw( blas::syrk( Layout::ColMajor, uplo, Op::Trans,     n, k, alpha, A, k-1, beta, C, ldc ), blas::Error );
    assert_throw( blas::syrk( Layout::ColMajor, uplo, Op::ConjTrans, n, k, alpha, A, k-1, beta, C, ldc ), blas::Error );

    assert_throw( blas::syrk( Layout::RowMajor, uplo, Op::NoTrans,   n, k, alpha, A, k-1, beta, C, ldc ), blas::Error );
    assert_throw( blas::syrk( Layout::RowMajor, uplo, Op::Trans,     n, k, alpha, A, n-1, beta, C, ldc ), blas::Error );
    assert_throw( blas::syrk( Layout::RowMajor, uplo, Op::ConjTrans, n, k, alpha, A, n-1, beta, C, ldc ), blas::Error );

    assert_throw( blas::syrk( layout,    uplo,    trans,  n,  k, alpha, A, lda, beta, C, n-1 ), blas::Error );

    if (verbose >= 1) {
        printf( "layout %c, uplo %c, trans %c\n"
                "A An=%5lld, An=%5lld, lda=%5lld, size=%5lld, norm %.2e\n"
                "C  n=%5lld,  n=%5lld, ldc=%5lld, size=%5lld, norm %.2e\n",
                layout2char(layout), uplo2char(uplo), op2char(trans),
                (lld) Am, (lld) An, (lld) lda, (lld) size_A, Anorm,
                (lld)  n, (lld)  n, (lld) ldc, (lld) size_C, Cnorm );
    }
    if (verbose >= 2) {
        printf( "alpha = %.4e + %.4ei; beta = %.4e + %.4ei;\n",
                real(alpha), imag(alpha),
                real(beta),  imag(beta) );
        printf( "A = "    ); print_matrix( Am, An, A, lda );
        printf( "C = "    ); print_matrix(  n,  n, C, ldc );
    }

    // run test
    libtest::flush_cache( params.cache.value() );
    double time = omp_get_wtime();
    blas::syrk( layout, uplo, trans, n, k,
                alpha, A, lda, beta, C, ldc );
    time = omp_get_wtime() - time;

    double gflop = gflop_syrk( n, k, C );
    params.time.value()   = time * 1000;  // msec
    params.gflops.value() = gflop / time;

    if (verbose >= 2) {
        printf( "C2 = " ); print_matrix( n, n, C, ldc );
    }

    if (params.ref.value() == 'y' || params.check.value() == 'y') {
        // run reference
        libtest::flush_cache( params.cache.value() );
        time = omp_get_wtime();
        cblas_syrk( cblas_layout_const(layout),
                    cblas_uplo_const(uplo),
                    cblas_trans_const(trans),
                    n, k, alpha, A, lda, beta, Cref, ldc );
        time = omp_get_wtime() - time;

        params.ref_time.value()   = time * 1000;  // msec
        params.ref_gflops.value() = gflop / time;

        if (verbose >= 2) {
            printf( "Cref = " ); print_matrix( n, n, Cref, ldc );
        }

        // check error compared to reference
        real_t error;
        int64_t okay;
        check_herk( uplo, n, k, alpha, beta, Anorm, Anorm, Cnorm,
                    Cref, ldc, C, ldc, &error, &okay );
        params.error.value() = error;
        params.okay.value() = okay;
    }

    delete[] A;
    delete[] C;
    delete[] Cref;
}

// -----------------------------------------------------------------------------
void test_syrk( Params& params, bool run )
{
    switch (params.datatype.value()) {
        case libtest::DataType::Integer:
            //test_syrk_work< int64_t >( params, run );
            throw std::exception();
            break;

        case libtest::DataType::Single:
            test_syrk_work< float, float >( params, run );
            break;

        case libtest::DataType::Double:
            test_syrk_work< double, double >( params, run );
            break;

        case libtest::DataType::SingleComplex:
            test_syrk_work< std::complex<float>, std::complex<float> >
                ( params, run );
            break;

        case libtest::DataType::DoubleComplex:
            test_syrk_work< std::complex<double>, std::complex<double> >
                ( params, run );
            break;
    }
}