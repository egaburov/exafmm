#include <mpi.h>
#include <cmath>
#include <cstdlib>
#include <iostream>

extern "C" void FMMcalccoulomb(int n, double* x, double* q, double *p, double* f, int periodicflag);

extern "C" void MPI_Shift(double *var, int n, int mpisize, int mpirank) {
  double *buf = new double [n];
  const int isend = (mpirank + 1          ) % mpisize;
  const int irecv = (mpirank - 1 + mpisize) % mpisize;
  MPI_Request sreq, rreq;

  MPI_Isend(var, n, MPI_DOUBLE, irecv, 1, MPI_COMM_WORLD, &sreq);
  MPI_Irecv(buf, n, MPI_DOUBLE, isend, 1, MPI_COMM_WORLD, &rreq);
  MPI_Wait(&sreq, MPI_STATUS_IGNORE);
  MPI_Wait(&rreq, MPI_STATUS_IGNORE);
  int i;
  for( i=0; i!=n; ++i ) {
    var[i] = buf[i];
  }
  delete[] buf;
}

int main(int argc, char **argv) {
  MPI_Init(&argc,&argv);
  int mpisize, mpirank;
  MPI_Comm_size(MPI_COMM_WORLD, &mpisize);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpirank);
  const int N = 1000000;
  const double size = 2 * M_PI;
  double *xi = new double [3*N];
  double *qi = new double [N];
  double *pi = new double [N];
  double *fi = new double [3*N];
  double *pd = new double [N];
  double *fd = new double [3*N];
  double *xj = new double [3*N];
  double *qj = new double [N];

  srand48(mpirank);
  for( int i=0; i!=N; ++i ) {
    xi[3*i+0] = drand48() * size - M_PI;
    xi[3*i+1] = drand48() * size - M_PI;
    xi[3*i+2] = drand48() * size - M_PI;
    qi[i] = 1. / N;
    pi[i] = 0;
    fi[3*i+0] = fi[3*i+1] = fi[3*i+2] = 0;
    pd[i] = 0;
    fd[3*i+0] = fd[3*i+1] = fd[3*i+2] = 0;
    xj[3*i+0] = xi[3*i+0];
    xj[3*i+1] = xi[3*i+1];
    xj[3*i+2] = xi[3*i+2];
    qj[i] = qi[i];
  }

  FMMcalccoulomb(N, xi, qi, pi, fi, 0);
  for( int i=0; i!=N; ++i ) {
    xj[3*i+0] = xi[3*i+0];
    xj[3*i+1] = xi[3*i+1];
    xj[3*i+2] = xi[3*i+2];
  }
  for( int irank=0; irank!=mpisize; ++irank ) {
    MPI_Shift(xj, 3*N, mpisize, mpirank);
    MPI_Shift(qj, N, mpisize, mpirank);
    for( int i=0; i!=100; ++i ) {
      double P = 0, Fx = 0, Fy = 0, Fz = 0;
      for( int j=0; j!=N; ++j ) {
        double dx = xi[3*i+0] - xj[3*j+0];
        double dy = xi[3*i+1] - xj[3*j+1];
        double dz = xi[3*i+2] - xj[3*j+2];
        double R2 = dx * dx + dy * dy + dz * dz;
        double invR = 1 / std::sqrt(R2);
        if( irank == mpisize-1 && i == j ) invR = 0;
        double invR3 = qj[j] * invR * invR * invR;
        P += qj[j] * invR;
        Fx += dx * invR3;
        Fy += dy * invR3;
        Fz += dz * invR3;
      }
      pd[i] += P;
      fd[3*i+0] += Fx;
      fd[3*i+1] += Fy;
      fd[3*i+2] += Fz;
    }
  }
  double Pd = 0, Pn = 0, Fd = 0, Fn = 0;
  for( int i=0; i!=100; ++i ) {
    Pd += (pi[i] - pd[i]) * (pi[i] - pd[i]);
    Pn += pd[i] * pd[i];
    Fd += (fi[3*i+0] - fd[3*i+0]) * (fi[3*i+0] - fd[3*i+0])
        + (fi[3*i+1] - fd[3*i+1]) * (fi[3*i+1] - fd[3*i+1])
        + (fi[3*i+2] - fd[3*i+2]) * (fi[3*i+2] - fd[3*i+2]);
    Fn += fd[3*i+0] * fd[3*i+0] + fd[3*i+1] * fd[3*i+1] + fd[3*i+2] * fd[3*i+2];
  }
  std::cout << "Coulomb       potential @ rank " << mpirank << " : " << sqrtf(Pd/Pn) << std::endl;
  std::cout << "Coulomb       force     @ rank " << mpirank << " : " << sqrtf(Fd/Fn) << std::endl;

  delete[] xi;
  delete[] qi;
  delete[] pi;
  delete[] fi;
  delete[] pd;
  delete[] fd;
  delete[] xj;
  delete[] qj;

  MPI_Finalize();
}