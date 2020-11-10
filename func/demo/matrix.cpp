#include "faasm/matrix.h"
#include "faasm/faasm.h"

int main(int argc, char* argv[])
{
    Eigen::MatrixXd dense = faasm::randomDenseMatrix(1, 10);
    Eigen::SparseMatrix<double> sparse = faasm::randomSparseMatrix(10, 20, 0.1);

    Eigen::MatrixXd prod = dense * sparse;

    printf("Output elements %f %f %f\n",
           prod.coeff(0, 0),
           prod.coeff(0, 1),
           prod.coeff(0, 2));

    return 0;
}
