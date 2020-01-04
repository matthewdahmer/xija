/* Core model integration using TMAL code */

#include "Python.h"

#if PY_MAJOR_VERSION < 3
void initcore(void)
#else
void PyInit_core(void)
#endif
{
    /* stub initialization function needed by distutils on Windows */
}

void dTdt(int j, int half, int n_preds, int n_tmals, int **tmal_ints, 
          double **tmal_floats, double **mvals, double *deriv, double *y)
{
    int i, i1, i2, i3, opcode;
    double dt2, mvals_i2, mvals_i3;
    
    for (i = 0; i < n_preds; i++) {
        deriv[i] = 0.0;
    }

    for (i = 0; i < n_tmals; i++) {
        opcode = tmal_ints[i][0];
        i1 = tmal_ints[i][1];
        i2 = tmal_ints[i][2];
    
        /* Check to see if we are at the half-step, if so, interpolate */
        
        if (opcode != 1) {
            if (half == 0) {
                mvals_i2 = mvals[i2][j];
            } else {
                mvals_i2 = 0.5*(mvals[i2][j]+mvals[i2][j+1]);
            }
        }
 
        switch (opcode) {
            case 0:  /* Node to node coupling */
                if (i2 < n_preds && i1 < n_preds) {
                    deriv[i1] = deriv[i1] + (y[i2] - y[i1]) / tmal_floats[i][0];
                }
                else {
                    deriv[i1] = deriv[i1] + (mvals_i2 - y[i1]) / tmal_floats[i][0];
                }
                break;
            case 1:  /* heat sink (coupling to fixed temperature) */
                if (i1 < n_preds) {
                    deriv[i1] = deriv[i1] + (tmal_floats[i][0] - y[i1]) / tmal_floats[i][1];
                }
                break;
            case 2:  /* precomputed heat */
                if (i1 < n_preds) {
                    deriv[i1] = deriv[i1] + mvals_i2;
                }
                break;
            case 3: /* active proportional heater */
                dt2 =  tmal_floats[i][0] - ((i2 < n_preds) ? y[i2] : mvals_i2);
                i3 = tmal_ints[i][3];
                if (dt2 > 0) {
                    mvals_i3 = tmal_floats[i][1] * dt2;
                    if (i1 < n_preds) {
                        deriv[i1] = deriv[i1] + mvals_i3;
                    }
                } else if (half == 0) {
                    mvals_i3 = 0.0;
                }
                if (half == 0) mvals[i3][j] = mvals_i3;
                break;
            case 4: /* active thermostatic heater */
                dt2 = tmal_floats[i][0] - ((i2 < n_preds) ? y[i2] : mvals_i2);
                i3 = tmal_ints[i][3];
                if (dt2 > 0) {
                    mvals_i3 = tmal_floats[i][1];
                    if (i1 < n_preds) {
                        deriv[i1] = deriv[i1] + mvals_i3;
                    }
                } else {
                    mvals_i3 = 0.0;
                }
                if (half == 0) mvals[i3][j] = mvals_i3;
                break;
        }
    }

}

int calc_model_new(int n_times, int n_preds, int n_tmals, double dt, 
                   double **mvals, int **tmal_ints, double **tmal_floats)
{
    double *y, *yh, *deriv, *k1, *k2, *k3, *k4;
    int i, j, rk4 = 0;    
    double one_sixth = 1.0/6.0;
    
    deriv = (double *)malloc(n_preds*sizeof(double));
    y = (double *)malloc(n_preds*sizeof(double));
    yh = (double *)malloc(n_preds*sizeof(double));
    k1 = (double *)malloc(n_preds*sizeof(double));
    k2 = (double *)malloc(n_preds*sizeof(double));
    if (rk4 == 1) {
        k3 = (double *)malloc(n_preds*sizeof(double));
        k4 = (double *)malloc(n_preds*sizeof(double));
    }

    for (j = 0; j < n_times-1; j++) {
        
        for (i = 0; i < n_preds; i++) {
            y[i] = mvals[i][j];
        }

        dTdt(j, 0, n_preds, n_tmals, tmal_ints, tmal_floats, mvals, deriv, y);
        
        for (i = 0; i < n_preds; i++) {
            k1[i] = dt * deriv[i];
            yh[i] = y[i] + 0.5*k1[i];
        }

        dTdt(j, 1, n_preds, n_tmals, tmal_ints, tmal_floats, mvals, deriv, yh);

        for (i = 0; i < n_preds; i++) {
            k2[i] = dt * deriv[i];
        }

        if (rk4 == 0) {
            for (i = 0; i < n_preds; i++) {
                mvals[i][j+1] = y[i] + k2[i];
            }
            continue;
        }
        
        for (i = 0; i < n_preds; i++) {
            yh[i] = y[i] + 0.5*k2[i];
        }

        dTdt(j, 1, n_preds, n_tmals, tmal_ints, tmal_floats, mvals, deriv, yh);

        for (i = 0; i < n_preds; i++) {
            k3[i] = dt * deriv[i];
            yh[i] = y[i] + k3[i];
        }

        dTdt(j+1, 0, n_preds, n_tmals, tmal_ints, tmal_floats, mvals, deriv, yh);

        for (i = 0; i < n_preds; i++) {
            k4[i] = dt * deriv[i];
            mvals[i][j+1] = y[i] + one_sixth*(k1[i]+2.0*(k2[i]+k3[i])+k4[i]);
        }

    }

    free(y);
    free(yh);
    free(deriv);
    free(k1);
    free(k2);
    if (rk4 == 1) {
        free(k3);
        free(k4);
    }

    return 0;
}
