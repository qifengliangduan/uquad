#include <math.h>
#include "uquad_kalman.h"
#include <uquad_types.h>
#include <uquad_config.h>

/// Vars for inertial kalman
uquad_mat_t* Fk_1_intertial = NULL;
uquad_mat_t* Fk_1_T_intertial = NULL;
uquad_mat_t* tmp_intertial = NULL;
uquad_mat_t* tmp2_intertial = NULL;
uquad_mat_t* mtmp_intertial = NULL;
uquad_mat_t* P__intertial = NULL;
uquad_mat_t* H_intertial = NULL;
uquad_mat_t* fx_intertial = NULL;
uquad_mat_t* hx_intertial = NULL;
// Auxiliares Update
uquad_mat_t* yk_intertial = NULL;
uquad_mat_t* HT_intertial = NULL;
uquad_mat_t* HP__intertial = NULL;
uquad_mat_t* HP_HT_intertial = NULL;
uquad_mat_t* Sk_intertial = NULL;
uquad_mat_t* Sk_aux1_intertial = NULL;
uquad_mat_t* Sk_aux2_intertial = NULL;
uquad_mat_t* P_HT_intertial = NULL;
uquad_mat_t* Kk_intertial = NULL;
uquad_mat_t* Kkyk_intertial = NULL;
uquad_mat_t* KkH_intertial = NULL;
uquad_mat_t* IKH_intertial = NULL;
uquad_mat_t* Sk_1_intertial = NULL;
uquad_mat_t* I_intertial = NULL;

/// Var for drag/drive
uquad_mat_t* TM       = NULL;
uquad_mat_t* D        = NULL;
uquad_mat_t* w_2      = NULL;
uquad_mat_t* w_2_tmp  = NULL;
uquad_mat_t* w_2_tmp2 = NULL;

// GPS

// Auxiliaries for h
uquad_mat_t* Rv_gps = NULL;
uquad_mat_t* v_gps  = NULL;
uquad_mat_t* R_gps  = NULL;

// Auxiliaries for Update
uquad_mat_t* fx_gps = NULL;
uquad_mat_t* Fk_1_gps = NULL;
uquad_mat_t* Fk_1_T_gps = NULL;
uquad_mat_t* mtmp_gps = NULL;
uquad_mat_t* P__gps = NULL;

// Auxiliaries for Update
uquad_mat_t* hx_gps = NULL;
uquad_mat_t* z_gps = NULL;
uquad_mat_t* yk_gps = NULL;
uquad_mat_t* H_gps = NULL;
uquad_mat_t* HP__gps  = NULL;
uquad_mat_t* HT_gps   = NULL;
uquad_mat_t* HP_HT_gps = NULL;
uquad_mat_t* Sk_gps   = NULL;
uquad_mat_t* Sk_aux1_gps = NULL;
uquad_mat_t* Sk_aux2_gps = NULL;
uquad_mat_t* Sk_1_gps = NULL;
uquad_mat_t* P_HT_gps = NULL;
uquad_mat_t* Kk_gps   = NULL;
uquad_mat_t* Kkyk_gps = NULL;
uquad_mat_t* x_hat_gps = NULL;
uquad_mat_t* I_gps    = NULL;
uquad_mat_t* KkH_gps  = NULL;
uquad_mat_t* IKH_gps  = NULL;


int H_init(uquad_mat_t** H, uquad_bool_t is_gps)
{
    int retval;
    if(!is_gps)
	*H = uquad_mat_alloc(KALMAN_ROWS_H,STATE_COUNT+STATE_BIAS);
    else
	*H = uquad_mat_alloc(KALMAN_ROWS_H_GPS,STATE_COUNT+STATE_BIAS);
    if(*H == NULL)
    {
	err_check(ERROR_MALLOC, "Failed to allocate H()!");
    }
    retval = uquad_mat_zeros(*H);
    err_propagate(retval);
    (*H)->m[0][3]=1;
    (*H)->m[1][4]=1;
    (*H)->m[2][5]=1;
#if KALMAN_BIAS
    (*H)->m[3][12]=1;
    (*H)->m[4][13]=1;
    (*H)->m[5][14]=1;
#endif // KALMAN_BIAS
    (*H)->m[6][9]=1;
    (*H)->m[7][10]=1;
    (*H)->m[8][11]=1;
    (*H)->m[9][2]=1;
    if(is_gps)
    {
	/// Reorder last terms, use {x,y,z}
	(*H)->m[9][0]  = 1;
	(*H)->m[10][1] = 1;
	(*H)->m[11][2] = 1;
    }
    return ERROR_OK;
}

int store_data(kalman_io_t* kalman_io_data, uquad_mat_t *w, imu_data_t* data, double T, gps_comm_data_t *gps_i_dat)
{
    uquad_mat_t *z = (gps_i_dat == NULL)?
	kalman_io_data->z:
	kalman_io_data->z_gps;
    int retval;
    retval = uquad_mat_copy(kalman_io_data->u, w);
    err_propagate(retval);

    retval = uquad_mat_set_subm(z, 0, 0, data->magn);
    err_propagate(retval);

    retval = uquad_mat_set_subm(z, 3, 0, data->acc);
    err_propagate(retval);

    retval = uquad_mat_set_subm(z, 6, 0, data->gyro);
    err_propagate(retval);

    if(gps_i_dat != NULL)
    {
	retval = uquad_mat_set_subm(z, 9, 0, gps_i_dat->pos);
	err_propagate(retval);
    }
    else
    {
	z->m_full[9] = data->alt;
    }

    kalman_io_data->T = T/1000000;

    return ERROR_OK;
}

/**
 * Calculates drag() torque, using:
 *    drag = w^2*DRAG_A2 + w*DRAG_A1
 *
 * NOTE: Aux mem can be either all NULL, or all of size (LENGTH_INPUTx1)
 *
 * @param drag answer (LENGTH_INPUTx1)
 * @param w input (LENGTH_INPUTx1)
 * @param w_2 aux mem or NULL
 * @param w_2_tmp aux mem or NULL
 * @param w_2_tmp2 aux mem or NULL
 *
 * @return error code
 */
int drag(uquad_mat_t *drag, uquad_mat_t *w,
	 uquad_mat_t *w_2,
	 uquad_mat_t *w_2_tmp,
	 uquad_mat_t *w_2_tmp2)
{  
    int retval;
    uquad_bool_t local_mem = false;
    if(w_2 == NULL || w_2_tmp == NULL || w_2_tmp2 == NULL)
    {
	/**
	 * Global vars used for tmp memory.
	 * If they are not defined, define them.
	 */
	if(w_2 != NULL || w_2_tmp != NULL || w_2_tmp2 != NULL)
	{
	    err_check(ERROR_INVALID_ARG,"Aux mem must be all or none!");
	}
	w_2      = uquad_mat_alloc(LENGTH_INPUT,1);
	w_2_tmp  = uquad_mat_alloc(LENGTH_INPUT,1);
	w_2_tmp2 = uquad_mat_alloc(LENGTH_INPUT,1);
	local_mem = true;
	if((w_2 == NULL) || (w_2_tmp == NULL) || (w_2_tmp2 == NULL))
	{
	    cleanup_log_if(ERROR_MALLOC, "Failed to allocate mem for drag()!");
	}
    }
    retval = uquad_mat_dot_product(w_2,w,w);
    err_propagate(retval);
    retval = uquad_mat_scalar_mul(w_2_tmp, w_2, DRAG_A1);
    err_propagate(retval);
    retval = uquad_mat_scalar_mul(w_2_tmp2, w, DRAG_A2);
    err_propagate(retval);
    retval = uquad_mat_add(drag,w_2_tmp,w_2_tmp2);
    err_propagate(retval);
    cleanup:
    if(local_mem)
    {
	uquad_mat_free(w_2);
	uquad_mat_free(w_2_tmp);
	uquad_mat_free(w_2_tmp2);
    }
    return retval;
}

/**
 * Calculates drive using:
 *    drive = w^2*DRIVE_A2 + w*DRIVE_A1
 *
 * NOTE: Aux mem can be either all NULL, or all of size (LENGTH_INPUTx1)
 *
 * @param drive answer
 * @param w input
 * @param w_2 aux mem or NULL
 * @param w_2_tmp aux mem or NULL
 * @param w_2_tmp2 aux mem or NULL
 *
 * @return error code
 */
int drive(uquad_mat_t *drive, uquad_mat_t *w,
	 uquad_mat_t *w_2,
	 uquad_mat_t *w_2_tmp,
	 uquad_mat_t *w_2_tmp2)
{  
    int retval;
    uquad_bool_t local_mem = false;
    if(w_2 == NULL || w_2_tmp == NULL || w_2_tmp2 == NULL)
    {
	/**
	 * Global vars used for tmp memory.
	 * If they are not defined, define them.
	 */
	if(w_2 != NULL || w_2_tmp != NULL || w_2_tmp2 != NULL)
	{
	    err_check(ERROR_INVALID_ARG,"Aux mem must be all or none!");
	}
	w_2      = uquad_mat_alloc(LENGTH_INPUT,1);
	w_2_tmp  = uquad_mat_alloc(LENGTH_INPUT,1);
	w_2_tmp2 = uquad_mat_alloc(LENGTH_INPUT,1);
	local_mem = true;
	if((w_2 == NULL) || (w_2_tmp == NULL) || (w_2_tmp2 == NULL))
	{
	    cleanup_log_if(ERROR_MALLOC, "Failed to allocate mem for drag()!");
	}
    }
    retval = uquad_mat_dot_product(w_2,w,w);
    err_propagate(retval);
    retval = uquad_mat_scalar_mul(w_2_tmp, w_2, DRIVE_A1);
    err_propagate(retval);
    retval = uquad_mat_scalar_mul(w_2_tmp2, w, DRIVE_A2);
    err_propagate(retval);
    retval = uquad_mat_add(drive,w_2_tmp,w_2_tmp2);
    err_propagate(retval);
    cleanup:
    if(local_mem)
    {
	uquad_mat_free(w_2);
	uquad_mat_free(w_2_tmp);
	uquad_mat_free(w_2_tmp2);
    }
    return retval;
}

int f(uquad_mat_t* fx, kalman_io_t* kalman_io_data)
{
    double x     = kalman_io_data -> x_hat -> m_full[0];
    double y     = kalman_io_data -> x_hat -> m_full[1];
    double z     = kalman_io_data -> x_hat -> m_full[2];
    double psi   = kalman_io_data -> x_hat -> m_full[3];
    double phi   = kalman_io_data -> x_hat -> m_full[4];
    double theta = kalman_io_data -> x_hat -> m_full[5];
    double vqx   = kalman_io_data -> x_hat -> m_full[6];
    double vqy   = kalman_io_data -> x_hat -> m_full[7];
    double vqz   = kalman_io_data -> x_hat -> m_full[8];
    double wqx   = kalman_io_data -> x_hat -> m_full[9];
    double wqy   = kalman_io_data -> x_hat -> m_full[10];
    double wqz   = kalman_io_data -> x_hat -> m_full[11];
    double abx   = 0;
    double aby   = 0;
    double abz   = 0;
    double T     = kalman_io_data -> T;

#if KALMAN_BIAS
    abx   = kalman_io_data -> x_hat -> m_full[12];
    aby   = kalman_io_data -> x_hat -> m_full[13];
    abz   = kalman_io_data -> x_hat -> m_full[14];
#endif // KALMAN_BIAS

    int retval;
    double* w    = kalman_io_data -> u -> m_full;
    uquad_mat_t* w_mat    = kalman_io_data -> u;
    retval =  drive(TM,w_mat,w_2,w_2_tmp,w_2_tmp2);
    err_propagate(retval);
    retval =  drag(D,w_mat,w_2,w_2_tmp,w_2_tmp2);
    err_propagate(retval);
    double* TM_vec = TM->m_full;
    double* D_vec = D->m_full;

    fx->m_full[0]  = x     + T *(vqx*cos(phi)*cos(theta)+vqy*(cos(theta)*sin(phi)*sin(psi)-cos(phi)*sin(theta))+vqz*(sin(psi)*sin(theta)+cos(psi)*cos(theta)*sin(phi)) ) ;
    fx->m_full[1]  = y     + T *(vqx*cos(phi)*sin(theta)+vqy*(sin(theta)*sin(phi)*sin(psi)+cos(psi)*cos(theta))+vqz*(cos(psi)*sin(theta)*sin(phi)-cos(theta)*sin(psi)) ) ;
    fx->m_full[2]  = z     + T *(-vqx*sin(phi)+vqy*cos(phi)*sin(psi)+vqz*cos(psi)*cos(psi));
    fx->m_full[3]  = psi   + T*( wqx+wqz*tan(phi)*cos(psi)+wqy*tan(phi)*sin(psi));
    fx->m_full[4]  = phi   + T*( wqy*cos(psi)-wqz*sin(psi));
    fx->m_full[5]  = theta + T*( wqz*cos(psi)/cos(phi)+wqy*sin(psi)/cos(phi));
    fx->m_full[6]  = vqx   + T*( vqy*wqz-vqz*wqy+GRAVITY*sin(phi)+abx);
    fx->m_full[7]  = vqy   + T*( vqz*wqx-vqx*wqz-GRAVITY*cos(phi)*sin(psi)+aby);
    fx->m_full[8]  = vqz   + T*( vqx*wqy-vqy*wqx-GRAVITY*cos(phi)*cos(psi)+1/MASA*(TM_vec[0]+TM_vec[1]+TM_vec[2]+TM_vec[3])+abz);
    fx->m_full[9] = wqx   + T*( wqy*wqz*(IYY-IZZ)+wqy*IZZM*(w[0]-w[1]+w[2]-w[3])+LENGTH*(TM_vec[1]-TM_vec[3]) )/IXX ;
    fx->m_full[10] = wqy   + T*( wqx*wqz*(IZZ-IXX)+wqx*IZZM*(w[0]-w[1]+w[2]-w[3])+LENGTH*(TM_vec[2]-TM_vec[0]) )/IYY;
    // fx->m_full[11] = wqz   + T*( -IZZM*(dw[0]-dw[1]+dw[2]-dw[3])+D[0]-D[1]+D[2]-D[3] )/IZZ;
    fx->m_full[11] = wqz   - T*( D_vec[0]-D_vec[1]+D_vec[2]-D_vec[3] )/IZZ;
#if KALMAN_BIAS
    fx->m_full[12] = abx;
    fx->m_full[13] = aby;
    fx->m_full[14] = abz;
#endif // KALMAN_BIAS

    return ERROR_OK;
}

int h(uquad_mat_t* hx, kalman_io_t* kalman_io_data, uquad_bool_t is_gps)
{
    int retval;
    if(TM == NULL)
    {
	TM = uquad_mat_alloc(4,1); // TODO verificar que hay memoria y se ejecuto la sentencia correctamente
	D = uquad_mat_alloc(4,1);
    }
    retval = drive(TM,kalman_io_data->u,w_2,w_2_tmp,w_2_tmp2);
    err_propagate(retval);
    double* TM_vec = TM -> m_full;
    double
	abx = 0,
	aby = 0,
	abz = 0;
#if KALMAN_BIAS
    abx = kalman_io_data -> x_ -> m_full[12];
    aby = kalman_io_data -> x_ -> m_full[13];
    abz = kalman_io_data -> x_ -> m_full[14];
#endif
    hx->m_full[0]  = kalman_io_data -> x_ -> m_full[3];
    hx->m_full[1]  = kalman_io_data -> x_ -> m_full[4];
    hx->m_full[2]  = kalman_io_data -> x_ -> m_full[5];
    hx->m_full[3]  = abx;
    hx->m_full[4]  = aby;
    hx->m_full[5]  = 1/MASA*(TM_vec[0]+TM_vec[1]+TM_vec[2]+TM_vec[3]) + abz;
    hx->m_full[6]  = kalman_io_data -> x_ -> m_full[9];
    hx->m_full[7]  = kalman_io_data -> x_ -> m_full[10];
    hx->m_full[8]  = kalman_io_data -> x_ -> m_full[11];
    hx->m_full[9] = kalman_io_data -> x_ -> m_full[2];
    if(is_gps)
    {
    hx->m_full[10] = kalman_io_data -> x_ -> m_full[0];
    hx->m_full[11] = kalman_io_data -> x_ -> m_full[1];
    hx->m_full[12] = kalman_io_data -> x_ -> m_full[2];
    }
    return ERROR_OK; 
}

int F(uquad_mat_t* Fx, kalman_io_t* kalman_io_data)
{
    // unused vars
    //    double x     = kalman_io_data -> x_hat -> m_full[0];
    //    double y     = kalman_io_data -> x_hat -> m_full[1];
    //    double z     = kalman_io_data -> x_hat -> m_full[2];
    double psi   = kalman_io_data -> x_hat -> m_full[3];
    double phi   = kalman_io_data -> x_hat -> m_full[4];
    double theta = kalman_io_data -> x_hat -> m_full[5];
    double vqx   = kalman_io_data -> x_hat -> m_full[6];
    double vqy   = kalman_io_data -> x_hat -> m_full[7];
    double vqz   = kalman_io_data -> x_hat -> m_full[8];
    double wqx   = kalman_io_data -> x_hat -> m_full[9];
    double wqy   = kalman_io_data -> x_hat -> m_full[10];
    double wqz   = kalman_io_data -> x_hat -> m_full[11];
    double T     = kalman_io_data -> T;

    double* w    = kalman_io_data -> u -> m_full;
    uquad_mat_t* w_t    = kalman_io_data -> u;
    int retval;

    retval =  drive(TM,w_t,w_2,w_2_tmp,w_2_tmp2);
    err_propagate(retval);
    retval =  drag(D,w_t,w_2,w_2_tmp,w_2_tmp2);
    err_propagate(retval);

    Fx->m[0][0] = 1;
    Fx->m[0][1] = 0;
    Fx->m[0][2] = 0;
    Fx->m[0][3] = T*(vqz*(cos(psi)*sin(theta) - cos(theta)*sin(phi)*sin(psi)) + vqy*cos(psi)*cos(theta)*sin(phi));
    Fx->m[0][4] = T*(vqy*(sin(phi)*sin(theta) + cos(phi)*cos(theta)*sin(psi)) - vqx*cos(theta)*sin(phi) + vqz*cos(phi)*cos(psi)*cos(theta));
    Fx->m[0][5] = -T*(vqy*(cos(phi)*cos(theta) + sin(phi)*sin(psi)*sin(theta)) - vqz*(cos(theta)*sin(psi) - cos(psi)*sin(phi)*sin(theta)) + vqx*cos(phi)*sin(theta));
    Fx->m[0][6] = T*cos(phi)*cos(theta);
    Fx->m[0][7] = -T*(cos(phi)*sin(theta) - cos(theta)*sin(phi)*sin(psi));
    Fx->m[0][8] = T*(sin(psi)*sin(theta) + cos(psi)*cos(theta)*sin(phi));
    Fx->m[0][9] = 0;
    Fx->m[0][10] = 0;
    Fx->m[0][11] = 0;

    Fx->m[1][0] = 0;
    Fx->m[1][1] = 1;
    Fx->m[1][2] = 0;
    Fx->m[1][3] = -T*(vqy*(cos(theta)*sin(psi) - cos(psi)*sin(phi)*sin(theta)) + vqz*(cos(psi)*cos(theta) + sin(phi)*sin(psi)*sin(theta)));
    Fx->m[1][4] = T*(vqz*cos(phi)*cos(psi)*sin(theta) - vqx*sin(phi)*sin(theta) + vqy*cos(phi)*sin(psi)*sin(theta));
    Fx->m[1][5] = T*(vqz*(sin(psi)*sin(theta) + cos(psi)*cos(theta)*sin(phi)) - vqy*(cos(psi)*sin(theta) - cos(theta)*sin(phi)*sin(psi)) + vqx*cos(phi)*cos(theta));
    Fx->m[1][6] = T*cos(phi)*sin(theta);
    Fx->m[1][7] = T*(cos(psi)*cos(theta) + sin(phi)*sin(psi)*sin(theta));
    Fx->m[1][8] = -T*(cos(theta)*sin(psi) - cos(psi)*sin(phi)*sin(theta));
    Fx->m[1][9] = 0;
    Fx->m[1][10] = 0;
    Fx->m[1][11] = 0;

    Fx->m[2][0] = 0;
    Fx->m[2][1] = 0;
    Fx->m[2][2] = 1;
    Fx->m[2][3] = T*(vqy*cos(phi)*cos(psi) - 2*vqz*cos(psi)*sin(psi));
    Fx->m[2][4] = -T*(vqx*cos(phi) + vqy*sin(phi)*sin(psi));
    Fx->m[2][5] = 0;
    Fx->m[2][6] = -T*sin(phi);
    Fx->m[2][7] = T*cos(phi)*sin(psi);
    Fx->m[2][8] = T*uquad_square(cos(psi));
    Fx->m[2][9] = 0;
    Fx->m[2][10] = 0;
    Fx->m[2][11] = 0;

    Fx->m[3][0] = 0;
    Fx->m[3][1] = 0;
    Fx->m[3][2] = 0;
    Fx->m[3][3] = T*(wqy*cos(psi)*tan(phi) - wqz*sin(psi)*tan(phi)) + 1;
    Fx->m[3][4] = T*(wqz*cos(psi)*(uquad_square(tan(phi)) + 1) + wqy*sin(psi)*(uquad_square(tan(phi)) + 1));
    Fx->m[3][5] = 0;
    Fx->m[3][6] = 0;
    Fx->m[3][7] = 0;
    Fx->m[3][8] = 0;
    Fx->m[3][9] = T;
    Fx->m[3][10] = T*sin(psi)*tan(phi);
    Fx->m[3][11] = T*cos(psi)*tan(phi);

    Fx->m[4][0] = 0;
    Fx->m[4][1] = 0;
    Fx->m[4][2] = 0;
    Fx->m[4][3] = -T*(wqz*cos(psi) + wqy*sin(psi));
    Fx->m[4][4] = 1;
    Fx->m[4][5] = 0;
    Fx->m[4][6] = 0;
    Fx->m[4][7] = 0;
    Fx->m[4][8] = 0;
    Fx->m[4][9] = 0;
    Fx->m[4][10] = T*cos(psi);
    Fx->m[4][11] = -T*sin(psi);

    Fx->m[5][0] = 0;
    Fx->m[5][1] = 0;
    Fx->m[5][2] = 0;
    Fx->m[5][3] = T*((wqy*cos(psi))/cos(phi) - (wqz*sin(psi))/cos(phi));
    Fx->m[5][4] = T*((wqz*cos(psi)*sin(phi))/uquad_square(cos(phi)) + (wqy*sin(phi)*sin(psi))/uquad_square(cos(phi)));
    Fx->m[5][5] = 1;
    Fx->m[5][6] = 0;
    Fx->m[5][7] = 0;
    Fx->m[5][8] = 0;
    Fx->m[5][9] = 0;
    Fx->m[5][10] = (T*sin(psi))/cos(phi);
    Fx->m[5][11] = (T*cos(psi))/cos(phi);

    Fx->m[6][0] = 0;
    Fx->m[6][1] = 0;
    Fx->m[6][2] = 0;
    Fx->m[6][3] = 0;
    Fx->m[6][4] = T*GRAVITY*cos(phi);
    Fx->m[6][5] = 0;
    Fx->m[6][6] = 1;
    Fx->m[6][7] = T*wqz;
    Fx->m[6][8] = -T*wqy;
    Fx->m[6][9] = 0;
    Fx->m[6][10] = -T*vqz;
    Fx->m[6][11] = T*vqy;

    Fx->m[7][0] = 0;
    Fx->m[7][1] = 0;
    Fx->m[7][2] = 0;
    Fx->m[7][3] = -T*GRAVITY*cos(phi)*cos(psi);
    Fx->m[7][4] = T*GRAVITY*sin(phi)*sin(psi);
    Fx->m[7][5] = 0;
    Fx->m[7][6] = -T*wqz;
    Fx->m[7][7] = 1;
    Fx->m[7][8] = T*wqx;
    Fx->m[7][9] = T*vqz;
    Fx->m[7][10] = 0;
    Fx->m[7][11] = -T*vqx;

    Fx->m[8][0] = 0;
    Fx->m[8][1] = 0;
    Fx->m[8][2] = 0;
    Fx->m[8][3] = T*GRAVITY*cos(phi)*sin(psi);
    Fx->m[8][4] = T*GRAVITY*cos(psi)*sin(phi);
    Fx->m[8][5] = 0;
    Fx->m[8][6] = T*wqy;
    Fx->m[8][7] = -T*wqx;
    Fx->m[8][8] = 1;
    Fx->m[8][9] = -T*vqy;
    Fx->m[8][10] = T*vqx;
    Fx->m[8][11] = 0;

    Fx->m[9][0] = 0;
    Fx->m[9][1] = 0;
    Fx->m[9][2] = 0;
    Fx->m[9][3] = 0;
    Fx->m[9][4] = 0;
    Fx->m[9][5] = 0;
    Fx->m[9][6] = 0;
    Fx->m[9][7] = 0;
    Fx->m[9][8] = 0;
    Fx->m[9][9] = 1;
    Fx->m[9][10] = (T*(wqz*(IYY - IZZ) + IZZM*(w[0] - w[1] + w[2] - w[3])))/IXX;
    Fx->m[9][11] = (T*wqy*(IYY - IZZ))/IXX;

    Fx->m[10][0] = 0;
    Fx->m[10][1] = 0;
    Fx->m[10][2] = 0;
    Fx->m[10][3] = 0;
    Fx->m[10][4] = 0;
    Fx->m[10][5] = 0;
    Fx->m[10][6] = 0;
    Fx->m[10][7] = 0;
    Fx->m[10][8] = 0;
    Fx->m[10][9] = -(T*(wqz*(IXX - IZZ) - IZZM*(w[0]-w[1]+w[2]-w[3])))/IYY;
    Fx->m[10][10] = 1;
    Fx->m[10][11] = -(T*wqx*(IXX - IZZ))/IYY;

    Fx->m[11][0] = 0;
    Fx->m[11][1] = 0;
    Fx->m[11][2] = 0;
    Fx->m[11][3] = 0;
    Fx->m[11][4] = 0;
    Fx->m[11][5] = 0;
    Fx->m[11][6] = 0;
    Fx->m[11][7] = 0;
    Fx->m[11][8] = 0;
    Fx->m[11][9] = 0;
    Fx->m[11][10] = 0;
    Fx->m[11][11] = 1;

#if KALMAN_BIAS
    Fx->m[0][12] = 0;
    Fx->m[0][13] = 0;
    Fx->m[0][14] = 0;

    Fx->m[1][12] = 0;
    Fx->m[1][13] = 0;
    Fx->m[1][14] = 0;

    Fx->m[2][12] = 0;
    Fx->m[2][13] = 0;
    Fx->m[2][14] = 0;

    Fx->m[3][12] = T;
    Fx->m[3][13] = 0;
    Fx->m[3][14] = 0;

    Fx->m[4][12] = 0;
    Fx->m[4][13] = 0;
    Fx->m[4][14] = 0;

    Fx->m[5][12] = 0;
    Fx->m[5][13] = 0;
    Fx->m[5][14] = 0;

    Fx->m[6][12] = T;
    Fx->m[6][13] = 0;
    Fx->m[6][14] = 0;

    Fx->m[7][12] = 0;
    Fx->m[7][13] = T;
    Fx->m[7][14] = 0;

    Fx->m[8][12] = 0;
    Fx->m[8][13] = 0;
    Fx->m[8][14] = T;

    Fx->m[9][12] = 0;
    Fx->m[9][13] = 0;
    Fx->m[9][14] = 0;

    Fx->m[10][12] = 0;
    Fx->m[10][13] = 0;
    Fx->m[10][14] = 0;

    Fx->m[11][12] = 0;
    Fx->m[11][13] = 0;
    Fx->m[11][14] = 0;

    Fx->m[12][0] = 0;
    Fx->m[12][1] = 0;
    Fx->m[12][2] = 0;
    Fx->m[12][3] = 0;
    Fx->m[12][4] = 0;
    Fx->m[12][5] = 0;
    Fx->m[12][6] = 0;
    Fx->m[12][7] = 0;
    Fx->m[12][8] = 0;
    Fx->m[12][9] = 0;
    Fx->m[12][10] = 0;
    Fx->m[12][11] = 0;
    Fx->m[12][12] = 1;
    Fx->m[12][13] = 0;
    Fx->m[12][14] = 0;

    Fx->m[13][0] = 0;
    Fx->m[13][1] = 0;
    Fx->m[13][2] = 0;
    Fx->m[13][3] = 0;
    Fx->m[13][4] = 0;
    Fx->m[13][5] = 0;
    Fx->m[13][6] = 0;
    Fx->m[13][7] = 0;
    Fx->m[13][8] = 0;
    Fx->m[13][9] = 0;
    Fx->m[13][10] = 0;
    Fx->m[13][11] = 0;
    Fx->m[13][12] = 0;
    Fx->m[13][13] = 1;
    Fx->m[13][14] = 0;

    Fx->m[14][0] = 0;
    Fx->m[14][1] = 0;
    Fx->m[14][2] = 0;
    Fx->m[14][3] = 0;
    Fx->m[14][4] = 0;
    Fx->m[14][5] = 0;
    Fx->m[14][6] = 0;
    Fx->m[14][7] = 0;
    Fx->m[14][8] = 0;
    Fx->m[14][9] = 0;
    Fx->m[14][10] = 0;
    Fx->m[14][11] = 0;
    Fx->m[14][12] = 0;
    Fx->m[14][13] = 0;
    Fx->m[14][14] = 1;

#endif // KALMAN_BIAS

    return ERROR_OK;
}

void uquad_kalman_inertial_aux_mem_deinit(void)
{
    uquad_mat_free(Fk_1_intertial);
    uquad_mat_free(Fk_1_T_intertial);
    uquad_mat_free(mtmp_intertial);
    uquad_mat_free(P__intertial);
    uquad_mat_free(fx_intertial);
    uquad_mat_free(hx_intertial);
    uquad_mat_free(yk_intertial);
    uquad_mat_free(H_intertial);
    uquad_mat_free(HT_intertial);
    uquad_mat_free(HP__intertial);
    uquad_mat_free(HP_HT_intertial);
    uquad_mat_free(Sk_intertial);
    uquad_mat_free(Sk_1_intertial);
    uquad_mat_free(Sk_aux1_intertial);
    uquad_mat_free(Sk_aux2_intertial);
    uquad_mat_free(P_HT_intertial);
    uquad_mat_free(Kk_intertial);
    uquad_mat_free(Kkyk_intertial);
    uquad_mat_free(KkH_intertial);
    uquad_mat_free(IKH_intertial);
    uquad_mat_free(I_intertial);
}

int uquad_kalman_inertial_aux_mem_init(void)
{
    int retval;
    if(Fk_1_intertial != NULL)
    {
	err_check(ERROR_FAIL,"Memory has already been allocated!");
    }
    Fk_1_intertial   = uquad_mat_alloc(STATE_COUNT+STATE_BIAS,STATE_COUNT+STATE_BIAS);
    Fk_1_T_intertial = uquad_mat_alloc(STATE_COUNT+STATE_BIAS,STATE_COUNT+STATE_BIAS);
    mtmp_intertial   = uquad_mat_alloc(STATE_COUNT+STATE_BIAS,STATE_COUNT+STATE_BIAS);
    P__intertial     = uquad_mat_alloc(STATE_COUNT+STATE_BIAS,STATE_COUNT+STATE_BIAS);
    retval = H_init(&H_intertial, false);
    err_propagate(retval);
    fx_intertial     = uquad_mat_alloc(STATE_COUNT+STATE_BIAS+3,1);
    hx_intertial     = uquad_mat_alloc(10,1);

    // Auxiliares para el update
    yk_intertial     = uquad_mat_alloc(hx_intertial->r,hx_intertial->c);
    HT_intertial     = uquad_mat_alloc(H_intertial->c, H_intertial->r);
    HP__intertial    = uquad_mat_alloc(H_intertial->r,P__intertial->c);
    HP_HT_intertial  = uquad_mat_alloc(H_intertial->r,H_intertial->r);
    Sk_intertial     = uquad_mat_alloc(HP_HT_intertial->r,HP_HT_intertial->c);
    Sk_1_intertial   = uquad_mat_alloc(Sk_intertial->r,Sk_intertial->c);
    if(Sk_intertial != NULL)
    {
	Sk_aux1_intertial = uquad_mat_alloc(Sk_intertial->r,Sk_intertial->c);
	Sk_aux2_intertial = uquad_mat_alloc(Sk_intertial->r,Sk_intertial->c << 1);
    }
    P_HT_intertial   = uquad_mat_alloc(P__intertial->r, H_intertial->r);
    Kk_intertial     = uquad_mat_alloc(P_HT_intertial->r,Sk_1_intertial->c);
    Kkyk_intertial   = uquad_mat_alloc(Kk_intertial->r,yk_intertial->c);
    KkH_intertial    = uquad_mat_alloc(Kk_intertial->r, H_intertial->c);
    IKH_intertial    = uquad_mat_alloc(KkH_intertial->r, KkH_intertial->c);
    I_intertial      = uquad_mat_alloc(KkH_intertial->r, KkH_intertial->c);
    if(Fk_1_intertial     == NULL ||
       Fk_1_T_intertial   == NULL ||
       mtmp_intertial     == NULL ||
       P__intertial       == NULL ||
       fx_intertial       == NULL ||
       hx_intertial       == NULL ||
       yk_intertial       == NULL ||
       HT_intertial       == NULL ||
       HP__intertial      == NULL ||
       HP_HT_intertial    == NULL ||
       Sk_intertial       == NULL ||
       Sk_1_intertial     == NULL ||
       Sk_aux1_intertial  == NULL ||
       Sk_aux2_intertial  == NULL ||
       P_HT_intertial     == NULL ||
       Kk_intertial       == NULL ||
       Kkyk_intertial     == NULL ||
       KkH_intertial      == NULL ||
       IKH_intertial      == NULL ||
       I_intertial        == NULL)
    {
	err_check(ERROR_MALLOC,"Failed to allocate mem for inertial kalman!");
    }
    return retval;
}

void uquad_kalman_gps_aux_mem_deinit(void)
{
    err_log("NOT IMPLEMENTED!");
}


int uquad_kalman_gps_aux_mem_init(void)
{
    int retval;
    if(Fk_1_gps != NULL)
    {
	err_check(ERROR_FAIL,"Memory has already been allocated!");
    }
    // Auxiliaries for prediction
    Fk_1_gps   = uquad_mat_alloc(STATE_COUNT + STATE_BIAS,STATE_COUNT + STATE_BIAS);
    Fk_1_T_gps = uquad_mat_alloc(STATE_COUNT + STATE_BIAS,STATE_COUNT + STATE_BIAS);
    mtmp_gps   = uquad_mat_alloc(STATE_COUNT + STATE_BIAS,STATE_COUNT + STATE_BIAS);
    P__gps     = uquad_mat_alloc(STATE_COUNT + STATE_BIAS,STATE_COUNT + STATE_BIAS);
    retval = H_init(&H_gps, true);
    err_propagate(retval);
    // Auxiliaries for update
    fx_gps        = uquad_mat_alloc(STATE_COUNT + STATE_BIAS, 1);
    hx_gps        = uquad_mat_alloc(STATE_COUNT,1);
    z_gps         = uquad_mat_alloc(STATE_COUNT,1);
    yk_gps     = uquad_mat_alloc(hx_gps->r,hx_gps->c);
    HT_gps     = uquad_mat_alloc(H_gps->c, H_gps->r);
    HP__gps    = uquad_mat_alloc(H_gps->r,P__gps->c);
    HP_HT_gps  = uquad_mat_alloc(H_gps->r,H_gps->r);
    Sk_gps     = uquad_mat_alloc(HP_HT_gps->r,HP_HT_gps->c);
    Sk_1_gps   = uquad_mat_alloc(Sk_gps->r,Sk_gps->c);
    if(Sk_gps != NULL)
    {
	Sk_aux1_gps = uquad_mat_alloc(Sk_gps->r,Sk_gps->c);
	Sk_aux2_gps = uquad_mat_alloc(Sk_gps->r,Sk_gps->c << 1);
    }
    P_HT_gps   = uquad_mat_alloc(P__gps->r, H_gps->r);
    Kk_gps     = uquad_mat_alloc(P_HT_gps->r,Sk_1_gps->c);
    Kkyk_gps   = uquad_mat_alloc(Kk_gps->r,yk_gps->c);
    KkH_gps    = uquad_mat_alloc(Kk_gps->r, H_gps->c);
    IKH_gps    = uquad_mat_alloc(KkH_gps->r, KkH_gps->c);
    I_gps      = uquad_mat_alloc(KkH_gps->r, KkH_gps->c);

    err_log("NOT IMPLEMENTED CORRECTLY!");
    return ERROR_OK;
}

void uquad_kalman_drive_drag_aux_mem_deinit(void)
{
    uquad_mat_free(w_2);
    uquad_mat_free(w_2_tmp);
    uquad_mat_free(w_2_tmp2);
    uquad_mat_free(TM);
    uquad_mat_free(D);
}

int uquad_kalman_drive_drag_aux_mem_init(void)
{
    w_2      = uquad_mat_alloc(LENGTH_INPUT,1);
    w_2_tmp  = uquad_mat_alloc(LENGTH_INPUT,1);
    w_2_tmp2 = uquad_mat_alloc(LENGTH_INPUT,1);
    TM       = uquad_mat_alloc(LENGTH_INPUT,1);
    D        = uquad_mat_alloc(LENGTH_INPUT,1);
    if(w_2 == NULL || w_2_tmp == NULL || w_2_tmp2 == NULL)
    {
	err_check(ERROR_MALLOC,"Failed to allocate memory!");
    }
    return ERROR_OK;
}

kalman_io_t* kalman_init()
{
    int retval;
    kalman_io_t* kalman_io_data = (kalman_io_t*)malloc(sizeof(kalman_io_t));
    kalman_io_data->x_hat = uquad_mat_alloc(STATE_COUNT+STATE_BIAS,1);
    kalman_io_data->x_    = uquad_mat_alloc(STATE_COUNT+STATE_BIAS,1);
    kalman_io_data->u     = uquad_mat_alloc(LENGTH_INPUT,1);
    kalman_io_data->z     = uquad_mat_alloc(KALMAN_ROWS_H,1);
    kalman_io_data->z_gps = uquad_mat_alloc(STATE_COUNT,1);
    kalman_io_data->Q     = uquad_mat_alloc(STATE_COUNT+STATE_BIAS,STATE_COUNT+STATE_BIAS);
    kalman_io_data->R     = uquad_mat_alloc(10,10);
    kalman_io_data->P     = uquad_mat_alloc(STATE_COUNT+STATE_BIAS,STATE_COUNT+STATE_BIAS);
    kalman_io_data->Q_gps = uquad_mat_alloc(KALMAN_GPS_SIZE, KALMAN_GPS_SIZE);
    kalman_io_data->R_gps = uquad_mat_alloc(STATE_COUNT,STATE_COUNT);
    kalman_io_data->P_gps = uquad_mat_alloc(KALMAN_GPS_SIZE, KALMAN_GPS_SIZE);

    retval = uquad_mat_zeros(kalman_io_data->x_hat);
    cleanup_if(retval);
    retval = uquad_mat_zeros(kalman_io_data->x_);
    cleanup_if(retval);
    retval = uquad_mat_zeros(kalman_io_data->u);
    cleanup_if(retval);
    retval = uquad_mat_zeros(kalman_io_data->Q);
    cleanup_if(retval);
    retval = uquad_mat_zeros(kalman_io_data->R);
    cleanup_if(retval);
    retval = uquad_mat_zeros(kalman_io_data->P);
    cleanup_if(retval);
    retval = uquad_mat_zeros(kalman_io_data->Q_gps);
    cleanup_if(retval);
    retval = uquad_mat_zeros(kalman_io_data->R_gps);
    cleanup_if(retval);
    retval = uquad_mat_zeros(kalman_io_data->P_gps);
    cleanup_if(retval);
 
    kalman_io_data->Q->m[0][0] = 100;
    kalman_io_data->Q->m[1][1] = 100;
    kalman_io_data->Q->m[2][2] = 100;
    kalman_io_data->Q->m[3][3] = 1;
    kalman_io_data->Q->m[4][4] = 1;
    kalman_io_data->Q->m[5][5] = 1;
    kalman_io_data->Q->m[6][6] = 100;
    kalman_io_data->Q->m[7][7] = 100;
    kalman_io_data->Q->m[8][8] = 100;
    kalman_io_data->Q->m[9][9] = 10;
    kalman_io_data->Q->m[10][10] = 10;
    kalman_io_data->Q->m[11][11] = 10;
#if KALMAN_BIAS
    kalman_io_data->Q->m[12][12] = 1;
    kalman_io_data->Q->m[13][13] = 1;
    kalman_io_data->Q->m[14][14] = 1;
#endif // KALMAN_BIAS

    kalman_io_data->R->m[0][0] = 1000;
    kalman_io_data->R->m[1][1] = 1000;
    kalman_io_data->R->m[2][2] = 1000;
    kalman_io_data->R->m[3][3] = 10000;
    kalman_io_data->R->m[4][4] = 10000;
    kalman_io_data->R->m[5][5] = 10000;
    kalman_io_data->R->m[6][6] = 100;
    kalman_io_data->R->m[7][7] = 100;
    kalman_io_data->R->m[8][8] = 100;
    kalman_io_data->R->m[9][9] = 1000;

    kalman_io_data->P->m[0][0] = 1;
    kalman_io_data->P->m[1][1] = 1;
    kalman_io_data->P->m[2][2] = 1;
    kalman_io_data->P->m[3][3] = 1;
    kalman_io_data->P->m[4][4] = 1;
    kalman_io_data->P->m[5][5] = 1;
    kalman_io_data->P->m[6][6] = 1;
    kalman_io_data->P->m[7][7] = 1;
    kalman_io_data->P->m[8][8] = 1;
    kalman_io_data->P->m[9][9] = 1;
    kalman_io_data->P->m[10][10] = 1;
    kalman_io_data->P->m[11][11] = 1;
#if KALMAN_BIAS
    kalman_io_data->P->m[12][12] = 1;
    kalman_io_data->P->m[13][13] = 1;
    kalman_io_data->P->m[14][14] = 1;
#endif // KALMAN_BIAS

    kalman_io_data->Q_gps->m[0][0] = 100;
    kalman_io_data->Q_gps->m[1][1] = 100;
    kalman_io_data->Q_gps->m[2][2] = 100;
    kalman_io_data->Q_gps->m[3][3] = 100;
    kalman_io_data->Q_gps->m[4][4] = 100;
    kalman_io_data->Q_gps->m[5][5] = 100;

    kalman_io_data->R_gps->m[0][0] = 1000;
    kalman_io_data->R_gps->m[1][1] = 1000;
    kalman_io_data->R_gps->m[2][2] = 1000;
    kalman_io_data->R_gps->m[3][3] = 10000;
    kalman_io_data->R_gps->m[4][4] = 10000;
    kalman_io_data->R_gps->m[5][5] = 10000;
    kalman_io_data->R_gps->m[6][6] = 100;
    kalman_io_data->R_gps->m[7][7] = 100;
    kalman_io_data->R_gps->m[8][8] = 100;
    kalman_io_data->R_gps->m[9][9] = 100;
    kalman_io_data->R_gps->m[10][10] = 100;
    kalman_io_data->R_gps->m[11][11] = 100;

    kalman_io_data->P_gps->m[0][0] = 1;
    kalman_io_data->P_gps->m[1][1] = 1;
    kalman_io_data->P_gps->m[2][2] = 1;
    kalman_io_data->P_gps->m[3][3] = 1;
    kalman_io_data->P_gps->m[4][4] = 1;
    kalman_io_data->P_gps->m[5][5] = 1;

    /// Initilization
    retval = uquad_mat_zeros(kalman_io_data->x_hat);
    cleanup_if(retval);
    retval = uquad_mat_zeros(kalman_io_data->x_);
    cleanup_if(retval);

    /// Aux memory for inertial kalman
    retval = uquad_kalman_inertial_aux_mem_init();
    cleanup_if(retval);

    /// Aux memory for gps kalman
    retval = uquad_kalman_gps_aux_mem_init();
    cleanup_if(retval);

    // Aux memory for drive/drag
    retval = uquad_kalman_drive_drag_aux_mem_init();
    cleanup_if(retval);

    return kalman_io_data;
    cleanup:
    kalman_deinit(kalman_io_data);
    return NULL;
}

int uquad_kalman(kalman_io_t * kalman_io_data, uquad_mat_t* w, imu_data_t* data, double T, gps_comm_data_t *gps_i_data)
{
    int retval;
    uquad_bool_t is_gps = (gps_i_data != NULL);
    uquad_mat_t
	*Fk_1    = (is_gps)?Fk_1_gps:Fk_1_intertial,
	*Fk_1_T  = (is_gps)?Fk_1_T_gps:Fk_1_T_intertial,
	*mtmp    = (is_gps)?mtmp_gps:mtmp_intertial,
	*P_      = (is_gps)?P__gps:P__intertial,
	*H       = (is_gps)?H_gps:H_intertial,
	*hx      = (is_gps)?hx_gps:hx_intertial,
	*z       = (is_gps)?kalman_io_data->z_gps:kalman_io_data->z,
	*R       = (is_gps)?kalman_io_data->R_gps:kalman_io_data->R,
	*P       = kalman_io_data->P, /* ALWAYS */
	*Q       = kalman_io_data->Q, /* ALWAYS */
	// Auxiliares Update
	*yk      = (is_gps)?yk_gps:yk_intertial,
	*HT      = (is_gps)?HT_gps:HT_intertial,
	*HP_     = (is_gps)?HP__gps:HP__intertial,
	*HP_HT   = (is_gps)?HP_HT_gps:HP_HT_intertial,
	*Sk      = (is_gps)?Sk_gps:Sk_intertial,
	*Sk_aux1 = (is_gps)?Sk_aux1_gps:Sk_aux1_intertial,
	*Sk_aux2 = (is_gps)?Sk_aux2_gps:Sk_aux2_intertial,
	*P_HT    = (is_gps)?P_HT_gps:P_HT_intertial,
	*Kk      = (is_gps)?Kk_gps:Kk_intertial,
	*Kkyk    = (is_gps)?Kkyk_gps:Kkyk_intertial,
	*KkH     = (is_gps)?KkH_gps:KkH_intertial,
	*IKH     = (is_gps)?IKH_gps:IKH_intertial,
	*Sk_1    = (is_gps)?Sk_1_gps:Sk_1_intertial,
	*I       = (is_gps)?I_gps:I_intertial;

    retval = store_data(kalman_io_data, w, data, T, gps_i_data);
    err_propagate(retval);

    // Prediction
    retval = f(kalman_io_data -> x_, kalman_io_data);
    err_propagate(retval);
    retval = F(Fk_1, kalman_io_data);
    err_propagate(retval);
    retval = uquad_mat_transpose(Fk_1_T, Fk_1);
    err_propagate(retval);
    retval = uquad_mat_prod(mtmp, Fk_1, P);
    err_propagate(retval);
    retval = uquad_mat_prod(Fk_1,mtmp, Fk_1_T); // Aca lo vuelvo a guardar en Fk_1 para no hacer otra variable temporal
    err_propagate(retval);
    retval = uquad_mat_add(P_,Fk_1,Q);
    err_propagate(retval);

    // Update
    retval = h(hx, kalman_io_data, is_gps);
    err_propagate(retval);
    retval =  uquad_mat_sub(yk, z , hx);
    err_propagate(retval);
    retval = uquad_mat_prod(HP_,H,P_);
    err_propagate(retval);
    retval = uquad_mat_transpose(HT,H);
    err_propagate(retval);
    retval = uquad_mat_prod(HP_HT,HP_,HT);
    err_propagate(retval);
    retval = uquad_mat_add(Sk,HP_HT,R); // Sk
    err_propagate(retval);
    retval = uquad_mat_inv(Sk_1,Sk,Sk_aux1,Sk_aux2);
    err_propagate(retval);
    retval = uquad_mat_prod(P_HT,P_,HT);
    err_propagate(retval);
    retval = uquad_mat_prod(Kk,P_HT,Sk_1);
    err_propagate(retval);
    retval = uquad_mat_prod(Kkyk,Kk,yk);
    err_propagate(retval);
    retval = uquad_mat_add(kalman_io_data->x_hat, kalman_io_data->x_, Kkyk);
    err_propagate(retval);
    retval =  uquad_mat_eye(I);
    err_propagate(retval);
    retval = uquad_mat_prod(KkH, Kk, H);
    err_propagate(retval);
    retval = uquad_mat_sub(IKH,I,KkH);
    err_propagate(retval);
    retval = uquad_mat_prod(P, IKH, P_);
    err_propagate(retval);

    return ERROR_OK;
}

int f_gps(uquad_mat_t* fx, kalman_io_t* kalman_io_data)
{
    int retval;
    retval = f(fx,kalman_io_data);
    err_propagate(retval);
    /* fx->m_full[0]  = kalman_io_data -> x_hat -> m_full[0]; */
    /* fx->m_full[1]  = kalman_io_data -> x_hat -> m_full[1]; */
    /* fx->m_full[2]  = kalman_io_data -> x_hat -> m_full[2]; */
    /* fx->m_full[3]  = kalman_io_data -> x_hat -> m_full[6]; */
    /* fx->m_full[4]  = kalman_io_data -> x_hat -> m_full[7]; */
    /* fx->m_full[5]  = kalman_io_data -> x_hat -> m_full[8]; */
    return retval;
}

int h_gps(uquad_mat_t* hx, kalman_io_t* kalman_io_data)
{
    int retval;
    retval = h(hx,kalman_io_data,true);
    err_propagate(retval);
    return retval;
    /* if(Rv_gps==NULL) */
    /* { */
    /* 	Rv_gps     = uquad_mat_alloc(3,1); */
    /* 	v_gps      = uquad_mat_alloc(3,1); */
    /* 	R_gps      = uquad_mat_alloc(3,3); */
    /* } */
    /* v_gps -> m_full[0] = kalman_io_data -> x_hat -> m_full[6]; */
    /* v_gps -> m_full[1] = kalman_io_data -> x_hat -> m_full[7]; */
    /* v_gps -> m_full[2] = kalman_io_data -> x_hat -> m_full[8]; */
    /* retval = uquad_mat_rotate(false,Rv_gps, v_gps,  */
    /* 			      kalman_io_data -> x_hat -> m_full[3], */
    /* 			      kalman_io_data -> x_hat -> m_full[4], */
    /* 			      kalman_io_data -> x_hat -> m_full[5], */
    /* 			      R_gps); */
    /* err_propagate(retval); */

    /* hx->m_full[0]  = kalman_io_data -> x_hat -> m_full[0]; */
    /* hx->m_full[1]  = kalman_io_data -> x_hat -> m_full[1]; */
    /* hx->m_full[2]  = kalman_io_data -> x_hat -> m_full[2]; */
    /* hx->m_full[3]  = Rv_gps -> m_full[0]; */
    /* hx->m_full[4]  = Rv_gps -> m_full[1]; */
    /* hx->m_full[5]  = Rv_gps -> m_full[2]; */

    /* return ERROR_OK; */
}

/* int H_gps(uquad_mat_t* Hx, kalman_io_t* kalman_io_data) */
/* { */
/*     double psi   = kalman_io_data -> x_hat -> m_full[3]; */
/*     double phi   = kalman_io_data -> x_hat -> m_full[4]; */
/*     double theta = kalman_io_data -> x_hat -> m_full[5]; */
/*     Hx->m[0][0] = 1; */
/*     Hx->m[0][1] = 0; */
/*     Hx->m[0][2] = 0; */
/*     Hx->m[0][3] = 0; */
/*     Hx->m[0][4] = 0; */
/*     Hx->m[0][5] = 0; */
/*     Hx->m[1][0] = 0; */
/*     Hx->m[1][1] = 1; */
/*     Hx->m[1][2] = 0; */
/*     Hx->m[1][3] = 0; */
/*     Hx->m[1][4] = 0; */
/*     Hx->m[1][5] = 0; */
/*     Hx->m[2][0] = 0; */
/*     Hx->m[2][1] = 0; */
/*     Hx->m[2][2] = 1; */
/*     Hx->m[2][3] = 0; */
/*     Hx->m[2][4] = 0; */
/*     Hx->m[2][5] = 0; */
/*     Hx->m[3][0] = 0; */
/*     Hx->m[3][1] = 0; */
/*     Hx->m[3][2] = 0; */
/*     Hx->m[3][3] = cos(phi)*cos(theta); */
/*     Hx->m[3][4] = cos(theta)*sin(phi)*sin(psi) - cos(psi)*sin(theta); */
/*     Hx->m[3][5] = sin(psi)*sin(theta) + cos(psi)*cos(theta)*sin(phi); */
/*     Hx->m[4][0] = 0; */
/*     Hx->m[4][1] = 0; */
/*     Hx->m[4][2] = 0; */
/*     Hx->m[4][3] = cos(phi)*sin(theta); */
/*     Hx->m[4][4] = cos(psi)*cos(theta) + sin(phi)*sin(psi)*sin(theta); */
/*     Hx->m[4][5] = cos(psi)*sin(phi)*sin(theta) - cos(theta)*sin(psi); */
/*     Hx->m[5][0] = 0; */
/*     Hx->m[5][1] = 0; */
/*     Hx->m[5][2] = 0; */
/*     Hx->m[5][3] = -sin(phi); */
/*     Hx->m[5][4] = cos(phi)*sin(psi); */
/*     Hx->m[5][5] = cos(phi)*cos(psi); */
/*     return ERROR_OK; */
/* } */

int uquad_kalman_gps(kalman_io_t* kalman_io_data, gps_comm_data_t* gps_i_data)
{
    int retval;
    // Prediction
    retval = f(fx_gps, kalman_io_data);
    err_propagate(retval);
    retval = F(Fk_1_gps, kalman_io_data);
    err_propagate(retval);
    retval = uquad_mat_transpose(Fk_1_T_gps, Fk_1_gps);
    err_propagate(retval);
    retval = uquad_mat_prod(mtmp_gps, Fk_1_gps, kalman_io_data -> P_gps);
    err_propagate(retval);
    retval = uquad_mat_prod(Fk_1_gps,mtmp_gps, Fk_1_T_gps); // Aca lo vuelvo a guardar en Fk_1 para no hacer otra variable temporal
    err_propagate(retval);
    retval = uquad_mat_add(P__gps,Fk_1_gps,kalman_io_data->Q_gps);
    err_propagate(retval);

    // Update
    retval = h_gps(hx_gps, kalman_io_data);
    err_propagate(retval);

    z_gps -> m_full[0] = gps_i_data -> pos -> m_full[0];
    z_gps -> m_full[1] = gps_i_data -> pos -> m_full[1];
    z_gps -> m_full[2] = gps_i_data -> pos -> m_full[2];
    z_gps -> m_full[3] = gps_i_data -> vel -> m_full[0];
    z_gps -> m_full[4] = gps_i_data -> vel -> m_full[1];
    z_gps -> m_full[5] = gps_i_data -> vel -> m_full[2];

    retval =  uquad_mat_sub(yk_gps, z_gps, hx_gps);
    err_propagate(retval);
    retval = uquad_mat_prod(HP__gps,H_gps,P__gps);
    err_propagate(retval);
    retval = uquad_mat_transpose(HT_gps,H_gps);
    err_propagate(retval);
    retval = uquad_mat_prod(HP_HT_gps,HP__gps,HT_gps);
    err_propagate(retval);
    retval = uquad_mat_add(Sk_gps,HP_HT_gps,kalman_io_data -> R_gps);
    err_propagate(retval);
    retval = uquad_mat_inv(Sk_1_gps,Sk_gps,Sk_aux1_gps,Sk_aux2_gps);
    err_propagate(retval);
    retval = uquad_mat_prod(P_HT_gps,P__gps,HT_gps);
    err_propagate(retval);
    retval = uquad_mat_prod(Kk_gps,P_HT_gps,Sk_1_gps);
    err_propagate(retval);
    retval = uquad_mat_prod(Kkyk_gps,Kk_gps,yk_gps);
    err_propagate(retval);
    retval = uquad_mat_add(x_hat_gps, fx_gps, Kkyk_gps);
    err_propagate(retval);
    retval =  uquad_mat_eye(I_gps);
    err_propagate(retval);
    retval = uquad_mat_prod(KkH_gps, Kk_gps, H_gps);
    err_propagate(retval);
    retval = uquad_mat_sub(IKH_gps,I_gps,KkH_gps);
    err_propagate(retval);
    retval = uquad_mat_prod(kalman_io_data -> P_gps, IKH_gps, P__gps);
    err_propagate(retval);

    kalman_io_data -> x_hat -> m_full[0] = x_hat_gps -> m_full[0];
    kalman_io_data -> x_hat -> m_full[1] = x_hat_gps -> m_full[1];
    kalman_io_data -> x_hat -> m_full[2] = x_hat_gps -> m_full[2];
    kalman_io_data -> x_hat -> m_full[6] = x_hat_gps -> m_full[3];
    kalman_io_data -> x_hat -> m_full[7] = x_hat_gps -> m_full[4];
    kalman_io_data -> x_hat -> m_full[8] = x_hat_gps -> m_full[5];

    return ERROR_OK;
}

void kalman_deinit(kalman_io_t *kalman_io_data)
{
    /// Aux memory for inertial kalman
    uquad_kalman_inertial_aux_mem_deinit();

    /// Aux memory for gps kalman
    uquad_kalman_gps_aux_mem_deinit();

    // Aux memory for drive/drag
    uquad_kalman_drive_drag_aux_mem_deinit();

    // GPS

    // Auxiliaries for h
    uquad_mat_free(Rv_gps);
    uquad_mat_free(v_gps );
    uquad_mat_free(R_gps );

    // Auxiliaries for Update
    uquad_mat_free(fx_gps);
    uquad_mat_free(Fk_1_gps);
    uquad_mat_free(Fk_1_T_gps);
    uquad_mat_free(mtmp_gps);
    uquad_mat_free(P__gps);

    // Auxiliaries for Update
    uquad_mat_free(hx_gps);
    uquad_mat_free(yk_gps);
    uquad_mat_free(H_gps);
    uquad_mat_free(HP__gps);
    uquad_mat_free(HT_gps  );
    uquad_mat_free(HP_HT_gps);
    uquad_mat_free(Sk_gps  );
    uquad_mat_free(Sk_aux1_gps);
    uquad_mat_free(Sk_aux2_gps);
    uquad_mat_free(Sk_1_gps);
    uquad_mat_free(P_HT_gps);
    uquad_mat_free(Kk_gps  );
    uquad_mat_free(Kkyk_gps);
    uquad_mat_free(x_hat_gps);
    uquad_mat_free(I_gps);
    uquad_mat_free(KkH_gps);
    uquad_mat_free(IKH_gps);




    if(kalman_io_data != NULL)
    {
	uquad_mat_free(kalman_io_data->x_hat);
	uquad_mat_free(kalman_io_data->x_);
	uquad_mat_free(kalman_io_data->u);
	uquad_mat_free(kalman_io_data->z);
	uquad_mat_free(kalman_io_data->z_gps);
	uquad_mat_free(kalman_io_data->Q);
	uquad_mat_free(kalman_io_data->R);
	uquad_mat_free(kalman_io_data->P);
	uquad_mat_free(kalman_io_data->Q_gps);
	uquad_mat_free(kalman_io_data->R_gps);
	uquad_mat_free(kalman_io_data->P_gps);

	free(kalman_io_data);
    }
}
