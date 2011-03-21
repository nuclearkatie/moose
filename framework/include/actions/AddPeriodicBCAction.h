#ifndef ADDPERIODICBCACTION_H
#define ADDPERIODICBCACTION_H

#include "InputParameters.h"
#include "Moose.h"
#include "Action.h"

#include <string>

// libMesh includes
#include "periodic_boundaries.h"

/**
 * This Action adds a periodic boundary to the problem. Note that Periodic Boundaries
 * are not MooseObjects so you need not specify a type for these boundaries.  If you
 * do, it will currently be ignored by this Action.
 */
class AddPeriodicBCAction : public Action
{
public:
  AddPeriodicBCAction(const std::string & name, InputParameters params);

  virtual void act();

protected:
  void setPeriodicVars(PeriodicBoundary & p, const std::vector<std::string> & var_names);
};

template<>
InputParameters validParams<AddPeriodicBCAction>();

#endif // ADDPERIODICBCACTION_H
