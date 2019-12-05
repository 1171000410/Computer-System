/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 转置函数是通过对块大小为32字节的1KB直接映射高速缓存上的未命中次数进行计数来评估的。
 */
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded.
 trans._submit——这是您将针对作业B部分进行评分的解决方案transpose函数
 不要更改描述字符串“Transpose submission”，
 因为驱动程序搜索该字符串以标识要分级的转置函数。
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int i,j,k,p,temp1,temp2,temp3,temp4,temp5,temp6,temp7,temp8;
    if(M==32&&N==32)
    {
        for(i=0;i<M;i=i+8)
        {
            for(j=0;j<N;j++)
            {
                temp1 = A[j][i];
                temp2 = A[j][i+1];
                temp3 = A[j][i+2];
                temp4 = A[j][i+3];
                temp5 = A[j][i+4];
                temp6 = A[j][i+5];
                temp7 = A[j][i+6];
                temp8 = A[j][i+7];
                B[i][j] = temp1;
                B[i+1][j] = temp2;
                B[i+2][j] = temp3;
                B[i+3][j] = temp4;
                B[i+4][j] = temp5;
                B[i+5][j] = temp6;
                B[i+6][j] = temp7;
                B[i+7][j] = temp8;
            }
        }
    }
    if(M==64&&N==64)
    {
        for(i=0;i<N;i=i+8)
        {
            for(j=0;j<M;j=j+8)
            {
                for(k=i;k<i+4;k++)
                {
                    temp1=A[k][j];
                    temp2=A[k][j+1];
                    temp3=A[k][j+2];
                    temp4=A[k][j+3];
                    temp5=A[k][j+4];
                    temp6=A[k][j+5];
                    temp7=A[k][j+6];
                    temp8=A[k][j+7];
                    B[j][k]=temp1;
                    B[j+1][k]=temp2;
                    B[j+2][k]=temp3;
                    B[j+3][k]=temp4;
                    B[j][k+4]=temp5;
                    B[j+1][k+4]=temp6;
                    B[j+2][k+4]=temp7;
                    B[j+3][k+4]=temp8;
                }
                for(p=j;p<j+4;p++)
                {
                    temp1=A[i+4][p];
                    temp2=A[i+5][p];
                    temp3=A[i+6][p];
                    temp4=A[i+7][p];
                    temp5=B[p][i+4];
                    temp6=B[p][i+5];
                    temp7=B[p][i+6];
                    temp8=B[p][i+7];
                    B[p][i+4]=temp1;
                    B[p][i+5]=temp2;
                    B[p][i+6]=temp3;
                    B[p][i+7]=temp4;
                    B[p+4][i]=temp5;
                    B[p+4][i+1]=temp6;
                    B[p+4][i+2]=temp7;
                    B[p+4][i+3]=temp8;
                }
                for(k=i+4;k<i+8;k++)
                {
                    temp1=A[k][j+4];
                    temp2=A[k][j+5];
                    temp3=A[k][j+6];
                    temp4=A[k][j+7];
                    B[j+4][k]=temp1;
                    B[j+5][k]=temp2;
                    B[j+6][k]=temp3;
                    B[j+7][k]=temp4;
                }
            }
        }
    }
    if(M==61&&N==67)
    {
        for(i=0;i<N;i=i+17)
        {
            for(j=0;j<M;j=j+17)
            {
                for(k=i;k<i+17 && k<N;k++)
                {
                    for(p=j;p<j+17 && p<M;p++)
                    {
                        temp1 = A[k][p];
                        B[p][k] = temp1;
                    }
                }
            }
        }
    }
}

/*
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started.
 您可以在下面定义额外的转置函数。下面我们定义了一个简单的方法来帮助您入门。
 */

/*
 * trans - A simple baseline transpose function, not optimized for the cache.
 一个简单的基线转置函数，没有对缓存进行优化。
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 registerFunctions——这个函数向驱动程序注册转置函数。
 在运行时，驱动程序将评估每个注册函数并总结它们的性能。
 这是一个方便的实验方法，用不同的转置策略。
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc);

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc);

}

/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 IsTrpSPOS-这个辅助函数检查B是否是转置的
*您可以通过调用来检查转置的正确性。
*在从转置函数返回之前。
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

