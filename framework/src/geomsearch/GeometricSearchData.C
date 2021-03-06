//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "GeometricSearchData.h"
// Moose includes
#include "NearestNodeLocator.h"
#include "PenetrationLocator.h"
#include "ElementPairLocator.h"
#include "SubProblem.h"
#include "MooseMesh.h"
#include "Assembly.h"

static const unsigned int MORTAR_BASE_ID = 2e6;

GeometricSearchData::GeometricSearchData(SubProblem & subproblem, MooseMesh & mesh)
  : _subproblem(subproblem), _mesh(mesh), _first(true)
{
}

GeometricSearchData::~GeometricSearchData()
{
  for (auto & it : _penetration_locators)
    delete it.second;

  for (auto & it : _nearest_node_locators)
    delete it.second;
}

void
GeometricSearchData::update(GeometricSearchType type)
{
  if (type == ALL || type == QUADRATURE || type == NEAREST_NODE)
  {
    if (_first) // Only do this once
    {
      _first = false;

      for (const auto & it : _slave_to_qslave)
        generateQuadratureNodes(it.first, it.second);

      // reinit on displaced mesh before update
      for (const auto & epl_it : _element_pair_locators)
      {
        ElementPairLocator & epl = *(epl_it.second);
        epl.reinit();
      }
    }

    // Update the position of quadrature nodes first
    for (const auto & qbnd : _quadrature_boundaries)
      updateQuadratureNodes(qbnd);
  }

  if (type == ALL || type == MORTAR)
    if (_mortar_boundaries.size() > 0)
      updateMortarNodes();

  if (type == ALL || type == NEAREST_NODE)
  {
    for (const auto & nnl_it : _nearest_node_locators)
    {
      NearestNodeLocator * nnl = nnl_it.second;
      nnl->findNodes();
    }
  }

  if (type == ALL || type == PENETRATION)
  {
    for (const auto & pl_it : _penetration_locators)
    {
      PenetrationLocator * pl = pl_it.second;
      pl->detectPenetration();
    }
  }

  if (type == ALL || type == PENETRATION)
  {
    for (auto & elem_pair_locator_pair : _element_pair_locators)
    {
      ElementPairLocator & epl = (*elem_pair_locator_pair.second);
      epl.update();
    }
  }
}

void
GeometricSearchData::reinit()
{
  _mesh.clearQuadratureNodes();
  // Update the position of quadrature nodes first
  for (const auto & qbnd : _quadrature_boundaries)
    reinitQuadratureNodes(qbnd);
  reinitMortarNodes();

  for (const auto & nnl_it : _nearest_node_locators)
  {
    NearestNodeLocator * nnl = nnl_it.second;
    nnl->reinit();
  }

  for (const auto & pl_it : _penetration_locators)
  {
    PenetrationLocator * pl = pl_it.second;
    pl->reinit();
  }

  for (const auto & epl_it : _element_pair_locators)
  {
    ElementPairLocator & epl = *(epl_it.second);
    epl.reinit();
  }
}

void
GeometricSearchData::clearNearestNodeLocators()
{
  for (const auto & nnl_it : _nearest_node_locators)
  {
    NearestNodeLocator * nnl = nnl_it.second;
    nnl->reinit();
  }
}

Real
GeometricSearchData::maxPatchPercentage()
{
  Real max = 0.0;

  for (const auto & nnl_it : _nearest_node_locators)
  {
    NearestNodeLocator * nnl = nnl_it.second;

    if (nnl->_max_patch_percentage > max)
      max = nnl->_max_patch_percentage;
  }

  return max;
}

PenetrationLocator &
GeometricSearchData::getPenetrationLocator(const BoundaryName & master,
                                           const BoundaryName & slave,
                                           Order order)
{
  unsigned int master_id = _mesh.getBoundaryID(master);
  unsigned int slave_id = _mesh.getBoundaryID(slave);

  _subproblem.addGhostedBoundary(master_id);
  _subproblem.addGhostedBoundary(slave_id);

  PenetrationLocator * pl =
      _penetration_locators[std::pair<unsigned int, unsigned int>(master_id, slave_id)];

  if (!pl)
  {
    pl = new PenetrationLocator(_subproblem,
                                *this,
                                _mesh,
                                master_id,
                                slave_id,
                                order,
                                getNearestNodeLocator(master_id, slave_id));
    _penetration_locators[std::pair<unsigned int, unsigned int>(master_id, slave_id)] = pl;
  }

  return *pl;
}

PenetrationLocator &
GeometricSearchData::getQuadraturePenetrationLocator(const BoundaryName & master,
                                                     const BoundaryName & slave,
                                                     Order order)
{
  unsigned int master_id = _mesh.getBoundaryID(master);
  unsigned int slave_id = _mesh.getBoundaryID(slave);

  _subproblem.addGhostedBoundary(master_id);
  _subproblem.addGhostedBoundary(slave_id);

  // Generate a new boundary id
  // TODO: Make this better!
  unsigned int base_id = 1e6;
  unsigned int qslave_id = slave_id + base_id;

  _slave_to_qslave[slave_id] = qslave_id;

  PenetrationLocator * pl =
      _penetration_locators[std::pair<unsigned int, unsigned int>(master_id, qslave_id)];

  if (!pl)
  {
    pl = new PenetrationLocator(_subproblem,
                                *this,
                                _mesh,
                                master_id,
                                qslave_id,
                                order,
                                getQuadratureNearestNodeLocator(master_id, slave_id));
    _penetration_locators[std::pair<unsigned int, unsigned int>(master_id, qslave_id)] = pl;
  }

  return *pl;
}

PenetrationLocator &
GeometricSearchData::getMortarPenetrationLocator(const BoundaryName & master,
                                                 const BoundaryName & slave,
                                                 Moose::ConstraintType side_type,
                                                 Order order)
{
  unsigned int master_id = _mesh.getBoundaryID(master);
  unsigned int slave_id = _mesh.getBoundaryID(slave);

  // Generate a new boundary id
  // TODO: Make this better!
  unsigned int mortar_boundary_id, boundary_id;
  switch (side_type)
  {
    case Moose::Master:
      boundary_id = master_id;
      mortar_boundary_id = MORTAR_BASE_ID + slave_id;
      _boundary_to_mortarboundary[slave_id] = mortar_boundary_id;
      break;

    case Moose::Slave:
      boundary_id = slave_id;
      mortar_boundary_id = MORTAR_BASE_ID + master_id;
      _boundary_to_mortarboundary[master_id] = mortar_boundary_id;
      break;
  }

  PenetrationLocator * pl =
      _penetration_locators[std::pair<unsigned int, unsigned int>(boundary_id, mortar_boundary_id)];
  if (!pl)
  {
    pl = new PenetrationLocator(_subproblem,
                                *this,
                                _mesh,
                                boundary_id,
                                mortar_boundary_id,
                                order,
                                getMortarNearestNodeLocator(master_id, slave_id, side_type));
    _penetration_locators[std::pair<unsigned int, unsigned int>(boundary_id, mortar_boundary_id)] =
        pl;
  }

  return *pl;
}

NearestNodeLocator &
GeometricSearchData::getNearestNodeLocator(const BoundaryName & master, const BoundaryName & slave)
{
  unsigned int master_id = _mesh.getBoundaryID(master);
  unsigned int slave_id = _mesh.getBoundaryID(slave);

  _subproblem.addGhostedBoundary(master_id);
  _subproblem.addGhostedBoundary(slave_id);

  return getNearestNodeLocator(master_id, slave_id);
}

NearestNodeLocator &
GeometricSearchData::getNearestNodeLocator(const unsigned int master_id,
                                           const unsigned int slave_id)
{
  NearestNodeLocator * nnl =
      _nearest_node_locators[std::pair<unsigned int, unsigned int>(master_id, slave_id)];

  _subproblem.addGhostedBoundary(master_id);
  _subproblem.addGhostedBoundary(slave_id);

  if (!nnl)
  {
    nnl = new NearestNodeLocator(_subproblem, _mesh, master_id, slave_id);
    _nearest_node_locators[std::pair<unsigned int, unsigned int>(master_id, slave_id)] = nnl;
  }

  return *nnl;
}

NearestNodeLocator &
GeometricSearchData::getQuadratureNearestNodeLocator(const BoundaryName & master,
                                                     const BoundaryName & slave)
{
  unsigned int master_id = _mesh.getBoundaryID(master);
  unsigned int slave_id = _mesh.getBoundaryID(slave);

  _subproblem.addGhostedBoundary(master_id);
  _subproblem.addGhostedBoundary(slave_id);

  return getQuadratureNearestNodeLocator(master_id, slave_id);
}

NearestNodeLocator &
GeometricSearchData::getQuadratureNearestNodeLocator(const unsigned int master_id,
                                                     const unsigned int slave_id)
{
  // TODO: Make this better!
  unsigned int base_id = 1e6;
  unsigned int qslave_id = slave_id + base_id;

  _slave_to_qslave[slave_id] = qslave_id;

  return getNearestNodeLocator(master_id, qslave_id);
}

void
GeometricSearchData::generateQuadratureNodes(unsigned int slave_id, unsigned int qslave_id)
{
  // Have we already generated quadrature nodes for this boundary id?
  if (_quadrature_boundaries.find(slave_id) != _quadrature_boundaries.end())
    return;

  _quadrature_boundaries.insert(slave_id);

  const MooseArray<Point> & points_face = _subproblem.assembly(0).qPointsFace();

  ConstBndElemRange & range = *_mesh.getBoundaryElementRange();
  for (const auto & belem : range)
  {
    const Elem * elem = belem->_elem;
    unsigned short int side = belem->_side;
    BoundaryID boundary_id = belem->_bnd_id;

    if (elem->processor_id() == _subproblem.processor_id())
    {
      if (boundary_id == (BoundaryID)slave_id)
      {
        _subproblem.setCurrentSubdomainID(elem, 0);
        _subproblem.prepare(elem, 0);
        _subproblem.reinitElemFace(elem, side, boundary_id, 0);

        for (unsigned int qp = 0; qp < points_face.size(); qp++)
          _mesh.addQuadratureNode(elem, side, qp, qslave_id, points_face[qp]);
      }
    }
  }
}

NearestNodeLocator &
GeometricSearchData::getMortarNearestNodeLocator(const BoundaryName & master,
                                                 const BoundaryName & slave,
                                                 Moose::ConstraintType side_type)
{
  unsigned int master_id = _mesh.getBoundaryID(master);
  unsigned int slave_id = _mesh.getBoundaryID(slave);

  return getMortarNearestNodeLocator(master_id, slave_id, side_type);
}

NearestNodeLocator &
GeometricSearchData::getMortarNearestNodeLocator(const unsigned int master_id,
                                                 const unsigned int slave_id,
                                                 Moose::ConstraintType side_type)
{
  unsigned int mortarboundary_id, boundary;

  switch (side_type)
  {
    case Moose::Master:
      boundary = master_id;
      mortarboundary_id = MORTAR_BASE_ID + slave_id;
      _boundary_to_mortarboundary[slave_id] = mortarboundary_id;
      break;

    case Moose::Slave:
      boundary = slave_id;
      mortarboundary_id = MORTAR_BASE_ID + master_id;
      _boundary_to_mortarboundary[master_id] = mortarboundary_id;
      break;

    default:
      mooseError("Unknown side type");
  }

  generateMortarNodes(master_id, slave_id, 1001);

  return getNearestNodeLocator(boundary, 1001);
}

void
GeometricSearchData::addElementPairLocator(const unsigned int & interface_id,
                                           std::shared_ptr<ElementPairLocator> epl)
{
  _element_pair_locators[interface_id] = epl;
}

void
GeometricSearchData::generateMortarNodes(unsigned int master_id,
                                         unsigned int slave_id,
                                         unsigned int qslave_id)
{
  // Have we already generated quadrature nodes for this boundary id?
  if (_mortar_boundaries.find(std::pair<unsigned int, unsigned int>(master_id, slave_id)) !=
      _mortar_boundaries.end())
    return;

  _mortar_boundaries.insert(std::pair<unsigned int, unsigned int>(master_id, slave_id));

  MooseMesh::MortarInterface * iface = _mesh.getMortarInterface(master_id, slave_id);

  const MooseArray<Point> & qpoints = _subproblem.assembly(0).qPoints();
  for (const auto & elem : iface->_elems)
  {
    _subproblem.setCurrentSubdomainID(elem, 0);
    _subproblem.assembly(0).reinit(elem);

    for (unsigned int qp = 0; qp < qpoints.size(); qp++)
      _mesh.addQuadratureNode(elem, 0, qp, qslave_id, qpoints[qp]);
  }
}

void
GeometricSearchData::updateQuadratureNodes(unsigned int slave_id)
{
  const MooseArray<Point> & points_face = _subproblem.assembly(0).qPointsFace();

  ConstBndElemRange & range = *_mesh.getBoundaryElementRange();
  for (const auto & belem : range)
  {
    const Elem * elem = belem->_elem;
    unsigned short int side = belem->_side;
    BoundaryID boundary_id = belem->_bnd_id;

    if (elem->processor_id() == _subproblem.processor_id())
    {
      if (boundary_id == (BoundaryID)slave_id)
      {
        _subproblem.setCurrentSubdomainID(elem, 0);
        _subproblem.prepare(elem, 0);
        _subproblem.reinitElemFace(elem, side, boundary_id, 0);

        for (unsigned int qp = 0; qp < points_face.size(); qp++)
          (*_mesh.getQuadratureNode(elem, side, qp)) = points_face[qp];
      }
    }
  }
}

void
GeometricSearchData::reinitQuadratureNodes(unsigned int /*slave_id*/)
{
  // Regenerate the quadrature nodes
  for (const auto & it : _slave_to_qslave)
    generateQuadratureNodes(it.first, it.second);
}

void
GeometricSearchData::updateMortarNodes()
{
  const MooseArray<Point> & qpoints = _subproblem.assembly(0).qPoints();

  auto & ifaces = _mesh.getMortarInterfaces();
  for (const auto & iface : ifaces)
    for (const auto & elem : iface->_elems)
    {
      _subproblem.setCurrentSubdomainID(elem, 0);
      _subproblem.assembly(0).reinit(elem);

      for (unsigned int qp = 0; qp < qpoints.size(); qp++)
        (*_mesh.getQuadratureNode(elem, 0, qp)) = qpoints[qp];
    }
}

void
GeometricSearchData::reinitMortarNodes()
{
  _mortar_boundaries.clear();
  // Regenerate the quadrature nodes for mortar spaces
  auto & ifaces = _mesh.getMortarInterfaces();
  for (const auto & iface : ifaces)
  {
    unsigned int master_id = _mesh.getBoundaryID(iface->_master);
    unsigned int slave_id = _mesh.getBoundaryID(iface->_slave);
    generateMortarNodes(master_id, slave_id, 0);
  }
}

void
GeometricSearchData::updateGhostedElems()
{
  for (const auto & nnl_it : _nearest_node_locators)
  {
    NearestNodeLocator * nnl = nnl_it.second;
    nnl->updateGhostedElems();
  }
}
