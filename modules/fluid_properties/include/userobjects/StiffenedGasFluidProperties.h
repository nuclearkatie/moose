//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#ifndef STIFFENEDGASFLUIDPROPERTIES_H
#define STIFFENEDGASFLUIDPROPERTIES_H

#include "SinglePhaseFluidProperties.h"

class StiffenedGasFluidProperties;

template <>
InputParameters validParams<StiffenedGasFluidProperties>();

/**
 * Stiffened gas fluid properties
 */
class StiffenedGasFluidProperties : public SinglePhaseFluidProperties
{
public:
  StiffenedGasFluidProperties(const InputParameters & parameters);
  virtual ~StiffenedGasFluidProperties();

  virtual Real p_from_v_e(Real v, Real e) const override;
  virtual void p_from_v_e(Real v, Real e, Real & p, Real & dp_dv, Real & dp_de) const override;
  virtual Real T_from_v_e(Real v, Real e) const override;
  virtual void T_from_v_e(Real v, Real e, Real & T, Real & dT_dv, Real & dT_de) const override;
  virtual Real c_from_v_e(Real v, Real e) const override;
  virtual void c_from_v_e(Real v, Real e, Real & c, Real & dc_dv, Real & dc_de) const override;
  virtual Real cp_from_v_e(Real v, Real e) const override;
  virtual Real cv_from_v_e(Real v, Real e) const override;
  virtual Real gamma_from_v_e(Real v, Real e) const override;
  virtual Real mu_from_v_e(Real v, Real e) const override;
  virtual Real k_from_v_e(Real v, Real e) const override;
  virtual Real s_from_v_e(Real v, Real e) const override;
  virtual void s_from_v_e(Real v, Real e, Real & s, Real & ds_dv, Real & ds_de) const override;
  virtual Real s_from_h_p(Real h, Real p) const override;
  virtual void s_from_h_p(Real h, Real p, Real & s, Real & ds_dh, Real & ds_dp) const override;
  virtual Real rho_from_p_s(Real p, Real s) const override;
  virtual void
  rho_from_p_s(Real p, Real s, Real & rho, Real & drho_dp, Real & drho_ds) const override;
  virtual Real rho_from_p_T(Real p, Real T) const override;
  virtual void
  rho_from_p_T(Real p, Real T, Real & rho, Real & drho_dp, Real & drho_dT) const override;
  virtual Real e_from_p_rho(Real p, Real rho) const override;
  virtual void
  e_from_p_rho(Real p, Real rho, Real & e, Real & de_dp, Real & de_drho) const override;
  virtual Real e_from_v_h(Real v, Real h) const override;
  virtual void e_from_v_h(Real v, Real h, Real & e, Real & de_dv, Real & de_dh) const override;
  virtual Real h_from_p_T(Real p, Real T) const override;
  virtual void h_from_p_T(Real p, Real T, Real & h, Real & dh_dp, Real & dh_dT) const override;
  virtual Real p_from_h_s(Real h, Real s) const override;
  virtual void p_from_h_s(Real h, Real s, Real & p, Real & dp_dh, Real & dp_ds) const override;
  virtual Real g_from_v_e(Real v, Real e) const override;
  virtual Real beta_from_p_T(Real p, Real T) const override;

  virtual Real c2_from_p_rho(Real pressure, Real rho) const;

protected:
  Real _gamma;
  Real _cv;
  Real _q;
  Real _q_prime;
  Real _p_inf;
  Real _cp;

  Real _mu;
  Real _k;
};

#endif /* STIFFENEDGASFLUIDPROPERTIES_H */
