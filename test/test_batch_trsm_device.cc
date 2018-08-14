#include "test.hh"
#include "cblas.hh"
#include "lapack_tmp.hh"
#include "blas_flops.hh"
#include "print_matrix.hh"
#include "check_gemm.hh"

// -----------------------------------------------------------------------------
template< typename TA, typename TB >
void test_device_batch_trsm_work( Params& params, bool run )
{
    using namespace libtest;
    using namespace blas;
    using namespace blas::batch;
    typedef scalar_type<TA, TB> scalar_t;
    typedef real_type<scalar_t> real_t;
    typedef long long lld;

    // get & mark input values
    blas::Side side_ = params.side.value();
    blas::Uplo uplo_ = params.uplo.value();
    blas::Op trans_  = params.trans.value();
    blas::Diag diag_ = params.diag.value();
    scalar_t alpha_  = params.alpha.value();
    int64_t m_       = params.dim.m();
    int64_t n_       = params.dim.n();
    int64_t batch    = params.batch.value();
    int64_t device  = params.device.value();
    int64_t align    = params.align.value();
    int64_t verbose  = params.verbose.value();

    // mark non-standard output values
    params.gflops.value();
    params.ref_time.value();
    params.ref_gflops.value();

    if (! run)
        return;

    // ----------
    // setup
    int64_t Am    = (side_ == Side::Left ? m_ : n_);
    int64_t Bm    = m_;
    int64_t Bn    = n_;
    int64_t lda_  = roundup( Am, align );
    int64_t ldb_  = roundup( Bm, align );
    size_t size_A = size_t(lda_)*Am;
    size_t size_B = size_t(ldb_)*Bn;
    TA* A         = new TA[ batch * size_A ];
    TB* B         = new TB[ batch * size_B ];
    TB* Bref      = new TB[ batch * size_B ];

    // device specifics 
    blas::Queue queue(device, batch);
    TA* dA; 
    TB* dB; 

    dA = blas::device_malloc<TA>( batch * size_A );
    dB = blas::device_malloc<TB>( batch * size_B );

    // pointer arrays
    std::vector<TA*>    Aarray( batch );
    std::vector<TB*>    Barray( batch );
    std::vector<TA*>   dAarray( batch );
    std::vector<TB*>   dBarray( batch );
    std::vector<TB*> Brefarray( batch );

    for(int i = 0; i < batch; i++){
         Aarray[i]   =  A   + i * size_A;
         Barray[i]   =  B   + i * size_B;
        dAarray[i]   = dA   + i * size_A;
        dBarray[i]   = dB   + i * size_B;
        Brefarray[i] = Bref + i * size_B;
    }

    // info
    std::vector<int64_t> info( batch );

    // wrap scalar arguments in std::vector
    std::vector<Side>     side(1, side_);
    std::vector<Uplo>     uplo(1, uplo_);
    std::vector<Op>       trans(1, trans_);
    std::vector<Diag>     diag(1, diag_);
    std::vector<int64_t>  m(1, m_);
    std::vector<int64_t>  n(1, n_);
    std::vector<int64_t>  ldda(1, lda_);
    std::vector<int64_t>  lddb(1, ldb_);
    std::vector<scalar_t> alpha(1, alpha_);

    int64_t idist = 1;
    int iseed[4] = { 0, 0, 0, 1 };
    lapack_larnv( idist, iseed, batch * size_A, A );  // TODO: generate
    lapack_larnv( idist, iseed, batch * size_B, B );  // TODO
    lapack_lacpy( "g", Bm, batch * Bn, B, ldb_, Bref, ldb_ );

    // set unused data to nan
    /*if (uplo_ == Uplo::Lower) {
        for (int j = 0; j < Am; ++j)
            for (int i = 0; i < j; ++i)  // upper
                A[ i + j*lda_ ] = nan("");
    }
    else {
        for (int j = 0; j < Am; ++j)
            for (int i = j+1; i < Am; ++i)  // lower
                A[ i + j*lda_ ] = nan("");
    }*/

    // Factor A into L L^H or U U^H to get a well-conditioned triangular matrix.
    // If diag_ == Unit, the diag_onal is replaced; this is still well-conditioned.
    for(int s = 0; s < batch; s++){
        TA* pA = Aarray[s];
        // First, brute force positive definiteness.
        for (int i = 0; i < Am; ++i) {
            pA[ i + i*lda_ ] += Am;
        }
        blas_int potrf_info = 0;
        lapack_potrf( uplo2str(uplo_), Am, pA, lda_, &potrf_info );
        assert( potrf_info == 0 );
    }
    blas::device_setmatrix(Am, batch * Am, A, lda_, dA, lda_, queue);
    blas::device_setmatrix(Bm, batch * Bn, B, ldb_, dB, ldb_, queue);
    queue.sync();

    // norms for error check
    real_t work[1];
    real_t* Anorm = new real_t[ batch ];
    real_t* Bnorm = new real_t[ batch ];
    for(int s = 0; s < batch; s++){
        Anorm[ s ] = lapack_lantr( "f", uplo2str(uplo_), diag2str(diag_), Am, Am, Aarray[s], lda_, work );
        Bnorm[ s ] = lapack_lange( "f", Bm, Bn, Barray[s], ldb_, work );
    }

    // decide error checking mode
    info.resize( 0 );

    // run test
    libtest::flush_cache( params.cache.value() );
    double time = get_wtime();
    blas::batch::trsm( side, uplo, trans, diag, m, n, alpha, dAarray, ldda, dBarray, lddb, 
                       batch, info, queue );
    queue.sync();
    time = get_wtime() - time;

    double gflop = batch * Gflop < scalar_t >::trsm( side_, m_, n_ );
    params.time.value()   = time;
    params.gflops.value() = gflop / time;

    blas::device_getmatrix(Bm, batch * Bn, dB, ldb_, B, ldb_, queue);
    queue.sync();

    if (params.check.value() == 'y') {
        // run reference
        libtest::flush_cache( params.cache.value() );
        time = get_wtime();
        for(int i = 0; i < batch; i++){
            cblas_trsm( CblasColumnMajor,
                        cblas_side_const(side_),
                        cblas_uplo_const(uplo_),
                        cblas_trans_const(trans_),
                        cblas_diag_const(diag_),
                        m_, n_, alpha_, Aarray[i], lda_, Brefarray[i], ldb_ );
        }
        time = get_wtime() - time;

        params.ref_time.value()   = time;
        params.ref_gflops.value() = gflop / time;

        // check error compared to reference
        // Am is reduction dimension
        // beta = 0, Cnorm = 0 (initial).
        real_t err, error = 0;
        bool ok, okay = true;
        for(int i = 0; i < batch; i++){
            check_gemm( Bm, Bn, Am, alpha_, scalar_t(0), Anorm[i], Bnorm[i], real_t(0),
                        Brefarray[i], ldb_, Barray[i], ldb_, verbose, &err, &ok );
            error = max(error, err);
            okay &= ok;
        }
        params.error.value() = error;
        params.okay.value() = okay;
    }

    delete[] A;
    delete[] B;
    delete[] Bref;
    delete[] Anorm;
    delete[] Bnorm;

    blas::device_free( dA );
    blas::device_free( dB );
}

// -----------------------------------------------------------------------------
void test_batch_trsm_device( Params& params, bool run )
{
    switch (params.datatype.value()) {
        case libtest::DataType::Integer:
            //test_device_batch_trsm_work< int64_t >( params, run );
            throw std::exception();
            break;

        case libtest::DataType::Single:
            test_device_batch_trsm_work< float, float >( params, run );
            break;

        case libtest::DataType::Double:
            test_device_batch_trsm_work< double, double >( params, run );
            break;

        case libtest::DataType::SingleComplex:
            test_device_batch_trsm_work< std::complex<float>, std::complex<float> >
                ( params, run );
            break;

        case libtest::DataType::DoubleComplex:
            test_device_batch_trsm_work< std::complex<double>, std::complex<double> >
                ( params, run );
            break;
    }
}
