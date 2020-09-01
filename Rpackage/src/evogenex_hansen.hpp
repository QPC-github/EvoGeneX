#ifndef __EVOGENEX_HANSEN_HPP__
#define __EVOGENEX_HANSEN_HPP__

#include "evogenex_common.h"

class MLE {
public:
    int nterm;
    int nrep;
    int nreg;
    int n;
    double alpha;
    double gamma;
    double sigmasq;
    double logLik;
    std::vector<double> par;
    const NumericVector &nbranch;
    const NumericVector &beta;
    const NumericVector &epochs;
    const NumericMatrix &bt;
    const Map<VectorXd> &dat;
#if KEEP_LOG
    ofstream out;
    int fcount;
#endif
    MatrixXd W;
    MatrixXd V;
    VectorXd theta;
    MLE(int _nterm,
        int _nrep,
        int _nreg,
        double _alpha,
        double _gamma,
        const NumericVector &_nbranch,
        const NumericVector &_beta,
        const NumericVector &_epochs,
        const NumericMatrix &_bt,
        const NumericVector &_dat
    ): nterm(_nterm), nrep(_nrep), nreg(_nreg),
    n(_nterm * _nrep), alpha(_alpha), gamma(_gamma),
    par{alpha, gamma},
    nbranch(_nbranch),
    beta(_beta),
    epochs(_epochs),
    bt(_bt),
    dat(as<Map<VectorXd> >(_dat)),
#if KEEP_LOG
    out("fast.log"),
    fcount(0),
#endif
    W(n, nreg),
    V(n, n),
    theta(nreg)
    {
    }
    void computeWeights() {
        int index = 0;
        double y[nterm];
        for (int term=0; term<nterm; term++) {
            //cout << "Processing for terminal " << term << endl;
            int nb = nbranch[term];
            //cout << "Num branch " << nb << endl;
            for (int br=0; br<nb; br++) {
                double t = epochs[index] - epochs[index+br];
                y[br] = exp(-alpha*t);
            }
            //cout << "Computing y for terminal " << term << endl;
            for (int br=0; br<nb-1; br++) {
                y[br] = y[br] - y[br+1];
            }
            //cout << "Creating W for terminal " << term << endl;
            for (int reg=0; reg<nreg; reg++) {
                double dotp = 0.0;
                //cout << "Computing dotp  regime " << reg << endl;
                //cout << "beta for regime " << reg << endl;
                for (int br=0; br<nb; br++) {
                    dotp += y[br]*(beta[index+br] == reg ? 1 : 0);
                    //cout << " " << (beta[index+br] == reg ? 1 : 0);
                }
                //cout << endl;
                //cout << "Dotp   " << dotp << endl;
                for (int rep=0; rep<nrep; rep++) {
                    int row = rep + term*nrep;
                    //cout << "row=" << row << endl;
                    W(row,reg) = dotp;
                }
            }
            index += nb;
        }
    }

    void computeCovars() {
        for (int termi=0; termi<nterm; termi++) {
            for (int repk=0; repk<nrep; repk++) {
                for (int termj=0; termj<nterm; termj++) {
                    for (int repl=0; repl<nrep; repl++) {
                        int p = repk + termi*nrep;
                        int q = repl + termj*nrep;
                        double ii = bt[termi + termi*nterm];
                        double jj = bt[termj + termj*nterm];
                        double ij = bt[termi + termj*nterm];
                        double part1 = exp(alpha*(-ii - jj + 2*ij));
                        double part2 = (1-exp(-2*alpha*ij));
                        V(p,q) = part1*part2/(2*alpha);
                        if ((termi == termj) && (repk == repl)) {
                            V(p,q) += gamma;
                        }
                    }
                }
            }
        }
    }

    double computeLogLik()
    {
        alpha = par[0];
        gamma = par[1];
 
#if KEEP_LOG
        fcount++;
        out << "###################### IN ComputeLogLik ########################" << endl;
        out << "n: " << n << endl;
        out << "nrep: " << nrep << endl;
        out << "alpha: " << alpha << endl;
        out << "gamma: " << gamma << endl;
#endif

        computeWeights();
#if KEEP_LOG
        out << "W:\n" << W << endl;
#endif

        computeCovars();
#if KEEP_LOG
        out << "V:\n" << V << endl;
#endif

        auto solver2 = V.colPivHouseholderQr();

        MatrixXd L = V.llt().matrixL();
#if KEEP_LOG
        out << "L:\n" << L << endl;
#endif

        auto solver = L.colPivHouseholderQr();
        auto X = solver.solve(W);
#if KEEP_LOG
        out << "X:\n" << X << endl;
#endif

        auto y = solver.solve(dat);
#if KEEP_LOG
        out << "y:\n" << y << endl;
#endif

        theta = X.bdcSvd(ComputeThinU | ComputeThinV).setThreshold(X_TOL).solve(y);
#if KEEP_LOG
        out << "theta:\n" << theta << endl;
#endif

        auto e = (W*theta - dat);
#if KEEP_LOG
        out << "e:\n" << e << endl;
#endif

        auto zz = solver2.solve(e);
#if KEEP_LOG
        out << "zz:\n" << zz << endl;
#endif

        auto q = e.transpose()*zz;
#if KEEP_LOG
        out << "q:\n" << q << endl;
#endif

        sigmasq = q(0,0)/n;
#if KEEP_LOG
        out << "sigmasq: " << sigmasq << endl;
#endif

        double detv = 2 * solver.logAbsDeterminant(); //toDenseMatrix().diagonal().array().log().sum();
        
        logLik = n*log(2*M_PI) + n*(1+log(sigmasq)) + detv;
#if KEEP_LOG
        out << "logLik: " << logLik << endl;
#endif
        return(logLik);
    }
};

#endif // __EVOGENEX_HANSEN_HPP__
